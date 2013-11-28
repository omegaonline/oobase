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

#include "../include/OOBase/Posix.h"
#include "../include/OOBase/Queue.h"
#include "../include/OOBase/StackAllocator.h"

#include "ProactorPosix.h"
#include "BSDSocket.h"

#if defined(HAVE_UNISTD_H)

#include <sys/stat.h>
#include <string.h>

#if !defined(MSG_NOSIGNAL)
#if (defined(__APPLE__) || defined(__MACH__))
#define MSG_NOSIGNAL SO_NOSIGPIPE
#else
#define MSG_NOSIGNAL 0
#endif
#endif

namespace
{
	class PosixAsyncSocket :
			public OOBase::AsyncSocket,
			public OOBase::AllocatorNew<OOBase::CrtAllocator>
	{
	public:
		PosixAsyncSocket(OOBase::detail::ProactorPosix* pProactor, int fd);
		virtual ~PosixAsyncSocket();

		int init();

		int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes);
		int recv_msg(void* param, recv_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer, size_t data_bytes);
		int send(void* param, send_callback_t callback, OOBase::Buffer* buffer);
		int send_v(void* param, send_v_callback_t callback, OOBase::Buffer* buffers[], size_t count);
		int send_msg(void* param, send_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer);
		int shutdown(bool bSend, bool bRecv);
		OOBase::socket_t get_handle() const;

	protected:
		OOBase::AllocatorInstance& get_internal_allocator() const
		{
			return m_pProactor->get_internal_allocator();
		}

	private:
		struct RecvItem
		{
			void*           m_param;
			OOBase::Buffer* m_buffer;
			size_t          m_bytes;
			OOBase::Buffer* m_ctl_buffer;
			union
			{
				recv_callback_t     m_callback;
				recv_msg_callback_t m_msg_callback;
			};
		};

		struct RecvNotify
		{
			int             m_err;
			struct RecvItem m_item;
		};

		struct SendItem
		{
			void*           m_param;
			size_t          m_count;
			union
			{
				struct
				{
					OOBase::Buffer* m_ctl_buffer;
					OOBase::Buffer* m_buffer;
				};
				OOBase::Buffer** m_buffers;
			};
			union
			{
				send_callback_t     m_callback;
				send_v_callback_t   m_v_callback;
				send_msg_callback_t m_msg_callback;
			};
		};

		struct SendNotify
		{
			int             m_err;
			struct SendItem m_item;
		};

		OOBase::detail::ProactorPosix* m_pProactor;
		int                            m_fd;
		OOBase::Mutex                  m_lock;
		OOBase::Queue<RecvItem>        m_recv_queue;
		OOBase::Queue<SendItem>        m_send_queue;

		static void fd_callback(int fd, void* param, unsigned int events);
		void process_recv(OOBase::Queue<RecvNotify,OOBase::AllocatorInstance>& notify_queue);
		int process_recv_i(RecvItem* item, bool& watch_again);
		int process_recv_msg(RecvItem* item, bool& watch_again);
		void process_send(OOBase::Queue<SendNotify,OOBase::AllocatorInstance>& notify_queue);
		int process_send_i(SendItem* item, bool& watch_again);
		int process_send_v(SendItem* item, bool& watch_again);
		int process_send_msg(SendItem* item, bool& watch_again);
	};
}

PosixAsyncSocket::PosixAsyncSocket(OOBase::detail::ProactorPosix* pProactor, int fd) :
		m_pProactor(pProactor),
		m_fd(fd)
{ }

PosixAsyncSocket::~PosixAsyncSocket()
{
	m_pProactor->unbind_fd(m_fd);

	OOBase::Net::close_socket(m_fd);

	// Free all items
	RecvItem recv_item;
	while (m_recv_queue.pop(&recv_item))
	{
		if (recv_item.m_buffer)
			recv_item.m_buffer->release();
		if (recv_item.m_ctl_buffer)
			recv_item.m_ctl_buffer->release();
	}

	SendItem send_item;
	while (m_send_queue.pop(&send_item))
	{
		if (send_item.m_ctl_buffer)
			send_item.m_ctl_buffer->release();

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

			free(send_item.m_buffers);
		}
	}
}

int PosixAsyncSocket::init()
{
	return m_pProactor->bind_fd(m_fd,this,&fd_callback);
}

