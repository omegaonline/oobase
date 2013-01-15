///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Posix.h"
#include "../include/OOBase/Queue.h"
#include "../include/OOBase/StackAllocator.h"

#if defined(HAVE_UNISTD_H) && !defined(_WIN32)

#include "ProactorPosix.h"
#include "BSDSocket.h"

#include <sys/stat.h>
#include <string.h>

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

namespace
{
	class AsyncSocket : public OOBase::AsyncSocket
	{
	public:
		AsyncSocket(OOBase::detail::ProactorPosix* pProactor, int fd);
		virtual ~AsyncSocket();

		int init();

		typedef void (*recv_callback_t)(void* param, OOBase::Buffer* buffer, int err);
		typedef void (*send_callback_t)(void* param, int err);

		int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes);
		int send(void* param, send_callback_t callback, OOBase::Buffer* buffer);
		int send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count);

	private:
		struct RecvItem
		{
			void*           m_param;
			recv_callback_t m_callback;
			OOBase::Buffer* m_buffer;
			size_t          m_bytes;
		};

		struct RecvNotify
		{
			int             m_err;
			void*           m_param;
			recv_callback_t m_callback;
			OOBase::Buffer* m_buffer;
		};

		struct SendItem
		{
			void*           m_param;
			send_callback_t m_callback;
			size_t          m_count;
			union
			{
				OOBase::Buffer*  m_buffer;
				OOBase::Buffer** m_buffers;
			};
		};

		struct SendNotify
		{
			int             m_err;
			void*           m_param;
			send_callback_t m_callback;
		};

		OOBase::detail::ProactorPosix* m_pProactor;
		int                            m_fd;
		OOBase::SpinLock               m_lock;
		OOBase::Queue<RecvItem>        m_recv_queue;
		OOBase::Queue<SendItem>        m_send_queue;

		static void fd_callback(int fd, void* param, unsigned int events);
		void process_recv(OOBase::Queue<RecvNotify,OOBase::AllocatorInstance>& notify_queue);
		int process_recv_i(RecvItem* item, bool& watch_again);
		void process_send(OOBase::Queue<SendNotify,OOBase::AllocatorInstance>& notify_queue);
		int process_send_i(SendItem* item, bool& watch_again);
		int process_send_v(SendItem* item, bool& watch_again);
	};
}

AsyncSocket::AsyncSocket(OOBase::detail::ProactorPosix* pProactor, int fd) :
		m_pProactor(pProactor),
		m_fd(fd)
{ }

AsyncSocket::~AsyncSocket()
{
	m_pProactor->unbind_fd(m_fd);

	OOBase::Net::close_socket(m_fd);

	// Free all items
	RecvItem recv_item;
	while (m_recv_queue.pop(&recv_item))
	{
		if (recv_item.m_buffer)
			recv_item.m_buffer->release();
	}

	SendItem send_item;
	while (m_send_queue.pop(&send_item))
	{
		if (send_item.m_count == 1)
		{
			if (send_item.m_buffer)
				send_item.m_buffer->release();
		}
		else if (send_item.m_buffers)
		{
			for (size_t i = 0; i < send_item.m_count; ++i)
			{
				if (send_item.m_buffers[i])
					send_item.m_buffers[i]->release();
			}

			OOBase::CrtAllocator::free(send_item.m_buffers);
		}
	}
}

int AsyncSocket::init()
{
	return m_pProactor->bind_fd(m_fd,this,&fd_callback);
}

int AsyncSocket::recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
{
	if (!buffer)
		return EINVAL;

	int err = 0;
	if (bytes)
	{
		err = buffer->space(bytes);
		if (err)
			return err;
	}
	else if (!buffer->space())
		return 0;

	if (!callback)
		return EINVAL;

	RecvItem item = { param, callback, buffer, bytes};
	item.m_buffer->addref();

	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	bool watch = m_recv_queue.empty();
	int err = m_recv_queue.push(item);

	guard.release();

	if (err)
	{
		item.m_buffer->release();
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXRecv) : 0);
}

