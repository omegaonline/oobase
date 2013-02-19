///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#if defined(HAVE_UNISTD_H)

#include "../include/OOBase/Mutex.h"
#include "../include/OOBase/StackAllocator.h"
#include "../include/OOBase/Socket.h"
#include "../include/OOBase/Posix.h"

#include "BSDSocket.h"

#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>

OOBase::socket_t OOBase::Net::open_socket(int family, int type, int protocol, int& err)
{
	socket_t sock = ::socket(family,type
#if defined(SOCK_NONBLOCK)
			| SOCK_NONBLOCK
#endif
#if defined(SOCK_CLOEXEC)
			| SOCK_CLOEXEC
#endif
			,protocol);

	if (sock == -1)
		err = errno;
	else
		err = 0;

#if !defined(SOCK_NONBLOCK)
	if ((err = OOBase::POSIX::set_non_blocking(sock,true)) != 0)
	{
		OOBase::Net::close_socket(sock);
		sock = -1;
	}
#endif

#if !defined(SOCK_CLOEXEC)
	if ((err = OOBase::POSIX::set_close_on_exec(sock,true)) != 0)
	{
		OOBase::Net::close_socket(sock);
		sock = -1;
	}
#endif

	return sock;
}

int OOBase::Net::close_socket(socket_t sock)
{
	return POSIX::close(sock);
}

int OOBase::Net::bind(socket_t sock, const sockaddr* addr, socklen_t addr_len)
{
	if (::bind(sock,addr,addr_len) == -1)
		return errno;

	return 0;
}

int OOBase::Net::connect(socket_t sock, const sockaddr* addr, socklen_t addrlen, const Timeout& timeout)
{
	// Check blocking state
	bool non_blocking = false;
	bool changed_blocking = false;
	int err = 0;

	// Make sure we are non-blocking
	if (!timeout.is_infinite())
	{
		err = POSIX::get_non_blocking(sock,non_blocking);
		if (err)
			return err;

		if (!non_blocking)
		{
			changed_blocking = true;
			err = POSIX::set_non_blocking(sock,true);
			if (err)
				return err;
		}
	}

	// Do the connect
	do
	{
		err = ::connect(sock,addr,addrlen);
	}
	while (err == -1 && errno == EINTR);

	if (!err)
	{
		if (changed_blocking)
			err = POSIX::set_non_blocking(sock,non_blocking);

		return err;
	}

	// Check to see if we actually have an error
	if (errno != EINPROGRESS)
		err = errno;
	else
	{
		// Wait for the connect to go ahead - by select()ing on write
		fd_set wfds;
		while (!timeout.has_expired())
		{
			int count = 0;
			do
			{
				FD_ZERO(&wfds);
				FD_SET(sock,&wfds);

				struct ::timeval tv;
				timeout.get_timeval(tv);

				count = ::select(static_cast<int>(sock)+1,NULL,&wfds,NULL,timeout.is_infinite() ? NULL : &tv);
			}
			while (count == -1 && errno == EINTR);

			if (count == -1)
				err = errno;
			else if (count == 0)
				err = ETIMEDOUT;
			else
			{
				// If connect() did something...
				socklen_t len = sizeof(err);
				if (::getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
					err = errno;
			}
			break;
		}
	}

	if (changed_blocking)
	{
		int err2 = POSIX::set_non_blocking(sock,non_blocking);
		if (!err)
			err = err2;
	}

	return err;
}

int OOBase::Net::accept(socket_t accept_sock, socket_t& new_sock, const Timeout& timeout)
{
	new_sock = -1;

	// Check blocking state
	bool non_blocking = false;
	bool changed_blocking = false;

	int err = POSIX::get_non_blocking(accept_sock,non_blocking);
	if (err)
		return err;

	// Make sure we are non-blocking
	if (!non_blocking && !timeout.is_infinite())
	{
		changed_blocking = true;
		err = POSIX::set_non_blocking(accept_sock,true);
		if (err)
			return err;
	}
	else if (non_blocking && timeout.is_infinite())
	{
		changed_blocking = true;
		err = POSIX::set_non_blocking(accept_sock,false);
		if (err)
			return err;
	}

	while (!err && !timeout.has_expired())
	{
		do
		{
			new_sock = ::accept(accept_sock,NULL,NULL);
		}
		while (new_sock == -1 && errno == EINTR);

		if (new_sock != -1)
			break;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			err = errno;
			break;
		}

		fd_set read_fds;
		int count = 0;
		do
		{
			FD_ZERO(&read_fds);
			FD_SET(accept_sock,&read_fds);

			struct ::timeval tv;
			timeout.get_timeval(tv);

			count = ::select(static_cast<int>(accept_sock)+1,&read_fds,NULL,NULL,timeout.is_infinite() ? NULL : &tv);
		}
		while (count == -1 && errno == EINTR);

		if (count == -1)
			err = errno;
		else if (count == 0)
			err = ETIMEDOUT;
	}

	if (changed_blocking)
	{
		int err2 = POSIX::set_non_blocking(accept_sock,non_blocking);
		if (!err)
			err = err2;
	}

	if (err && new_sock != -1)
	{
		Net::close_socket(new_sock);
		new_sock = -1;
	}

	return err;
}

