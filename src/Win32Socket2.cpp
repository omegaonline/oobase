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

#include "../include/OOBase/internal/Win32Socket.h"
#include "../include/OOBase/Singleton.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		void WSAStartup();
	}
}

namespace
{
	class WSA
	{
	public:
		WSA();
		~WSA();
	};

	class Socket : public OOBase::Win32::Socket
	{
	public:
		Socket(SOCKET sock) : OOBase::Win32::Socket((HANDLE)sock)
		{}

		void shutdown(bool bSend, bool bRecv)
		{
			int how = -1;
			if (bSend)
				how = (bRecv ? SD_BOTH : SD_SEND);
			else if (bRecv)
				how = SD_RECEIVE;

			if (how != -1)
				::shutdown((SOCKET)(HANDLE)m_handle,how);
		}
	};
}

#if 0
OOBase::Socket* OOBase::Socket::connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	OOBase::SOCKET sock = OOBase::connect(address,port,perr,wait);
	if (sock == INVALID_SOCKET)
		return 0;

	OOBase::Win32::Socket* pSocket;
	OOBASE_NEW_T(OOBase::Win32::Socket,pSocket,OOBase::Win32::Socket(sock));
	if (!pSocket)
	{
		*perr = ERROR_OUTOFMEMORY;
		closesocket(sock);
		return 0;
	}

	return pSocket;
}
#endif

WSA::WSA()
{
	// Start Winsock
	WSADATA wsa;
	int err = WSAStartup(MAKEWORD(2,2),&wsa);
	if (err != 0)
		OOBase_CallCriticalFailure(WSAGetLastError());
	
	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
		OOBase_CallCriticalFailure("Very old Winsock dll");
}

WSA::~WSA()
{
	WSACleanup();
}

void OOBase::Win32::WSAStartup()
{
	OOBase::Singleton<WSA>::instance();
}

#endif // _WIN32