int AsyncSocket::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	size_t bytes = (buffer ? buffer->length() : 0);
	if (bytes == 0)
		return 0;

	if (!buffer)
		return EINVAL;

	SendItem item = { param, callback, 1 };
	item.m_buffer = buffer;
	item.m_buffer->addref();

	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	bool watch = m_send_queue.empty();
	int err = m_send_queue.push(item);

	guard.release();

	if (err)
	{
		item.m_buffer->release();
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXSend) : 0);
}

int AsyncSocket::send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return EINVAL;

	// Count how many actual buffers we have
	size_t actual_count = 0;
	OOBase::Buffer* single = NULL;
	for (size_t i = 0; i < count; ++i)
	{
		if (buffers[i] && buffers[i]->length() > 0)
		{
			if (!single)
				single = buffers[i];

			++actual_count;
		}
	}

	if (actual_count == 0)
		return 0;

	if (actual_count == 1 && single)
		return send(param,callback,single);

	SendItem item = { param, callback, actual_count };
	item.m_buffers = static_cast<OOBase::Buffer**>(OOBase::CrtAllocator::allocate(actual_count * sizeof(OOBase::Buffer*)));
	if (!item.m_buffers)
		return ERROR_OUTOFMEMORY;

	size_t idx = 0;
	for (size_t i = 0; i < count; ++i)
	{
		if (buffers[i] && buffers[i]->length() > 0)
		{
			item.m_buffers[idx] = buffers[i];
			item.m_buffers[idx]->addref();
			++idx;
		}
	}

	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	bool watch = m_send_queue.empty();
	int err = m_send_queue.push(item);

	guard.release();

	if (err)
	{
		for (size_t i=0;i<actual_count;++i)
			item.m_buffers[i]->release();

		OOBase::CrtAllocator::free(item.m_buffers);
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXSend) : 0);
}

void AsyncSocket::fd_callback(int fd, void* param, unsigned int events)
{
	AsyncSocket* pThis = static_cast<AsyncSocket*>(param);
	if (pThis->m_fd == fd)
	{
		OOBase::StackAllocator<512> allocator;
		OOBase::Queue<RecvNotify,OOBase::AllocatorInstance> recv_notify_queue(allocator);
		OOBase::Queue<SendNotify,OOBase::AllocatorInstance> send_notify_queue(allocator);

		OOBase::Guard<OOBase::SpinLock> guard(pThis->m_lock);

		if (events & OOBase::detail::eTXRecv)
			pThis->process_recv(recv_notify_queue);

		if (events & OOBase::detail::eTXSend)
			pThis->process_send(send_notify_queue);

		guard.release();

		RecvNotify recv_notify;
		while (recv_notify_queue.pop(&recv_notify))
		{
			(*recv_notify.m_callback)(recv_notify.m_param,recv_notify.m_buffer,recv_notify.m_err);
			recv_notify.m_buffer->release();
		}

		SendNotify send_notify;
		while (send_notify_queue.pop(&send_notify))
			(*send_notify.m_callback)(send_notify.m_param,send_notify.m_err);
	}
}

int AsyncSocket::process_recv_i(RecvItem* item, bool& watch_again)
{
	// We read again on EOF, as we only return error codes
	int err = 0;
	for (;;)
	{
		size_t to_read = (item->m_bytes ? item->m_bytes : item->m_buffer->space());
		ssize_t r = 0;
		do
		{
			r = ::recv(m_fd,item->m_buffer->wr_ptr(),to_read,0);
		}
		while (r == -1 && errno == EINTR);

		if (r == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				watch_again = true;
			else
				err = errno;
			break;
		}

		if (r == 0)
			break;

		err = item->m_buffer->wr_ptr(r);
		if (err)
			break;

		if (item->m_bytes)
			item->m_bytes -= r;

		if (item->m_bytes == 0)
			break;
	}

	return err;
}

