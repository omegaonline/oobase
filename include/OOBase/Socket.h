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
#include "Timeout.h"

#if defined(_WIN32)

#if defined(__MINGW32__) && defined(_WINSOCKAPI_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 6386)
#endif

#include <ws2tcpip.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
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

	namespace Net
	{
		socket_t open_socket(int family, int type, int protocol, int& err);
		int close_socket(socket_t sock);
		int bind(socket_t sock, const sockaddr* addr, socklen_t addr_len);
		int connect(socket_t sock, const sockaddr* addr, socklen_t addrlen, const Timeout& timeout = Timeout());
		int accept(socket_t accept_sock, socket_t& new_sock, const Timeout& timeout = Timeout());

#if defined(_WIN32)
		int accept(HANDLE hPipe, const Timeout& timeout = Timeout());
#endif
	}

	class Socket : public RefCounted
	{
	public:
		static Socket* attach(socket_t sock, int& err);

#if defined(_WIN32)
		static Socket* attach(HANDLE hPipe, int& err);
#endif

		static Socket* connect(const char* address, const char* port, int& err, const Timeout& timeout = Timeout());
		static Socket* connect(const char* path, int& err, const Timeout& timeout = Timeout());

		virtual size_t send(const void* buf, size_t len, int& err, const Timeout& timeout = Timeout()) = 0;
		virtual int send_v(Buffer* buffers[], size_t count, const Timeout& timeout = Timeout()) = 0;
		virtual size_t send_msg(const void* data_buf, size_t data_len, const void* ctl_buf, size_t ctl_len, int& err, const Timeout& timeout = Timeout()) = 0;
				
		template <typename T>
		int send(const T& val, const Timeout& timeout = Timeout())
		{
			int err = 0;
			send(&val,sizeof(T),err,timeout);
			return err;
		}

		// Do not send pointers!
		template <typename T>
		int send(T*, const Timeout& timeout = Timeout());
		
		int send(Buffer* buffer, const Timeout& timeout = Timeout())
		{
			if (!buffer)
				return EINVAL;

			int err = 0;
			size_t len = send(buffer->rd_ptr(),buffer->length(),err,timeout);
			buffer->rd_ptr(len);
			return err;
		}

		virtual size_t recv(void* buf, size_t len, bool bAll, int& err, const Timeout& timeout = Timeout()) = 0;
		virtual int recv_v(Buffer* buffers[], size_t count, const Timeout& timeout = Timeout()) = 0;
		virtual size_t recv_msg(void* data_buf, size_t data_len, void* ctl_buf, size_t ctl_len, int& err, const Timeout& timeout = Timeout()) = 0;

		template <typename T>
		int recv(T& val, const Timeout& timeout = Timeout())
		{
			int err = 0;
			recv(&val,sizeof(T),true,err,timeout);
			return err;
		}

		// Do not recv pointers!
		template <typename T>
		int recv(T*, const Timeout& timeout = Timeout());

		template <typename T>
		int recv(RefPtr<T>& val, const Timeout& timeout = Timeout())
		{
			return recv(static_cast<T*>(val),timeout);
		}

		template <typename T>
		int recv(RefPtr<T>& val, size_t len, const Timeout& timeout = Timeout())
		{
			return recv(static_cast<T*>(val),len,timeout);
		}

		template <typename T, typename D>
		size_t recv(SmartPtr<T,D>& buf, size_t len, bool bAll, int& err, const Timeout& timeout = Timeout())
		{
			return recv(static_cast<T*>(buf),len,bAll,err,timeout);
		}

		int recv(Buffer* buffer, const Timeout& timeout = Timeout())
		{
			if (!buffer)
				return EINVAL;

			int err = 0;
			size_t len = recv(buffer->wr_ptr(),buffer->space(),false,err,timeout);
			buffer->wr_ptr(len);
			return err;
		}

		int recv(Buffer* buffer, size_t len, const Timeout& timeout = Timeout())
		{
			if (!buffer)
				return EINVAL;

			if (len == 0)
				return 0;

			int err = buffer->space(len);
			if (err == 0)
			{
				size_t len2 = recv(buffer->wr_ptr(),len,true,err,timeout);
				buffer->wr_ptr(len2);
			}
			return err;
		}

		virtual void close() = 0;
	};
}

#endif // OOBASE_SOCKET_H_INCLUDED_