int PosixAsyncSocket::recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
{
	int err = 0;
	if (bytes)
	{
		if (!buffer)
			return EINVAL;

		err = buffer->space(bytes);
		if (err)
			return err;
	}
	else if (!buffer || !buffer->space())
		return 0;

	if (!callback)
		return EINVAL;

	RecvItem item = { param, buffer, bytes, NULL };
	item.m_callback = callback;
	item.m_buffer->addref();

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	bool watch = m_recv_queue.empty();
	err = m_recv_queue.push(item);

	guard.release();

	if (err)
	{
		item.m_buffer->release();
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXRecv) : 0);
}

int PosixAsyncSocket::recv_msg(void* param, recv_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer, size_t data_bytes)
{
	int err = 0;
	if (data_bytes)
	{
		if (!data_buffer)
			return EINVAL;

		err = data_buffer->space(data_bytes);
		if (err)
			return err;
	}
	else if ((!data_buffer || !data_buffer->space()) && (!ctl_buffer || !ctl_buffer->space()))
		return 0;

	if (!data_buffer || !data_buffer->space() || !ctl_buffer || !ctl_buffer->space() || !callback)
		return EINVAL;

	RecvItem item = { param, data_buffer, data_bytes, ctl_buffer };
	item.m_msg_callback = callback;
	item.m_buffer->addref();
	item.m_ctl_buffer->addref();

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	bool watch = m_recv_queue.empty();
	err = m_recv_queue.push(item);

	guard.release();

	if (err)
	{
		item.m_buffer->release();
		item.m_ctl_buffer->release();
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXRecv) : 0);
}

int PosixAsyncSocket::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	size_t bytes = (buffer ? buffer->length() : 0);
	if (bytes == 0)
		return 0;

	if (!buffer)
		return EINVAL;

	SendItem item = { param, 1 };
	item.m_callback = callback;
	item.m_buffer = buffer;
	item.m_buffer->addref();

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

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

int PosixAsyncSocket::send_v(void* param, send_v_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return EINVAL;

	// Count how many actual buffers we have
	size_t actual_count = 0;
	for (size_t i = 0; i < count; ++i)
	{
		if (buffers[i] && buffers[i]->length() > 0)
			++actual_count;
	}

	if (actual_count == 0)
		return 0;

	SendItem item = { param, actual_count };
	item.m_v_callback = callback;
	item.m_buffers = static_cast<OOBase::Buffer**>(m_pProactor->get_internal_allocator().allocate(actual_count * sizeof(OOBase::Buffer*),OOBase::alignment_of<OOBase::Buffer*>::value));
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

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	bool watch = m_send_queue.empty();
	int err = m_send_queue.push(item);

	guard.release();

	if (err)
	{
		for (size_t i=0;i<actual_count;++i)
			item.m_buffers[i]->release();

		free(item.m_buffers);
		return err;
	}

	return (watch ? m_pProactor->watch_fd(m_fd,OOBase::detail::eTXSend) : 0);
}

