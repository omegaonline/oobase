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

#include "../include/OOBase/internal/BSDSocket.h"
#include "../include/OOBase/Posix.h"

#if defined(_WIN32)
namespace OOBase
{
	namespace Win32
	{
		void WSAStartup();
	}
}
#endif

#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>
#endif

#if defined(_WIN32)
#include <ws2tcpip.h>

#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define ETIMEDOUT WSAETIMEDOUT
#define EINPROGRESS WSAEWOULDBLOCK
#define ssize_t int
#define socket_errno WSAGetLastError()
#define SOCKET_WOULD_BLOCK WSAEWOULDBLOCK

#else

#define socket_errno (errno)
#define SOCKET_WOULD_BLOCK EAGAIN
#define closesocket(s) ::close(s)

#endif

namespace
{
	class Socket : public OOBase::Socket
	{
	public:
		Socket(OOBase::Socket::socket_t sock);
		virtual ~Socket();

		int send(const void* buf, size_t len, const OOBase::timeval_t* timeout = 0);
		size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0);
		void shutdown(bool bSend, bool bRecv);

	private:
		OOBase::Socket::socket_t  m_sock;
	};

	Socket::Socket(OOBase::Socket::socket_t sock) :
			m_sock(sock)
	{
	}

	Socket::~Socket()
	{
		closesocket(m_sock);
	}

	int Socket::send(const void* buf, size_t len, const OOBase::timeval_t* wait)
	{
		const char* cbuf = static_cast<const char*>(buf);

		::timeval tv;
		if (wait)
		{
			tv.tv_sec = static_cast<long>(wait->tv_sec());
			tv.tv_usec = wait->tv_usec();
		}

		fd_set wfds;
		fd_set efds;
		for (;;)
		{
			ssize_t sent = ::send(m_sock,cbuf,static_cast<int>(len),0);
			if (sent != -1)
			{
				len -= sent;
				cbuf += sent;
				if (len == 0)
					return 0;
			}
			else
			{
				int err = socket_errno;
				if (err == EINTR)
					continue;
				else if (err != SOCKET_WOULD_BLOCK)
					return err;

				// Do the select...
				FD_ZERO(&wfds);
				FD_ZERO(&efds);
				FD_SET(m_sock,&wfds);
				FD_SET(m_sock,&efds);
				int count = select(static_cast<int>(m_sock+1),0,&wfds,&efds,wait ? &tv : 0);
				if (count == -1)
				{
					err = socket_errno;
					if (err != EINTR)
						return err;
				}
				else if (count == 1)
				{
					if (FD_ISSET(m_sock,&efds))
					{
						// Error
						socklen_t len = sizeof(err);
						if (getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
							return socket_errno;

						return err;
					}
				}
				else
					return ETIMEDOUT;
			}
		}
	}

	size_t Socket::recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait)
	{
		char* cbuf = static_cast<char*>(buf);
		size_t total_recv = 0;

		::timeval tv;
		if (wait)
		{
			tv.tv_sec = static_cast<long>(wait->tv_sec());
			tv.tv_usec = wait->tv_usec();
		}

		fd_set rfds;
		fd_set efds;
		for (;;)
		{
			ssize_t recvd = ::recv(m_sock,cbuf,static_cast<int>(len),0);
			if (recvd != -1)
			{
				*perr = 0;
				total_recv += recvd;
				cbuf += recvd;
				len -= recvd;
				if (len == 0 || recvd == 0)
					return total_recv;
			}
			else
			{
				*perr = socket_errno;
				if (*perr == EINTR)
					continue;
				else if (*perr != SOCKET_WOULD_BLOCK)
					return total_recv;

				// Do the select...
				FD_ZERO(&rfds);
				FD_ZERO(&efds);
				FD_SET(m_sock,&rfds);
				FD_SET(m_sock,&efds);
				int count = select(static_cast<int>(m_sock+1),&rfds,0,&efds,wait ? &tv : 0);
				if (count == -1)
				{
					*perr = socket_errno;
					if (*perr != EINTR)
						return total_recv;
				}
				else if (count == 1)
				{
					if (FD_ISSET(m_sock,&efds))
					{
						// Error
						socklen_t len = sizeof(*perr);
						if (getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)perr,&len) == -1)
							*perr = socket_errno;

						return total_recv;
					}
				}
				else
					return total_recv;
			}
		}
	}

	void Socket::shutdown(bool bSend, bool bRecv)
	{
		int how = -1;
		if (bSend)
			how = (bRecv ? SHUT_RDWR : SHUT_WR);
		else if (bRecv)
			how = SHUT_RD;

		if (how != -1)
			::shutdown(m_sock,how);
	}

	int connect_i(OOBase::Socket::socket_t sock, const sockaddr* addr, size_t addrlen, const OOBase::timeval_t* wait)
	{
		// Do the connect
		if (::connect(sock,addr,static_cast<int>(addrlen)) != -1)
			return 0;

		// Check to see if we actually have an error
		int err = socket_errno;
		if (err != EINPROGRESS)
			return err;

		// Wait for the connect to go ahead - by select()ing on write
		::timeval tv;
		if (wait)
		{
			tv.tv_sec = static_cast<long>(wait->tv_sec());
			tv.tv_usec = wait->tv_usec();
		}

		fd_set wfds;
		fd_set efds; // Annoyingly Winsock uses the exceptions not just the writes

		for (;;)
		{
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			FD_SET(sock,&wfds);
			FD_SET(sock,&efds);

			int count = select(static_cast<int>(sock+1),0,&wfds,&efds,wait ? &tv : 0);
			if (count == -1)
			{
				err = socket_errno;
				if (err != EINTR)
					return err;
			}
			else if (count == 1)
			{
				// If connect() did something...
				socklen_t len = sizeof(err);
				if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
					return socket_errno;

				return err;
			}
			else
				return ETIMEDOUT;
		}
	}

	OOBase::Socket::socket_t connect_i(const char* address, const char* port, int* perr, const OOBase::timeval_t* wait)
	{
		// Start a countdown
		OOBase::timeval_t wait2 = (wait ? *wait : OOBase::timeval_t::MaxTime);
		OOBase::Countdown countdown(&wait2);

		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = 0;
		if (getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			*perr = socket_errno;
			return INVALID_SOCKET;
		}

		// Took too long to resolve...
		countdown.update();
		if (wait2 == OOBase::timeval_t::Zero)
			return ETIMEDOUT;

		// Loop trying to connect on each address until one succeeds
		OOBase::Socket::socket_t sock = INVALID_SOCKET;
		for (addrinfo* pAddr = pResults; pAddr != 0; pAddr = pAddr->ai_next)
		{
			if ((sock = OOBase::BSD::create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
			{
				if ((*perr = connect_i(sock,pAddr->ai_addr,pAddr->ai_addrlen,&wait2)) != 0)
					closesocket(sock);
				else
					break;
			}

			countdown.update();
			if (wait2 == OOBase::timeval_t::Zero)
				return ETIMEDOUT;
		}

		// Done with address info
		freeaddrinfo(pResults);

		// Clear error
		if (sock != INVALID_SOCKET)
			*perr = 0;

		return sock;
	}
}

OOBase::Socket::socket_t OOBase::BSD::create_socket(int family, int socktype, int protocol, int* perr)
{
#if defined(_WIN32)
	Win32::WSAStartup();
#endif

	*perr = 0;

	OOBase::Socket::socket_t sock = ::socket(family,socktype,protocol);
	if (sock == INVALID_SOCKET)
	{
		*perr = socket_errno;
		return sock;
	}

	*perr = OOBase::BSD::set_non_blocking(sock,true);
	if (*perr != 0)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}

#if defined (HAVE_UNISTD_H)
	*perr = OOBase::POSIX::set_close_on_exec(sock,true);
	if (*perr != 0)
	{
		closesocket(sock);
		return INVALID_SOCKET;
	}
#endif

	return sock;
}

