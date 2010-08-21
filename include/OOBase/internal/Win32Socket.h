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

#include "../Socket.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		class Socket : public OOBase::Socket
		{
		public:
			Socket(HANDLE handle);
			virtual ~Socket();

			size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait = 0);
			int send(const void* buf, size_t len, const OOBase::timeval_t* wait = 0);
						
		protected:
			OOBase::Win32::SmartHandle m_handle;
			OOBase::Win32::SmartHandle m_hReadEvent;
			OOBase::Win32::SmartHandle m_hWriteEvent;
		};
	}
}

#endif // _WIN32

#endif // OOBASE_WIN32_SOCKET_H_INCLUDED_
