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

#include "../include/OOBase/Memory.h"

#if !defined(_WIN32)

#include "../include/OOBase/Posix.h"

#include "BSDSocket.h"

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>
#endif

OOBase::socket_t OOBase::BSD::create_socket(int family, int socktype, int protocol, int& err)
{
	err = 0;
	OOBase::socket_t sock = ::socket(family,socktype,protocol);
	if (sock == -1)
	{
		err = errno;
		return sock;
	}

	err = OOBase::BSD::set_non_blocking(sock,true);
	if (err != 0)
	{
		::close(sock);
		return -1;
	}

#if defined(HAVE_UNISTD_H)
	err = OOBase::POSIX::set_close_on_exec(sock,true);
	if (err != 0)
	{
		::close(sock);
		return -1;
	}
#endif

	return sock;
}

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

int OOBase::BSD::connect(int sock, const sockaddr* addr, size_t addrlen, const OOBase::timeval_t* timeout)
{
	// Do the connect
	if (::connect(sock,addr,static_cast<int>(addrlen)) != -1)
		return 0;

	// Check to see if we actually have an error
	int err = errno;
	if (err != EINPROGRESS)
		return err;

	// Wait for the connect to go ahead - by select()ing on write
	::timeval tv;
	if (timeout)
	{
		tv.tv_sec = static_cast<long>(timeout->tv_sec());
		tv.tv_usec = timeout->tv_usec();
	}

	fd_set wfds;
	fd_set efds; // Annoyingly Winsock uses the exceptions not just the writes

	for (;;)
	{
		FD_ZERO(&wfds);
		FD_ZERO(&efds);
		FD_SET(sock,&wfds);
		FD_SET(sock,&efds);

		int count = select(static_cast<int>(sock+1),NULL,&wfds,&efds,timeout ? &tv : NULL);
		if (count == -1)
		{
			err = errno;
			if (err != EINTR)
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
		//size_t send_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout = NULL);
		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout = NULL);
		//size_t recv_v(OOBase::Buffer* buffers[], size_t count, int& err, const OOBase::timeval_t* timeout = NULL);

		void close();

	private:
		int  m_sock;
	};

	Socket::Socket(int sock) :
			m_sock(sock)
	{
	}

	Socket::~Socket()
	{
		::close(m_sock);
	}

	size_t Socket::send(const void* buf, size_t len, int& err, const OOBase::timeval_t* timeout)
	{
		err = 0;
		const char* cbuf = static_cast<const char*>(buf);

		::timeval tv;
		if (timeout)
		{
			tv.tv_sec = static_cast<long>(timeout->tv_sec());
			tv.tv_usec = timeout->tv_usec();
		}

		fd_set wfds;
		fd_set efds;

		size_t total = 0;
		while (total < len)
		{
			ssize_t sent = ::send(m_sock,cbuf,static_cast<int>(len),0);
			if (sent != -1)
			{
				total += sent;
				cbuf += sent;
			}
			else
			{
				err = errno;
				if (err == EINTR)
					continue;
				else if (err != EAGAIN)
					break;

				// Do the select...
				FD_ZERO(&wfds);
				FD_ZERO(&efds);
				FD_SET(m_sock,&wfds);
				FD_SET(m_sock,&efds);
				int count = select(static_cast<int>(m_sock+1),NULL,&wfds,&efds,timeout ? &tv : NULL);
				if (count == -1)
				{
					err = errno;
					if (err != EINTR)
						break;
				}
				else if (count == 1)
				{
					if (FD_ISSET(m_sock,&efds))
					{
						// Error
						socklen_t len = sizeof(err);
						if (getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
							err = errno;

						break;
					}
				}
				else
				{
					err = ETIMEDOUT;
					break;
				}
			}
		}

		return total;
	}

	size_t Socket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::timeval_t* timeout)
	{
		err = 0;
		char* cbuf = static_cast<char*>(buf);

		::timeval tv;
		if (timeout)
		{
			tv.tv_sec = static_cast<long>(timeout->tv_sec());
			tv.tv_usec = timeout->tv_usec();
		}

		fd_set rfds;
		fd_set efds;

		size_t total = 0;
		for (;;)
		{
			ssize_t recvd = ::recv(m_sock,cbuf,static_cast<int>(len),0);
			if (recvd != -1)
			{
				err = 0;
				total += recvd;
				cbuf += recvd;
				len -= recvd;
				if (len == 0 || recvd == 0 || !bAll)
					break;
			}
			else
			{
				err = errno;
				if (err == EINTR)
					continue;
				else if (err != EAGAIN)
					break;

				// Do the select...
				FD_ZERO(&rfds);
				FD_ZERO(&efds);
				FD_SET(m_sock,&rfds);
				FD_SET(m_sock,&efds);
				int count = select(static_cast<int>(m_sock+1),&rfds,NULL,&efds,timeout ? &tv : NULL);
				if (count == -1)
				{
					err = errno;
					if (err != EINTR)
						break;
				}
				else if (count == 1)
				{
					if (FD_ISSET(m_sock,&efds))
					{
						// Error
						socklen_t len = sizeof(err);
						if (getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
							err = errno;

						break;
					}
				}
				else
				{
					err = ETIMEDOUT;
					break;
				}
			}
		}

		return total;
	}

	void Socket::close()
	{
		::shutdown(m_sock,SHUT_RDWR);
	}

	int connect_i(const char* address, const char* port, int& err, const OOBase::timeval_t* timeout)
	{
		// Start a countdown
		OOBase::timeval_t timeout2 = (timeout ? *timeout : OOBase::timeval_t::MaxTime);
		OOBase::Countdown countdown(&timeout2);

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
		countdown.update();
		if (timeout2 == OOBase::timeval_t::Zero)
			err = ETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				err = 0;
				sock = OOBase::BSD::create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,err);
				if (sock == -1)
					break;
				else
				{
					if ((err = OOBase::BSD::connect(sock,pAddr->ai_addr,pAddr->ai_addrlen,&timeout2)) != 0)
						::close(sock);
					else
						break;
				}

				countdown.update();
				if (timeout2 == OOBase::timeval_t::Zero)
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

void OOBase::POSIX::create_unix_socket(sockaddr_un& addr, socklen_t& len, const char* path)
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
	int sock = BSD::create_socket(AF_UNIX,SOCK_STREAM,0,err);
	if (sock == -1)
		return NULL;

	sockaddr_un addr;
	socklen_t addr_len = 0;
	POSIX::create_unix_socket(addr,addr_len,path);
	
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
