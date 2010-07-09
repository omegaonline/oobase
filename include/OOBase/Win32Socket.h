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

#ifndef OOBASE_WIN32_SOCKET_H_INCLUDED_
#define OOBASE_WIN32_SOCKET_H_INCLUDED_

#include "Socket.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		void WSAStartup();

		class Socket : public OOBase::Socket
		{
		public:
			Socket(SOCKET hSocket);
			virtual ~Socket();

			virtual int send(const void* buf, size_t len, const OOBase::timeval_t* wait = 0);
			virtual size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait = 0);
			virtual void close();
			
		private:
			SOCKET m_hSocket;

			int close_on_exec(bool /*set*/)
			{
				return 0;
			}
		};

		class LocalSocket : public OOBase::LocalSocket
		{
		public:
			LocalSocket(HANDLE hPipe);
			virtual ~LocalSocket();

			virtual int send(const void* buf, size_t len, const OOBase::timeval_t* wait = 0);
			virtual size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait = 0);
			virtual void close();
			virtual OOBase::LocalSocket::uid_t get_uid();

		private:
			SmartHandle m_hPipe;
			SmartHandle m_hReadEvent;
			SmartHandle m_hWriteEvent;

			int close_on_exec(bool /*set*/)
			{
				return 0;
			}
		};
	}
}

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_SOCKET_H_INCLUDED_