namespace
{
	class BSDSocket : public OOBase::Socket
	{
	public:
		BSDSocket(OOBase::socket_t sock);
		virtual ~BSDSocket();

		size_t send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		size_t send_msg(const void* data_buf, size_t data_len, const void* ctl_buf, size_t ctl_len, int& err, const OOBase::Timeout& timeout);
		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		size_t recv_msg(void* data_buf, size_t data_len, OOBase::Buffer* ctl_buffer, int& err, const OOBase::Timeout& timeout);

		int shutdown(bool bSend, bool bRecv);
		int close();

	private:
		int           m_sock;
		OOBase::Mutex m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex m_send_lock;

		int do_select(bool bWrite, const OOBase::Timeout& timeout);

		void destroy()
		{
			OOBase::CrtAllocator::delete_free(this);
		}
	};

	int connect_i(const char* address, const char* port, int& err, const OOBase::Timeout& timeout)
	{
		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = NULL;
		if (::getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			err = errno;
			return -1;
		}

		int sock = -1;

		// Took too long to resolve...
		if (timeout.has_expired())
			err = ETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				sock = OOBase::Net::open_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,err);
				if (err)
					break;

				if ((err = OOBase::Net::connect(sock,pAddr->ai_addr,pAddr->ai_addrlen,timeout)) != 0)
					OOBase::Net::close_socket(sock);
				else
					break;

				if (timeout.has_expired())
				{
					err = ETIMEDOUT;
					break;
				}
			}
		}

		// Done with address info
		::freeaddrinfo(pResults);

		return sock;
	}
}

BSDSocket::BSDSocket(int sock) :
		m_sock(sock)
{
}

BSDSocket::~BSDSocket()
{
	close();
}

int BSDSocket::do_select(bool bWrite, const OOBase::Timeout& timeout)
{
	fd_set fds,efds;
	int count = 0;
	do
	{
		FD_ZERO(&fds);
		FD_ZERO(&efds);
		FD_SET(m_sock,&fds);
		FD_SET(m_sock,&efds);

		struct ::timeval tv;
		timeout.get_timeval(tv);

		count = ::select(m_sock+1,(bWrite ? NULL : &fds),(bWrite ? &fds : NULL),&efds,timeout.is_infinite() ? NULL : &tv);
	}
	while (count == -1 && errno == EINTR);

	if (count == -1)
	{
		// if we close a file handle during a select we get EBADF
		if (errno == EBADF)
			return ECONNRESET;

		return errno;
	}

	if (count == 0)
		return ETIMEDOUT;

	if (FD_ISSET(m_sock,&efds))
	{
		// Error
		int err = 0;
		socklen_t len = sizeof(err);
		if (::getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
			err = errno;

		return err;
	}

	return 0;
}

size_t BSDSocket::send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (!buf)
	{
		err = EINVAL;
		return 0;
	}

	const char* cbuf = static_cast<const char*>(buf);
	size_t to_send = len;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

	if (m_sock == -1)
	{
		err = ECONNRESET;
		return 0;
	}

	do
	{
		ssize_t sent = 0;
		do
		{
			sent = ::send(m_sock,cbuf,to_send,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent > 0)
		{
			to_send -= sent;
			cbuf += sent;
		}
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(true,timeout);
			}
		}
	}
	while (!err && to_send);

	return (len - to_send);
}