int PosixAsyncSocket::send_msg(void* param, send_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer)
{
	size_t data_len = (data_buffer ? data_buffer->length() : 0);
	size_t ctl_len = (ctl_buffer ? ctl_buffer->length() : 0);

	if (!data_len && !ctl_len)
		return 0;

	if (!data_len || !ctl_len)
		return EINVAL;

	SendItem item = { param, 1 };
	item.m_msg_callback = callback;
	item.m_ctl_buffer = ctl_buffer;
	item.m_ctl_buffer->addref();
	item.m_buffer = data_buffer;
	item.m_buffer->addref();

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

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

int PosixAsyncSocket::shutdown(bool bSend, bool bRecv)
{
	int how = -1;
	if (bSend && bRecv)
		how = SHUT_RDWR;
	else if (bSend)
		how = SHUT_WR;
	else if (bRecv)
		how = SHUT_RD;

	return (how != -1 ? ::shutdown(m_fd,how) : 0);
}

OOBase::socket_t PosixAsyncSocket::get_handle() const
{
	return m_fd;
}

void PosixAsyncSocket::fd_callback(int fd, void* param, unsigned int events)
{
	PosixAsyncSocket* pThis = static_cast<PosixAsyncSocket*>(param);
	if (pThis->m_fd == fd)
	{
		OOBase::StackAllocator<512> allocator;
		OOBase::Queue<RecvNotify,OOBase::AllocatorInstance> recv_notify_queue(allocator);
		OOBase::Queue<SendNotify,OOBase::AllocatorInstance> send_notify_queue(allocator);

		OOBase::Guard<OOBase::Mutex> guard(pThis->m_lock);

		if (events & OOBase::detail::eTXRecv)
			pThis->process_recv(recv_notify_queue);

		if (events & OOBase::detail::eTXSend)
			pThis->process_send(send_notify_queue);

		guard.release();

		RecvNotify recv_notify;
		while (recv_notify_queue.pop(&recv_notify))
		{
#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				if (recv_notify.m_item.m_ctl_buffer)
					(*recv_notify.m_item.m_msg_callback)(recv_notify.m_item.m_param,recv_notify.m_item.m_buffer,recv_notify.m_item.m_ctl_buffer,recv_notify.m_err);
				else
					(*recv_notify.m_item.m_callback)(recv_notify.m_item.m_param,recv_notify.m_item.m_buffer,recv_notify.m_err);
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				if (recv_notify.m_item.m_ctl_buffer)
					recv_notify.m_item.m_ctl_buffer->release();
				recv_notify.m_item.m_buffer->release();
				throw;
			}
#endif
			if (recv_notify.m_item.m_ctl_buffer)
				recv_notify.m_item.m_ctl_buffer->release();
			recv_notify.m_item.m_buffer->release();
		}

		SendNotify send_notify;
		while (send_notify_queue.pop(&send_notify))
		{
			if (send_notify.m_item.m_count == 1)
			{
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					if (send_notify.m_item.m_ctl_buffer)
					{
						if (send_notify.m_item.m_msg_callback)
							(*send_notify.m_item.m_msg_callback)(send_notify.m_item.m_param,send_notify.m_item.m_buffer,send_notify.m_item.m_ctl_buffer,send_notify.m_err);
					}
					else if (send_notify.m_item.m_callback)
						(*send_notify.m_item.m_callback)(send_notify.m_item.m_param,send_notify.m_item.m_buffer,send_notify.m_err);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				}
				catch (...)
				{
					if (send_notify.m_item.m_ctl_buffer)
						send_notify.m_item.m_ctl_buffer->release();
					send_notify.m_item.m_buffer->release();
					throw;
				}
#endif
				if (send_notify.m_item.m_ctl_buffer)
					send_notify.m_item.m_ctl_buffer->release();
				send_notify.m_item.m_buffer->release();
			}
			else
			{
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					if (send_notify.m_item.m_v_callback)
						(*send_notify.m_item.m_v_callback)(send_notify.m_item.m_param,send_notify.m_item.m_buffers,send_notify.m_item.m_count,send_notify.m_err);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				}
				catch (...)
				{
					for (size_t i = 0;i<send_notify.m_item.m_count;++i)
					{
						if (send_notify.m_item.m_buffers[i])
							send_notify.m_item.m_buffers[i]->release();
					}

					pThis->m_pProactor->get_internal_allocator().free(send_notify.m_item.m_buffers);
					throw;
				}
#endif
				for (size_t i = 0;i<send_notify.m_item.m_count;++i)
				{
					if (send_notify.m_item.m_buffers[i])
						send_notify.m_item.m_buffers[i]->release();
				}

				pThis->m_pProactor->get_internal_allocator().free(send_notify.m_item.m_buffers);
			}
		}
	}
}

int PosixAsyncSocket::process_recv_i(RecvItem* item, bool& watch_again)
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

int PosixAsyncSocket::process_recv_msg(RecvItem* item, bool& watch_again)
{
	// We only do a single read
	struct iovec io = {0};
	io.iov_base = item->m_buffer->wr_ptr();
	io.iov_len = (item->m_bytes ? item->m_bytes : item->m_buffer->space());

	struct msghdr msg = {0};
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = item->m_ctl_buffer->wr_ptr();
	msg.msg_controllen = item->m_ctl_buffer->space();

#ifdef MSG_CMSG_CLOEXEC
	msg.msg_flags |= MSG_CMSG_CLOEXEC;
#endif

	ssize_t r = 0;
	do
	{
		r = ::recvmsg(m_fd,&msg,msg.msg_flags);

#ifdef MSG_CMSG_CLOEXEC
		if (r < 0 && errno == EINVAL)
		{
			msg.msg_flags &= ~MSG_CMSG_CLOEXEC;
			r = ::recvmsg(m_fd,&msg,msg.msg_flags);
		}
#endif
	}
	while (r == -1 && errno == EINTR);

	if (r == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			watch_again = true;
		else
			return errno;
	}
	else if (r > 0)
	{
		int err = item->m_ctl_buffer->wr_ptr(msg.msg_controllen);
		if (!err)
			err = item->m_buffer->wr_ptr(r);
		if (err)
			return err;
	}

	return 0;
}

