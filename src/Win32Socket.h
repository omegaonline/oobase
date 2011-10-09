///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#ifndef OOBASE_WIN32_SOCKET_H_INCLUDED_
#define OOBASE_WIN32_SOCKET_H_INCLUDED_

#include "../include/OOBase/Socket.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		void WSAStartup();

		BOOL WSAAcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped);
		void WSAGetAcceptExSockAddrs(SOCKET sListenSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, sockaddr **LocalSockaddr, int* LocalSockaddrLength, sockaddr **RemoteSockaddr, int* RemoteSockaddrLength);

		SOCKET create_socket(int family, int socktype, int protocol, int& err);
		int connect(SOCKET sock, const sockaddr* addr, socklen_t addrlen, const Countdown& countdown);
	}
}

#endif // _WIN32

#endif // OOBASE_WIN32_SOCKET_H_INCLUDED_
