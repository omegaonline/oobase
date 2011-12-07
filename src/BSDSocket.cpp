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

#if !defined(_WIN32)

#include "../include/OOBase/Posix.h"

#include "BSDSocket.h"

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>
#endif

int OOBase::BSD::set_non_blocking(socket_t sock, bool set)
{
#if defined(HAVE_UNISTD_H)

	int flags = fcntl(sock,F_GETFL);
	if (flags == -1)
		return errno;

	flags = (set ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
	if (fcntl(sock,F_SETFL,flags) == -1)
		return errno;

#endif

	return 0;
}

int OOBase::BSD::connect(socket_t sock, const sockaddr* addr, size_t addrlen, const OOBase::Countdown& countdown)
{
	// Do the connect
	if (::connect(sock,addr,static_cast<int>(addrlen)) != -1)
		return 0;

	// Check to see if we actually have an error
	int err = errno;
	if (err != EINPROGRESS)
		return err;

	// Wait for the connect to go ahead - by select()ing on write

	fd_set wfds;
	fd_set efds; // Annoyingly Winsock uses the exceptions not just the writes

	for (;;)
	{
		int count = 0;
		do
		{
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			FD_SET(sock,&wfds);
			FD_SET(sock,&efds);

			struct ::timeval tv;
			countdown.timeval(tv);

			count = ::select(static_cast<int>(sock+1),NULL,&wfds,&efds,countdown.is_infinite() ? NULL : &tv);
		}
		while (count == -1 && errno == EINTR);

		if (count == -1)
		{
			err = errno;
			return err;
		}
		else if (count == 1)
		{
			// If connect() did something...
			socklen_t len = sizeof(err);
			if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
				return errno;

			return err;
		}
		else
			return ETIMEDOUT;
	}
}

namespace
{
	class Socket : public OOBase::Socket
	{
	public:
		Socket(OOBase::socket_t sock);
		virtual ~Socket();

		size_t send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout = NULL);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout = NULL);
		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout = NULL);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout = NULL);

		void close();

	private:
		int  m_sock;

		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;

		int do_select(bool bWrite, const OOBase::Countdown& countdown);
	};

	int connect_i(const char* address, const char* port, int& err, const OOBase::timeval_t* timeout)
	{
		// Start a countdown
		OOBase::Countdown countdown(timeout);

		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = NULL;
		if (getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			err = errno;
			return -1;
		}

		int sock = -1;

		// Took too long to resolve...
		if (countdown.has_ended())
			err = ETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				err = 0;
				sock = ::socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol);
				if (sock == -1)
				{
					err = errno;
					break;
				}
				else
				{
#if defined(HAVE_UNISTD_H)
					if ((err = OOBase::POSIX::set_close_on_exec(sock,true)) != 0)
					{
						::close(sock);
						break;
					}
#endif
					if ((err = OOBase::BSD::connect(sock,pAddr->ai_addr,pAddr->ai_addrlen,countdown)) != 0)
						::close(sock);
					else
						break;
				}

				if (countdown.has_ended())
				{
					err = ETIMEDOUT;
					break;
				}
			}
		}

		// Done with address info
		freeaddrinfo(pResults);

		return sock;
	}
}

Socket::Socket(int sock) :
		m_sock(sock)
{
}

Socket::~Socket()
{
	::close(m_sock);
}

int Socket::do_select(bool bWrite, const OOBase::Countdown& countdown)
{
	fd_set fds,efds;
	int count = 0;
	do
	{
		FD_ZERO(&fds);
		FD_ZERO(&efds);
		FD_SET(m_sock,&fds);
		FD_SET(m_sock,&efds);

		struct timeval timeout;
		countdown.timeval(timeout);

		count = ::select(m_sock+1,(bWrite ? NULL : &fds),(bWrite ? &fds : NULL),&efds,countdown.is_infinite() ? NULL : &timeout);
	}
	while (count == -1 && errno == EINTR);

	if (count == -1)
		return errno;

	if (count == 0)
		return ETIMEDOUT;

	if (FD_ISSET(m_sock,&efds))
	{
		// Error
		int err = 0;
		socklen_t len = sizeof(err);
		if (getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
			err = errno;

		return err;
	}

	return 0;
}

size_t Socket::send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout)
{
	// Start a countdown
	OOBase::Countdown countdown(timeout);

	const char* cbuf = static_cast<const char*>(buf);
	size_t to_send = len;
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,timeout ? false : true);
	if (timeout && !guard.acquire(countdown))
	{
		err = ETIMEDOUT;
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
			if (err == EAGAIN)
			{
				// Do the select...
				err = do_select(true,countdown);
			}
		}
	}
	while (err == 0 && to_send);

	return (len - to_send);
}