void AsyncSocket::process_recv(OOBase::Queue<RecvNotify,OOBase::AllocatorInstance>& notify_queue)
{
	int err = 0;
	while (!m_recv_queue.empty())
	{
		if (!err)
		{
			RecvItem* front = m_recv_queue.front();

			bool watch_again = false;
			err = process_recv_i(front,watch_again);

			if (!err && watch_again)
			{
				// Watch for eTXRecv again
				err = m_pProactor->watch_fd(m_fd,OOBase::detail::eTXRecv);
				if (!err)
					break;
			}
		}

		// By the time we get here, we have a complete recv or an error
		RecvItem item;
		m_recv_queue.pop(&item);

		if (!item.m_callback)
			item.m_buffer->release();
		else
		{
			RecvNotify notify = { err, item.m_param, item.m_callback, item.m_buffer };
			err = notify_queue.push(notify);
			if (err)
			{
				item.m_buffer->release();
				OOBase_CallCriticalFailure(err);
			}
		}
	}
}

int AsyncSocket::process_send_i(SendItem* item, bool& watch_again)
{
	// Single send
	int err = 0;
	for (;;)
	{
		ssize_t sent = 0;
		do
		{
			sent = ::send(m_fd,item->m_buffer->rd_ptr(),item->m_buffer->length(),MSG_NOSIGNAL);
		}
		while (sent == -1 && errno == EINTR);

		if (sent == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				watch_again = true;
			else
				err = errno;
			break;
		}

		item->m_buffer->rd_ptr(sent);

		if (item->m_buffer->length() == 0)
			break;
	}

	if (!watch_again)
	{
		// Done with the buffer
		item->m_buffer->release();
		item->m_buffer = NULL;
	}

	return err;
}

int AsyncSocket::process_send_v(SendItem* item, bool& watch_again)
{
	// Sendv

	// Find first valid buffer
	size_t first_buffer = 0;
	for (size_t i = 0;i<item->m_count;++i)
	{
		if (item->m_buffers[i])
			break;

		++first_buffer;
	}

	int err = 0;
	if (first_buffer < item->m_count)
	{
		OOBase::StackArrayPtr<struct iovec,8> iovecs(item->m_count - first_buffer);
		if (!iovecs)
			err = ERROR_OUTOFMEMORY;
		else
		{
			struct msghdr msg = {0};
			msg.msg_iov = iovecs;
			msg.msg_iovlen = item->m_count - first_buffer;

			for (size_t i=0;i<msg.msg_iovlen;++i)
			{
				msg.msg_iov[i].iov_len = item->m_buffers[i+first_buffer]->length();
				msg.msg_iov[i].iov_base = const_cast<char*>(item->m_buffers[i+first_buffer]->rd_ptr());
			}

			while (msg.msg_iovlen)
			{
				ssize_t sent = 0;
				do
				{
					sent = ::sendmsg(m_fd,&msg,MSG_NOSIGNAL);
				}
				while (sent == -1 && errno == EINTR);

				if (sent == -1)
				{
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						watch_again = true;
					else
						err = errno;
					break;
				}
				else
				{
					// Update buffers...
					while (sent > 0)
					{
						if (static_cast<size_t>(sent) >= msg.msg_iov->iov_len)
						{
							item->m_buffers[first_buffer]->rd_ptr(msg.msg_iov->iov_len);
							item->m_buffers[first_buffer]->release();
							item->m_buffers[first_buffer] = NULL;
							++first_buffer;

							sent -= msg.msg_iov->iov_len;
							++msg.msg_iov;
							--msg.msg_iovlen;
						}
						else
						{
							item->m_buffers[first_buffer]->rd_ptr(sent);
							msg.msg_iov->iov_len -= sent;
							msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + sent;
							sent = 0;
						}
					}
				}
			}
		}
	}

	if (!watch_again)
	{
		for (size_t i = 0;i<item->m_count;++i)
		{
			if (item->m_buffers[i])
				item->m_buffers[i]->release();
		}

		OOBase::CrtAllocator::free(item->m_buffers);
		item->m_buffers = NULL;
	}

	return err;
}