int BSDSocket::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return EINVAL;

	OOBase::StackArrayPtr<struct iovec,8> iovecs(count);
	if (!iovecs)
		return ERROR_OUTOFMEMORY;

	struct msghdr msg = {0};
	msg.msg_iov = iovecs;
	msg.msg_iovlen = 0;

	for (size_t i=0;i<count;++i)
	{
		size_t to_write = (buffers[i] ? buffers[i]->length() : 0);
		if (to_write)
		{
			msg.msg_iov[msg.msg_iovlen].iov_len = to_write;
			msg.msg_iov[msg.msg_iovlen].iov_base = const_cast<char*>(buffers[i]->rd_ptr());
			++msg.msg_iovlen;
		}
	}

	if (msg.msg_iovlen == 0)
		return 0;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

	if (m_sock == -1)
		return ECONNRESET;

	size_t first_buffer = 0;
	int err = 0;
	do
	{
		ssize_t sent = 0;
		do
		{
			sent = ::sendmsg(m_sock,&msg,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent > 0)
		{
			// Update buffers...
			do
			{
				if (buffers[first_buffer] && buffers[first_buffer]->length())
				{
					if (static_cast<size_t>(sent) >= msg.msg_iov->iov_len)
					{
						buffers[first_buffer]->rd_ptr(msg.msg_iov->iov_len);
						++first_buffer;
						sent -= msg.msg_iov->iov_len;
						++msg.msg_iov;
						if (--msg.msg_iovlen == 0)
							break;
					}
					else
					{
						buffers[first_buffer]->rd_ptr(sent);
						msg.msg_iov->iov_len -= sent;
						msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + sent;
						sent = 0;
					}
				}
				else
					++first_buffer;
			}
			while (first_buffer < count && sent > 0);
		}
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(true,timeout);
			}
		}
	}
	while (!err && msg.msg_iovlen);

	return err;
}

size_t BSDSocket::send_msg(const void* data_buf, size_t data_len, const void* ctl_buf, size_t ctl_len, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	data_len = (data_buf ? data_len : 0);
	ctl_len = (ctl_buf ? ctl_len : 0);

	if (!data_len && !ctl_len)
		return 0;

	if (!data_len || !ctl_len)
	{
		err = EINVAL;
		return 0;
	}

	struct iovec io = {0};
	io.iov_base = const_cast<void*>(data_buf);
	io.iov_len = data_len;

	struct msghdr msg = {0};
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = const_cast<void*>(ctl_buf);
	msg.msg_controllen = ctl_len;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

	if (m_sock == -1)
	{
		err = ECONNRESET;
		return 0;
	}

	ssize_t sent = 0;
	do
	{
		do
		{
			sent = ::sendmsg(m_sock,&msg,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent > 0)
			break;

		err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK)
		{
			// Do the select...
			err = do_select(true,timeout);
		}
	}
	while (!err);

	return sent;
}

size_t BSDSocket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (!buf)
	{
		err = EINVAL;
		return 0;
	}

	char* cbuf = static_cast<char*>(buf);
	size_t to_recv = len;
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	if (m_sock == -1)
	{
		err = ECONNRESET;
		return 0;
	}

	do
	{
		ssize_t recvd = 0;
		do
		{
			recvd = ::recv(m_sock,cbuf,to_recv,0);
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd > 0)
		{
			to_recv -= recvd;
			cbuf += recvd;

			if (!bAll)
				break;
		}
		else if (recvd == 0)
			break;
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(false,timeout);
			}
		}
	}
	while (!err && to_recv);

	return (len - to_recv);
}

int BSDSocket::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return EINVAL;

	OOBase::StackArrayPtr<struct iovec,8> iovecs(count);
	if (!iovecs)
		return ERROR_OUTOFMEMORY;

	struct msghdr msg = {0};
	msg.msg_iov = iovecs;
	msg.msg_iovlen = 0;

	for (size_t i=0;i<count;++i)
	{
		size_t to_read = (buffers[i] ? buffers[i]->space() : 0);
		if (to_read)
		{
			msg.msg_iov[msg.msg_iovlen].iov_len = to_read;
			msg.msg_iov[msg.msg_iovlen].iov_base = buffers[i]->wr_ptr();
			++msg.msg_iovlen;
		}
	}

	if (msg.msg_iovlen == 0)
		return 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	if (m_sock == -1)
		return ECONNRESET;

	size_t first_buffer = 0;
	int err = 0;
	do
	{
		ssize_t recvd = 0;
		do
		{
			recvd = ::recvmsg(m_sock,&msg,0);
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd > 0)
		{
			// Update buffers...
			do
			{
				if (buffers[first_buffer] && buffers[first_buffer]->space())
				{
					if (static_cast<size_t>(recvd) >= msg.msg_iov->iov_len)
					{
						buffers[first_buffer]->wr_ptr(msg.msg_iov->iov_len);
						++first_buffer;
						recvd -= msg.msg_iov->iov_len;
						++msg.msg_iov;
						if (--msg.msg_iovlen == 0)
							break;
					}
					else
					{
						buffers[first_buffer]->wr_ptr(recvd);
						msg.msg_iov->iov_len -= recvd;
						msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + recvd;
						recvd = 0;
					}
				}
				else
					++first_buffer;
			}
			while (first_buffer < count && recvd > 0);
		}
		else if (recvd == 0)
			break;
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(false,timeout);
			}
		}
	}
	while (!err && msg.msg_iovlen);

	return err;
}