int Socket::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	// Start a countdown
	OOBase::Countdown countdown(timeout);

	struct iovec static_bufs[4];
	OOBase::SmartPtr<struct iovec,OOBase::LocalAllocator> ptrBufs;

	struct msghdr msg = {0};
	msg.msg_iov = static_bufs;
	msg.msg_iovlen = count;

	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		if (!ptrBufs.allocate(count * sizeof(struct iovec)))
			return ENOMEM;

		msg.msg_iov = ptrBufs;
	}

	for (size_t i=0;i<count;++i)
	{
		msg.msg_iov[i].iov_len = buffers[i]->length();
		msg.msg_iov[i].iov_base = const_cast<char*>(buffers[i]->rd_ptr());
	}

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,timeout ? false : true);
	if (timeout && !guard.acquire(countdown))
		return ETIMEDOUT;

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
			while (sent > 0);
		}
		else
		{
			err = errno;
			if (err == EAGAIN)
			{
				// Do the select...
				err = do_select(true,countdown);
			}
		}
	}
	while (err == 0 && msg.msg_iovlen);

	return err;
}

size_t Socket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout)
{
	// Start a countdown
	OOBase::Countdown countdown(timeout);

	char* cbuf = static_cast<char*>(buf);
	size_t to_recv = len;
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(countdown))
	{
		err = ETIMEDOUT;
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
		else
		{
			if (recvd == 0)
				err = ECONNRESET;
			else
				err = errno;

			if (err == EAGAIN)
			{
				// Do the select...
				err = do_select(false,countdown);
			}
		}
	}
	while (err == 0 && to_recv);

	return (len - to_recv);
}

int Socket::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	// Start a countdown
	OOBase::Countdown countdown(timeout);

	struct iovec static_bufs[4];
	OOBase::SmartPtr<struct iovec,OOBase::LocalAllocator> ptrBufs;

	struct msghdr msg = {0};
	msg.msg_iov = static_bufs;
	msg.msg_iovlen = count;

	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		if (!ptrBufs.allocate(count * sizeof(struct iovec)))
			return ENOMEM;

		msg.msg_iov = ptrBufs;
	}

	for (size_t i=0;i<count;++i)
	{
		msg.msg_iov[i].iov_len = buffers[i]->space();
		msg.msg_iov[i].iov_base = buffers[i]->wr_ptr();
	}

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,timeout ? false : true);
	if (timeout && !guard.acquire(countdown))
		return ETIMEDOUT;

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
			while (recvd > 0);
		}
		else
		{
			if (recvd == 0)
				err = ECONNRESET;
			else
				err = errno;

			if (err == EAGAIN)
			{
				// Do the select...
				err = do_select(false,countdown);
			}
		}
	}
	while (err == 0 && msg.msg_iovlen);

	return err;
}

void Socket::close()
{
	::shutdown(m_sock,SHUT_WR);
}

OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int& err, const timeval_t* timeout)
{
	socket_t sock = connect_i(address,port,err,timeout);
	if (sock == -1)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock);
	if (!pSocket)
	{
		err = ENOMEM;
		::close(sock);
	}

	return pSocket;
}

#if defined(HAVE_UNISTD_H)

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

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int& err, const timeval_t* timeout)
{
	int sock = ::socket(AF_UNIX,SOCK_STREAM,0);
	if (sock == -1)
	{
		err = errno;
		return NULL;
	}

#if defined(HAVE_UNISTD_H)
	if ((err = OOBase::POSIX::set_close_on_exec(sock,true)) != 0)
	{
		::close(sock);
		return NULL;
	}
#endif

	sockaddr_un addr;
	socklen_t addr_len = 0;
	POSIX::create_unix_socket_address(addr,addr_len,path);
	
	if ((err = BSD::connect(sock,(sockaddr*)(&addr),addr_len,timeout)) != 0)
	{
		::close(sock);
		return NULL;
	}

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock);
	if (!pSocket)
	{
		err = ENOMEM;
		::close(sock);
	}

	return pSocket;
}
#endif // defined(HAVE_UNISTD_H)

#endif //  !defined(_WIN32)
