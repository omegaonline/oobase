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

#if defined(HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#endif

// Try to align winsock and BSD ;)
#if !defined(_WIN32)
#define INVALID_SOCKET (-1)
#endif

namespace OOBase
{
	class Socket
	{
	public:
		/** \typedef socket_t
		 *  The platform specific socket type.
		 */
#if defined(_WIN32)
		typedef SOCKET socket_t;
#else
		typedef int socket_t;
#endif

	public:
		virtual size_t recv(void* buf, size_t len, int* perr, const timeval_t* timeout = 0) = 0;
		virtual int send(const void* buf, size_t len, const timeval_t* timeout = 0) = 0;
		virtual void shutdown() = 0;
				
		static Socket* connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait = 0);
		static Socket* connect_local(const std::string& path, int* perr, const timeval_t* wait = 0);

		// Helpers
		template <typename T>
		int send(const T& val, const timeval_t* timeout = 0)
		{
			return send(&val,sizeof(T),timeout);
		}

		int send(const Buffer* buffer, const timeval_t* timeout = 0)
		{
			return send(buffer->rd_ptr(),buffer->length(),timeout);
		}

		template <typename T>
		int recv(T& val, const timeval_t* timeout = 0)
		{
			int err = 0;
			recv(&val,sizeof(T),&err,timeout);
			return err;
		}

		int recv(Buffer* buffer, const timeval_t* timeout = 0)
		{
			return recv(buffer,0,timeout);
		}

		virtual int recv(Buffer* buffer, size_t len, const timeval_t* timeout = 0)
		{
			int err = buffer->space(len);
			if (err == 0)
			{
				len = recv(buffer->wr_ptr(),len,&err,timeout);
				if (err == 0)
					buffer->wr_ptr(len);
			}
			return err;
		}

		virtual ~Socket() {};

	protected:
		Socket() {}
		
	private:
		Socket(const Socket&);
		Socket& operator = (const Socket&);
	};
}

#endif // OOBASE_SOCKET_H_INCLUDED_