size_t BSDSocket::recv_msg(void* data_buf, size_t data_len, OOBase::Buffer* ctl_buffer, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	data_len = (data_buf ? data_len : 0);
	size_t ctl_len = (ctl_buffer ? ctl_buffer->space() : 0);

	if (!data_len && !ctl_len)
		return 0;

	if (!data_len || !ctl_len)
	{
		err = EINVAL;
		return 0;
	}

	struct iovec io = {0};
	io.iov_base = data_buf;
	io.iov_len = data_len;

	struct msghdr msg = {0};
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = ctl_buffer->wr_ptr();
	msg.msg_controllen = ctl_len;

#ifdef MSG_CMSG_CLOEXEC
	msg.msg_flags |= MSG_CMSG_CLOEXEC;
#endif

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	if (m_sock == -1)
	{
		err = ECONNRESET;
		return 0;
	}

	ssize_t recvd = 0;
	do
	{
		do
		{
			recvd = ::recvmsg(m_sock,&msg,msg.msg_flags);
#ifdef MSG_CMSG_CLOEXEC
			if (recvd < 0 && errno == EINVAL)
			{
				msg.msg_flags &= ~MSG_CMSG_CLOEXEC;
				recvd = ::recvmsg(m_sock,&msg,msg.msg_flags);
			}
#endif
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd >= 0)
		{
			err = ctl_buffer->wr_ptr(msg.msg_controllen);
			break;
		}

		err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK)
		{
			// Do the select...
			err = do_select(false,timeout);
		}
	}
	while (!err);

	return recvd;
}

int BSDSocket::shutdown(bool bSend, bool bRecv)
{
	int how = -1;
	if (bSend && bRecv)
		how = SHUT_RDWR;
	else if (bSend)
		how = SHUT_WR;
	else if (bRecv)
		how = SHUT_RD;

	return (how != -1 ? ::shutdown(m_sock,how) : 0);
}

int BSDSocket::close()
{
	int err = OOBase::Net::close_socket(m_sock);
	if (!err)
	{
		OOBase::Guard<OOBase::Mutex> guard1(m_recv_lock);
		OOBase::Guard<OOBase::Mutex> guard2(m_send_lock);

		m_sock = -1;
	}
	return err;
}

OOBase::Socket* OOBase::Socket::attach(socket_t sock, int& err)
{
	err = OOBase::POSIX::set_non_blocking(sock,true);
	if (err)
		return NULL;

	OOBase::Socket* pSocket = OOBase::CrtAllocator::allocate_new<BSDSocket>(sock);
	if (!pSocket)
		err = ENOMEM;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int& err, const Timeout& timeout)
{
	socket_t sock = connect_i(address,port,err,timeout);
	if (sock == -1)
		return NULL;

	OOBase::Socket* pSocket = OOBase::CrtAllocator::allocate_new<BSDSocket>(sock);
	if (!pSocket)
	{
		err = ENOMEM;
		OOBase::Net::close_socket(sock);
	}

	return pSocket;
}

void OOBase::POSIX::create_unix_socket_address(sockaddr_un& addr, socklen_t& len, const char* path)
{
	addr.sun_family = AF_UNIX;
	
	if (path[0] == '\0')
	{
		addr.sun_path[0] = '\0';
		strncpy(addr.sun_path+1,path+1,sizeof(addr.sun_path)-1);
		len = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path+1) + 1;
	}
	else
	{
		strncpy(addr.sun_path,path,sizeof(addr.sun_path)-1);
		addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
		len = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path);
	}
}

OOBase::Socket* OOBase::Socket::connect(const char* path, int& err, const Timeout& timeout)
{
	int sock = Net::open_socket(AF_UNIX,SOCK_STREAM,0,err);
	if (err)
		return NULL;

	sockaddr_un addr;
	socklen_t addr_len = 0;
	POSIX::create_unix_socket_address(addr,addr_len,path);
	
	if ((err = Net::connect(sock,(sockaddr*)(&addr),addr_len,timeout)) != 0)
	{
		OOBase::Net::close_socket(sock);
		return NULL;
	}

	OOBase::Socket* pSocket = OOBase::CrtAllocator::allocate_new<BSDSocket>(sock);
	if (!pSocket)
	{
		err = ENOMEM;
		OOBase::Net::close_socket(sock);
	}

	return pSocket;
}

#endif // defined(HAVE_UNISTD_H)