void PosixAsyncSocket::process_recv(OOBase::Queue<RecvNotify,OOBase::AllocatorInstance>& notify_queue)
{
	int err = 0;
	while (!m_recv_queue.empty())
	{
		if (!err)
		{
			RecvItem* front = m_recv_queue.front();

			bool watch_again = false;
			if (front->m_ctl_buffer)
				err = process_recv_msg(front,watch_again);
			else
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

		RecvNotify notify;
		notify.m_err = err;
		notify.m_item = item;

		err = notify_queue.push(notify);
		if (err)
		{
			item.m_buffer->release();
			if (item.m_ctl_buffer)
				item.m_ctl_buffer->release();

			OOBase_CallCriticalFailure(err);
		}
	}
}

int PosixAsyncSocket::process_send_i(SendItem* item, bool& watch_again)
{
	// Single send
	int err = 0;
	for (;;)
	{
		ssize_t sent = 0;
		do
		{
			sent = ::send(m_fd,item->m_buffer->rd_ptr(),item->m_buffer->length(),0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
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

	return err;
}

int PosixAsyncSocket::process_send_v(SendItem* item, bool& watch_again)
{
	// Sendv

	// Find first valid buffer
	size_t first_buffer = 0;
	for (size_t i = 0;i<item->m_count;++i)
	{
		if (item->m_buffers[i]->length())
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
				msg.msg_iov[i].iov_base = const_cast<uint8_t*>(item->m_buffers[i+first_buffer]->rd_ptr());
			}

			while (msg.msg_iovlen)
			{
				ssize_t sent = 0;
				do
				{
					sent = ::sendmsg(m_fd,&msg,0
#if defined(MSG_NOSIGNAL)
						| MSG_NOSIGNAL
#endif
					);
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
							++first_buffer;

							sent -= msg.msg_iov->iov_len;
							++msg.msg_iov;
							if (--msg.msg_iovlen == 0)
								break;
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

	return err;
}

int PosixAsyncSocket::process_send_msg(SendItem* item, bool& watch_again)
{
	// We only do a single write
	struct iovec io = {0};
	io.iov_base = const_cast<uint8_t*>(item->m_buffer->rd_ptr());
	io.iov_len = item->m_buffer->length();

	struct msghdr msg = {0};
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = const_cast<uint8_t*>(item->m_ctl_buffer->rd_ptr());
	msg.msg_controllen = item->m_ctl_buffer->length();

	ssize_t sent = 0;
	do
	{
		sent = ::sendmsg(m_fd,&msg,0
#if defined(MSG_NOSIGNAL)
					| MSG_NOSIGNAL
#endif
				);
	}
	while (sent == -1 && errno == EINTR);

	if (sent == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			watch_again = true;
		else
			return errno;
	}
	else
	{
		item->m_buffer->rd_ptr(sent);
		item->m_ctl_buffer->rd_ptr(msg.msg_controllen);
	}

	return 0;
}

void PosixAsyncSocket::process_send(OOBase::Queue<SendNotify,OOBase::AllocatorInstance>& notify_queue)
{
	int err = 0;
	while (!m_send_queue.empty())
	{
		if (!err)
		{
			SendItem* front = m_send_queue.front();

			bool watch_again = false;
			if (front->m_count == 1)
			{
				if (front->m_ctl_buffer)
					err = process_send_msg(front,watch_again);
				else
					err = process_send_i(front,watch_again);
			}
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

		SendNotify notify;
		notify.m_err = err;
		notify.m_item = item;
		err = notify_queue.push(notify);
		if (err)
		{
			if (item.m_count == 1)
			{
				if (item.m_buffer)
					item.m_buffer->release();
				if (item.m_ctl_buffer)
					item.m_ctl_buffer->release();
			}
			else
			{
				for (size_t i = 0;i<item.m_count;++i)
				{
					if (item.m_buffers[i])
						item.m_buffers[i]->release();
				}

				free(item.m_buffers);
			}

			OOBase_CallCriticalFailure(err);
		}
	}
}

namespace
{
	class SocketAcceptor :
			public OOBase::Acceptor,
			public OOBase::AllocatorNew<OOBase::CrtAllocator>
	{
	public:
		SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_callback_t callback);
		SocketAcceptor(OOBase::detail::ProactorPosix* pProactor, void* param, OOBase::Proactor::accept_pipe_callback_t callback);
		virtual ~SocketAcceptor();

		int bind(const sockaddr* addr, socklen_t addr_len, SECURITY_ATTRIBUTES& sa);

	private:
		OOBase::detail::ProactorPosix*           m_pProactor;
		void*                                    m_param;
		OOBase::Proactor::accept_callback_t      m_callback;
		OOBase::Proactor::accept_pipe_callback_t m_callback_local;
		SECURITY_ATTRIBUTES                      m_sa;
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

int SocketAcceptor::bind(const sockaddr* addr, socklen_t addr_len, SECURITY_ATTRIBUTES& sa)
{
	// Create a new socket
	int err = 0;
	int fd = OOBase::Net::open_socket(addr->sa_family,SOCK_STREAM,0,err);
	if (err)
		return err;

	m_sa = sa;

	// Apparently, chmod before bind()

	// Bind to the address
	if ((m_sa.mode && ::fchmod(fd,m_sa.mode) != 0) || ::bind(fd,addr,addr_len) != 0 || ::listen(fd,SOMAXCONN) != 0)
		err = errno;
	else
	{
		if (m_sa.pass_credentials)
		{
#if defined(SO_PASSCRED)
			int val = 1;
			if (::setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &val, sizeof(val)) != 0)
				err = errno;
#elif defined(LOCAL_CREDS)
			int val = 1;
			if (::setsockopt(fd, SOL_SOCKET, LOCAL_CREDS, &val, sizeof(val)) != 0)
				err = errno;
#endif
		}

		if (!err)
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

#if defined (HAVE_ACCEPT4)
			new_fd = ::accept4(m_fd,(sockaddr*)&addr,&addr_len,0
#if defined(SOCK_NONBLOCK)
					| SOCK_NONBLOCK
#endif
#if defined(SOCK_CLOEXEC)
					| SOCK_CLOEXEC
#endif
					);
#else
			new_fd = ::accept(m_fd,(sockaddr*)&addr,&addr_len);
#endif
		}
		while (new_fd == -1 && errno == EINTR);

		PosixAsyncSocket* pSocket = NULL;
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
#if !defined(SOCK_CLOEXEC)
			if (err == 0)
				err = OOBase::POSIX::set_non_blocking(new_fd,true);
#endif
#if !defined(SOCK_NONBLOCK)
			if (err == 0)
				err = OOBase::POSIX::set_close_on_exec(new_fd,true);
#endif

			if (m_sa.pass_credentials)
			{
#if defined(SO_PASSCRED)
				int val = 1;
				if (::setsockopt(new_fd, SOL_SOCKET, SO_PASSCRED, &val, sizeof(val)) != 0)
					err = errno;
#elif defined(LOCAL_CREDS)
				int val = 1;
				if (::setsockopt(fd, SOL_SOCKET, LOCAL_CREDS, &val, sizeof(val)) != 0)
					err = errno;
#endif
			}

			if (err == 0)
			{
				// Wrap the handle
				pSocket = new PosixAsyncSocket(m_pProactor,new_fd);
				if (!pSocket)
					err = ENOMEM;
				else
				{
					err = pSocket->init();
					if (err != 0)
					{
						pSocket->release();
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

	SocketAcceptor* pAcceptor = new SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		SECURITY_ATTRIBUTES defaults;
		defaults.mode = 0;
		defaults.pass_credentials = false;

		err = pAcceptor->bind(addr,addr_len,defaults);
		if (err != 0)
		{
			OOBase::CrtAllocator::delete_free(pAcceptor);
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

	SocketAcceptor* pAcceptor = new SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		SECURITY_ATTRIBUTES defaults;
		defaults.mode = 0777;
		defaults.pass_credentials = false;

		// Compose filename
		sockaddr_un addr = {0};
		socklen_t addr_len;
		POSIX::create_unix_socket_address(addr,addr_len,path);

		err = pAcceptor->bind((sockaddr*)&addr,addr_len,psa ? *psa : defaults);
		if (err != 0)
		{
			OOBase::CrtAllocator::delete_free(pAcceptor);
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

	PosixAsyncSocket* pSocket = new PosixAsyncSocket(this,sock);
	if (!pSocket)
		err = ENOMEM;
	else
	{
		err = pSocket->init();
		if (err != 0)
		{
			pSocket->release();
			pSocket = NULL;
		}
	}

	return pSocket;
}

#endif // defined(HAVE_UNISTD_H) && !defined(_WIN32)