int OOBase::BSD::set_non_blocking(Socket::socket_t sock, bool set)
{
#if defined (_WIN32)
	u_long v = (set ? 1 : 0);
	if (ioctlsocket(sock,FIONBIO,&v) == -1)
		return socket_errno;

#elif defined(HAVE_UNISTD_H)
	int flags = fcntl(sock,F_GETFL);
	if (flags == -1)
		return errno;

	flags = (set ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
	if (fcntl(sock,F_SETFL,flags) == -1)
		return errno;
#endif

	return 0;
}

// Win32 native sockets are probably faster...
//#if !defined(_WIN32)
OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int* perr, const timeval_t* wait)
{
#if defined(_WIN32)
	// Ensure we have winsock loaded
	Win32::WSAStartup();
#endif

	Socket::socket_t sock = connect_i(address,port,perr,wait);
	if (sock == INVALID_SOCKET)
		return 0;

	::Socket* pSocket;
	OOBASE_NEW_T(::Socket,pSocket,::Socket(sock));
	if (!pSocket)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	return pSocket;
}
//#endif

#if defined(HAVE_UNISTD_H)

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int* perr, const timeval_t* wait)
{
	OOBase::Socket::socket_t sock = OOBase::BSD::create_socket(AF_UNIX,SOCK_STREAM,0,perr);
	if (sock == -1)
		return 0;

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	memset(addr.sun_path,0,sizeof(addr.sun_path));
	path.copy(addr.sun_path,sizeof(addr.sun_path)-1);

	if ((*perr = connect_i(sock,(sockaddr*)(&addr),sizeof(addr),wait)) != 0)
	{
		::close(sock);
		return 0;
	}

	::Socket* pSocket;
	OOBASE_NEW_T(::Socket,pSocket,::Socket(sock));
	if (!pSocket)
	{
		*perr = ENOMEM;
		::close(sock);
		return 0;
	}

	return pSocket;
}

#endif // defined(HAVE_UNISTD_H)
