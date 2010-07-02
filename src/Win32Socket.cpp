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
#include "../include/OOBase/Singleton.h"

#if defined(_WIN32)

namespace
{
	struct WSAOnce
	{
		WSAOnce()
		{
			WSADATA wsa;
			int err = WSAStartup(MAKEWORD(2,2),&wsa);
			if (err != 0)
				OOBase_CallCriticalFailure(WSAGetLastError());
			
			if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
				OOBase_CallCriticalFailure("Very old Winsock dll");
		}

		~WSAOnce()
		{
			WSACleanup();
		}
	};
}

void OOBase::Win32::WSAStartup()
{
	// Use a singleton to init once
	Singleton<WSAOnce,WSAOnce>::instance();
}

OOBase::Socket* OOBase::Socket::connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	OOBase::SOCKET sock = OOBase::connect(address,port,perr,wait);
	if (sock == INVALID_SOCKET)
		return 0;

	OOBase::Win32::Socket* pSocket = 0;
	OOBASE_NEW(pSocket,OOBase::Win32::Socket(sock));
	if (!pSocket)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	return pSocket;
}

#endif // _WIN32