void AsyncSocket::process_send(OOBase::Queue<SendNotify,OOBase::AllocatorInstance>& notify_queue)
{
	int err = 0;
	while (!m_send_queue.empty())
	{
		if (!err)
		{
			SendItem* front = m_send_queue.front();

			bool watch_again = false;
			if (front->m_count == 1)
				err = process_send_i(front,watch_again);
			else
				err = process_send_v(front,watch_again);

			if (!err && watch_again)
			{
				// Watch for eTXRecv again
				err = m_pProactor->watch_fd(m_fd,OOBase::detail::eTXSend);
				if (!err)
					break;
			}
		}

		// By the time we get here, we have a complete send or an error
		SendItem item;
		m_send_queue.pop(&item);

		if (item.m_callback)
		{
			SendNotify notify = { err, item.m_param, item.m_callback };
			err = notify_queue.push(notify);
			if (err)
				OOBase_CallCriticalFailure(err);
		}
	}
}

namespace
{
	class SocketAcceptor : public OOBase::Acceptor
	{
	public:
		SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_callback_t callback);
		SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_pipe_callback_t callback);
		virtual ~SocketAcceptor();

		int bind(const sockaddr* addr, socklen_t addr_len, mode_t mode);

	private:
		OOBase::detail::ProactorPosix*           m_pProactor;
		void*                                    m_param;
		OOBase::Proactor::accept_callback_t      m_callback;
		OOBase::Proactor::accept_pipe_callback_t m_callback_local;
		int                                      m_fd;

		static void fd_callback(int fd, void* param, unsigned int events);
		void do_accept();
	};
}

SocketAcceptor::SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_callback_t callback) :
		m_pProactor(pProactor),
		m_param(param),
		m_callback(callback),
		m_callback_local(NULL),
		m_fd(-1)
{ }

SocketAcceptor::SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_pipe_callback_t callback) :
		m_pProactor(pProactor),
		m_param(param),
		m_callback(NULL),
		m_callback_local(callback),
		m_fd(-1)
{ }

SocketAcceptor::~SocketAcceptor()
{
	if (m_fd != -1)
	{
		m_pProactor->unbind_fd(m_fd);
		OOBase::Net::close_socket(m_fd);
	}
}

int SocketAcceptor::bind(const sockaddr* addr, socklen_t addr_len, mode_t mode)
{
	// Create a new socket
	int err = 0;
	int fd = OOBase::Net::open_socket(addr->sa_family,SOCK_STREAM,0,err);
	if (err)
		return err;

	// Apparently, chmod before bind()

	// Bind to the address
	if (::fchmod(fd,mode) != 0 || ::bind(fd,addr,addr_len) != 0 || ::listen(fd,SOMAXCONN) != 0)
		err = errno;
	else
	{
		err = m_pProactor->bind_fd(fd,this,&fd_callback);
		if (!err)
		{
			m_fd = fd;
			err = m_pProactor->watch_fd(fd,OOBase::detail::eTXRecv);
			if (err)
			{
				m_pProactor->unbind_fd(m_fd);
				m_fd = -1;
			}
		}
	}

	if (err)
		OOBase::Net::close_socket(fd);

	return err;
}

void SocketAcceptor::fd_callback(int fd, void* param, unsigned int events)
{
	SocketAcceptor* pThis = static_cast<SocketAcceptor*>(param);
	if (pThis->m_fd == fd && (events & OOBase::detail::eTXRecv))
		pThis->do_accept();
}

