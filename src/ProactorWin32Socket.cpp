///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "ProactorWin32.h"

#include "../include/OOBase/Allocator.h"

#if defined(_WIN32)

#include <Ws2tcpip.h>

namespace
{
	SOCKET create_socket(int family, int socktype, int protocol, int* perr)
	{
		*perr = 0;
		SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET)
			*perr = WSAGetLastError();

		return sock;
	}

	class SocketAcceptor : public OOBase::Socket
	{
	public:
		SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const char* strAddress, int err), int af_family);

		size_t send(const void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t send_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t recv(void* buf, size_t len, bool bAll, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t recv_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);
		void shutdown(bool bSend, bool bRecv);

		int accept();
	};

	class AsyncSocket : public OOSvrBase::AsyncSocket
	{
	public:
		static OOSvrBase::AsyncSocket* Create(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, int* perr);

		int recv(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, bool bAll);
		int send(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int recv_v(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);
		int send_v(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		void shutdown(bool bSend, bool bRecv);
	};
}

size_t SocketAcceptor::send(const void*, size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t SocketAcceptor::recv(void*, size_t, bool, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t SocketAcceptor::send_v(OOBase::Buffer*[], size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t SocketAcceptor::recv_v(OOBase::Buffer*[], size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

OOBase::SmartPtr<OOBase::Socket> OOSvrBase::Win32::ProactorImpl::accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const char* strAddress, int err), const char* address, const char* port, int* perr)
{
	*perr = 0;

	int af_family = AF_INET;
	OOBase::socket_t sock = INVALID_SOCKET;
	if (!address)
	{
		sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_addr.S_un.S_addr = ADDR_ANY;

		if (port)
			addr.sin_port = htons((u_short)atoi(port));

		if ((sock = create_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP,perr)) == INVALID_SOCKET)
			return 0;
		
		if (::bind(sock,(sockaddr*)&addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			*perr = WSAGetLastError();
			closesocket(sock);
			return 0;
		}
	}
	else
	{
		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* pResults = 0;
		if (::getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			*perr = WSAGetLastError();
			return 0;
		}

		// Loop trying to connect on each address until one succeeds
		for (addrinfo* pAddr = pResults; pAddr != 0; pAddr = pAddr->ai_next)
		{
			if ((sock = create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
			{
				if (::bind(sock,pAddr->ai_addr,pAddr->ai_addrlen) == SOCKET_ERROR)
				{
					*perr = WSAGetLastError();
					closesocket(sock);
				}
				else
				{
					af_family = pAddr->ai_family;
					break;
				}
			}
		}

		// Done with address info
		::freeaddrinfo(pResults);

		// Clear error
		if (sock != INVALID_SOCKET)
			*perr = 0;
	}

	// Now start the socket listening...
	if (::listen(sock,SOMAXCONN) == SOCKET_ERROR)
	{
		*perr = WSAGetLastError();
		closesocket(sock);
		return 0;
	}

	// Wrap up in a controlling socket class
	OOBase::SmartPtr<SocketAcceptor> pAcceptor;
	OOBASE_NEW_T(SocketAcceptor,pAcceptor,SocketAcceptor(this,sock,param,callback,af_family));
	if (!pAcceptor)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	*perr = pAcceptor->accept();
	if (*perr != 0)
		return 0;

	return pAcceptor.detach();
}

OOSvrBase::AsyncSocket* OOSvrBase::Win32::ProactorImpl::attach_socket(OOBase::socket_t sock, int* perr)
{
	// The socket must have been opened as WSA_FLAG_OVERLAPPED!!!
	return ::AsyncSocket::Create(this,sock,perr);
}

#endif // _WIN32
