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

#include "../include/OOBase/Win32Socket.h"
#include "../include/OOBase/PosixSocket.h"

#if defined(_MSC_VER)
#pragma warning(disable: 4127)
#endif

#if defined(HAVE_NETDB_H)
#include <netdb.h>
#endif

#if defined(_WIN32)
#include <ws2tcpip.h>

#define ETIMEDOUT WSAETIMEDOUT
#define EINPROGRESS WSAEWOULDBLOCK
#define ssize_t int

#else

#define WSAGetLastError() (errno)
#define closesocket(fd) ::close(fd)

#define WSAEWOULDBLOCK EAGAIN

#endif

OOBase::SOCKET OOBase::socket(int family, int socktype, int protocol, int* perr)
{
	*perr = 0;

	OOBase::SOCKET sock = ::socket(family,socktype,protocol);
	if (sock == INVALID_SOCKET)
	{
		*perr = WSAGetLastError();
		return sock;
	}

#if defined(_WIN32)
	u_long v = 1;
	if (ioctlsocket(sock,FIONBIO,&v) == -1)
	{
		*perr = WSAGetLastError();
		closesocket(sock);
		return INVALID_SOCKET;
	}
#else
	if ((*perr = POSIX::fcntl_addfl(sock,O_NONBLOCK)) != 0)
	{
		close(sock);
		return INVALID_SOCKET;
	}
#endif

	return sock;
}

int OOBase::connect(SOCKET sock, const sockaddr* addr, size_t addrlen, const timeval_t* wait)
{
	// Do the connect
	if (::connect(sock,addr,addrlen) != -1)
		return 0;

	// Check to see if we actually have an error
	int err = WSAGetLastError();
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

		int count = select(sock+1,0,&wfds,&efds,wait ? &tv : 0);
		if (count == -1)
		{
			err = WSAGetLastError();
			if (err != EINTR)
				return err;
		}
		else if (count == 1)
		{
			// If connect() did something...
			socklen_t len = sizeof(err);
			if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
				return WSAGetLastError();

			return err;
		}
		else
			return ETIMEDOUT;
	}
}

OOBase::SOCKET OOBase::connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait)
{
	// Start a countdown
	timeval_t wait2 = (wait ? *wait : timeval_t::MaxTime);
	Countdown countdown(&wait2);

	// Resolve the passed in addresses...
	addrinfo hints = {0};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* pResults = 0;
	if (getaddrinfo(address.c_str(),port.c_str(),&hints,&pResults) != 0)
	{
		*perr = WSAGetLastError();
		return INVALID_SOCKET;
	}

	// Took too long to resolve...
	countdown.update();
	if (wait2 == timeval_t::Zero)
		return ETIMEDOUT;

	// Loop trying to connect on each address until one succeeds
	OOBase::SOCKET sock = INVALID_SOCKET;
	for (addrinfo* pAddr = pResults; pAddr->ai_next != 0; pAddr = pAddr->ai_next)
	{
		if ((sock = OOBase::socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
		{
			if ((*perr = OOBase::connect(sock,pAddr->ai_addr,pAddr->ai_addrlen,&wait2)) != 0)
				closesocket(sock);
			else
				break;
		}

		countdown.update();
		if (wait2 == timeval_t::Zero)
			return ETIMEDOUT;
	}

	// Done with address info
	freeaddrinfo(pResults);

	// Clear error
	if (sock != INVALID_SOCKET)
		*perr = 0;

	return sock;
}

int OOBase::send(SOCKET sock, const void* buf, size_t len, const timeval_t* wait)
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
		ssize_t sent = ::send(sock,cbuf,len,0);
		if (sent != -1)
		{
			len -= sent;
			cbuf += sent;
			if (len == 0)
				return 0;
		}
		else
		{
			int err = WSAGetLastError();
			if (err == EINTR)
				continue;
			else if (err != WSAEWOULDBLOCK)
				return err;

			// Do the select...
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			FD_SET(sock,&wfds);
			FD_SET(sock,&efds);
			int count = select(sock+1,0,&wfds,&efds,wait ? &tv : 0);
			if (count == -1)
			{
				err = WSAGetLastError();
				if (err != EINTR)
					return err;
			}
			else if (count == 1)
			{
				if (FD_ISSET(sock,&efds))
				{
					// Error
					socklen_t len = sizeof(err);
					if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
						return WSAGetLastError();

					return err;
				}
			}
			else
				return ETIMEDOUT;
		}
	}
}

size_t OOBase::recv(SOCKET sock, void* buf, size_t len, int* perr, const timeval_t* wait)
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
		ssize_t recvd = ::recv(sock,cbuf,len,0);
		if (recvd != -1)
		{
			total_recv += recvd;
			cbuf += recvd;
			len -= recvd;
			if (len == 0)
				return total_recv;
		}
		else
		{
			*perr = WSAGetLastError();
			if (*perr == EINTR)
				continue;
			else if (*perr != WSAEWOULDBLOCK)
				return total_recv;

			// Do the select...
			FD_ZERO(&rfds);
			FD_ZERO(&efds);
			FD_SET(sock,&rfds);
			FD_SET(sock,&efds);
			int count = select(sock+1,&rfds,0,&efds,wait ? &tv : 0);
			if (count == -1)
			{
				*perr = WSAGetLastError();
				if (*perr != EINTR)
					return total_recv;
			}
			else if (count == 1)
			{
				if (FD_ISSET(sock,&efds))
				{
					// Error
					socklen_t len = sizeof(*perr);
					if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)perr,&len) == -1)
						*perr = WSAGetLastError();

					return total_recv;
				}
			}
			else
				return total_recv;
		}
	}
}
