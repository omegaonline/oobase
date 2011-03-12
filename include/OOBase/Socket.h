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

#ifndef OOBASE_SOCKET_H_INCLUDED_
#define OOBASE_SOCKET_H_INCLUDED_

#include "Buffer.h"
#include "SmartPtr.h"

#if defined(_WIN32)
#include <winsock2.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <sys/socket.h>
#endif

namespace OOBase
{
	/** \typedef socket_t
	 *  The platform specific socket type.
	 */
#if defined(_WIN32)
	typedef SOCKET socket_t;
#else
	typedef int socket_t;
#endif

	class Socket
	{
	public:
		static OOBase::SmartPtr<OOBase::Socket> connect(const char* address, const char* port, int* perr, const timeval_t* timeout = 0);
		static OOBase::SmartPtr<OOBase::Socket> connect_local(const char* path, int* perr, const timeval_t* timeout = 0);

		virtual size_t send(const void* buf, size_t len, int* perr, const timeval_t* timeout = 0) = 0;
		virtual size_t send_v(Buffer* buffers[], size_t count, int* perr, const timeval_t* timeout = 0) = 0;
				
		template <typename T>
		int send(const T& val, const timeval_t* timeout = 0)
		{
			int err = 0;
			send(&val,sizeof(T),&err,timeout);
			return err;
		}

		int send(Buffer* buffer, const timeval_t* timeout = 0)
		{
			int err = 0;
			size_t len = send(buffer->rd_ptr(),buffer->length(),&err,timeout);
			buffer->rd_ptr(len);
			return err;
		}

		virtual size_t recv(void* buf, size_t len, bool bAll, int* perr, const timeval_t* timeout = 0) = 0;
		virtual size_t recv_v(Buffer* buffers[], size_t count, int* perr, const timeval_t* timeout = 0) = 0;

		template <typename T>
		int recv(T& val, const timeval_t* timeout = 0)
		{
			int err = 0;
			recv(&val,sizeof(T),true,&err,timeout);
			return err;
		}

		int recv(Buffer* buffer, bool bAll, const timeval_t* timeout = 0)
		{
			int err = 0;
			size_t len = recv(buffer->wr_ptr(),buffer->space(),bAll,&err,timeout);
			buffer->wr_ptr(len);
			return err;
		}

		virtual void shutdown(bool bSend, bool bRecv) = 0;

		virtual ~Socket() {};

	protected:
		Socket() {}

	private:
		Socket(const Socket&);
		Socket& operator = (const Socket&);
	};
}

#if !defined(INVALID_SOCKET)
#define INVALID_SOCKET (OOBase::socket_t(-1))
#endif

#endif // OOBASE_SOCKET_H_INCLUDED_
