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

#ifndef OOBASE_BSD_SOCKET_H_INCLUDED_
#define OOBASE_BSD_SOCKET_H_INCLUDED_

#include "Socket.h"

namespace OOBase
{
	namespace BSD
	{
		// Try to align winsock and unix ;)
#if defined(_WIN32)
		typedef SOCKET socket_t;
#else
		typedef int socket_t;
		#define INVALID_SOCKET (-1)
#endif

		class Socket : public OOBase::Socket
		{
		public:
			Socket(socket_t sock);
			virtual ~Socket();
			
			int send(const void* buf, size_t len, const timeval_t* timeout = 0);
			size_t recv(void* buf, size_t len, int* perr, const timeval_t* timeout = 0);
			void close();
			
			static socket_t create(int family, int socktype, int protocol, int* perr);
			static int connect(socket_t sock, const sockaddr* addr, size_t addrlen, const timeval_t* wait = 0);
			static socket_t connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait = 0);
			static int send(socket_t sock, const void* buf, size_t len, const timeval_t* wait = 0);
			static size_t recv(socket_t sock, void* buf, size_t len, int* perr, const timeval_t* wait = 0);
			static int close(socket_t sock);

			static int set_non_blocking(socket_t sock, bool set);
			static int set_close_on_exec(socket_t sock, bool set);

		private:
			socket_t  m_sock;
		};
	}

#if defined(HAVE_SYS_UN_H)
	namespace POSIX
	{
		class LocalSocket : public OOBase::LocalSocket
		{
		public:
			LocalSocket(int fd);

			int send(const void* buf, size_t len, const timeval_t* timeout = 0);
			size_t recv(void* buf, size_t len, int* perr, const timeval_t* timeout = 0);
			void close();

			OOBase::LocalSocket::uid_t get_uid();
		};
	}
#endif
}

#endif // OOBASE_BSD_SOCKET_H_INCLUDED_