void SocketAcceptor::do_accept()
{
	for (;;)
	{
		sockaddr_storage addr = {0};
		socklen_t addr_len = 0;

		int new_fd = -1;
		do
		{
			addr_len = sizeof(addr);

#if defined(_GNU_SOURCE)
			new_fd = ::accept4(m_fd,(sockaddr*)&addr,&addr_len,SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
			new_fd = ::accept(m_fd,(sockaddr*)&addr,&addr_len);
#endif
		}
		while (new_fd == -1 && errno == EINTR);

		::AsyncSocket* pSocket = NULL;
		int err = 0;

		if (new_fd == -1)
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Will complete later...
				err = m_pProactor->watch_fd(m_fd,OOBase::detail::eTXRecv);
				if (!err)
					break;
			}
		}
		else
		{
#if !defined(_GNU_SOURCE)
			err = OOBase::POSIX::set_close_on_exec(new_fd,true);
			if (err == 0)
				err = OOBase::POSIX::set_non_blocking(new_fd,true);
#endif
			if (err == 0)
			{
				// Wrap the handle
				pSocket = new (std::nothrow) ::AsyncSocket(m_pProactor,new_fd);
				if (!pSocket)
					err = ENOMEM;
				else
				{
					err = pSocket->init();
					if (err != 0)
					{
						delete pSocket;
						pSocket = NULL;
					}
				}
			}

			if (err && !pSocket)
				OOBase::Net::close_socket(new_fd);
		}

		if (m_callback_local)
			(*m_callback_local)(m_param,pSocket,err);
		else
			(*m_callback)(m_param,pSocket,(sockaddr*)&addr,addr_len,err);

		if (new_fd == -1)
		{
			// accept() failed, don't loop
			break;
		}
	}
}

OOBase::Acceptor* OOBase::detail::ProactorPosix::accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err)
{
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		err = EINVAL;
		return NULL;
	}

	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		err = pAcceptor->bind(addr,addr_len,0666);
		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}

	return pAcceptor;
}

OOBase::Acceptor* OOBase::detail::ProactorPosix::accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		err = EINVAL;
		return NULL;
	}

	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		mode_t mode = 0666;
		if (psa)
			mode = psa->mode;

		// Compose filename
		sockaddr_un addr = {0};
		socklen_t addr_len;
		POSIX::create_unix_socket_address(addr,addr_len,path);

		err = pAcceptor->bind((sockaddr*)&addr,addr_len,mode);
		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}

	return pAcceptor;
}

OOBase::AsyncSocket* OOBase::detail::ProactorPosix::connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout)
{
	int fd = Net::open_socket(addr->sa_family,SOCK_STREAM,0,err);
	if (err)
		return NULL;

	if ((err = Net::connect(fd,addr,addr_len,timeout)) != 0)
	{
		Net::close_socket(fd);
		return NULL;
	}

	AsyncSocket* pSocket = attach(fd,err);
	if (!pSocket)
		Net::close_socket(fd);

	return pSocket;
}

OOBase::AsyncSocket* OOBase::detail::ProactorPosix::connect(const char* path, int& err, const Timeout& timeout)
{
	int fd = Net::open_socket(AF_UNIX,SOCK_STREAM,0,err);
	if (err)
		return NULL;

	// Compose filename
	sockaddr_un addr = {0};
	socklen_t addr_len;
	POSIX::create_unix_socket_address(addr,addr_len,path);

	if ((err = Net::connect(fd,(sockaddr*)&addr,addr_len,timeout)) != 0)
	{
		Net::close_socket(fd);
		return NULL;
	}

	AsyncSocket* pSocket = attach(fd,err);
	if (!pSocket)
		Net::close_socket(fd);

	return pSocket;
}

OOBase::AsyncSocket* OOBase::detail::ProactorPosix::attach(socket_t sock, int& err)
{
	// Set non-blocking...
	err = POSIX::set_non_blocking(sock,true);
	if (err)
		return NULL;

	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this,sock);
	if (!pSocket)
		err = ENOMEM;
	else
	{
		err = pSocket->init();
		if (err != 0)
		{
			delete pSocket;
			pSocket = NULL;
		}
	}

	return pSocket;
}

#endif // defined(HAVE_UNISTD_H) && !defined(_WIN32)
