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
		using ::SOCKET;
#else
		typedef int SOCKET;
#endif

		class SocketImpl
		{
		public:
			SocketImpl(SOCKET fd);
			virtual ~SocketImpl();

			virtual int send(const void* buf, size_t len, const OOBase::timeval_t* wait = 0);
			virtual size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait = 0);
			virtual void close();
			virtual int close_on_exec(bool set);

		protected:
			SOCKET  m_fd;
		};

		template <class Base>
		class SocketTempl :
				public Base,
				public SocketImpl
		{
		public:
			SocketTempl(SOCKET fd) :
					SocketImpl(fd)
			{}

			virtual ~SocketTempl()
			{}

			int send(const void* buf, size_t len, const OOBase::timeval_t* timeout = 0)
			{
				return SocketImpl::send(buf,len,timeout);
			}

			size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0)
			{
				return SocketImpl::recv(buf,len,perr,timeout);
			}

			void close()
			{
				SocketImpl::close();
			}

			int close_on_exec(bool set)
			{
				return SocketImpl::close_on_exec(set);
			}

		private:
			SocketTempl(const SocketTempl&) {}
			SocketTempl& operator = (const SocketTempl&) { return *this; }
		};

		class Socket :
				public SocketTempl<OOBase::Socket>
		{
		public:
			Socket(SOCKET fd) :
					SocketTempl<OOBase::Socket>(fd)
			{}
		};

		// Helpers for timed socket operations - All sockets are expected to be non-blocking!
		//
		// WARNING - THIS IS ALL MOVING WHEN THE EV PROACTOR STUFF IS TESTED PROPERLY!
		//
		SOCKET socket(int family, int socktype, int protocol, int* perr);
		int connect(SOCKET sock, const sockaddr* addr, size_t addrlen, const timeval_t* wait = 0);
		SOCKET connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait = 0);
		int send(SOCKET sock, const void* buf, size_t len, const timeval_t* wait = 0);
		size_t recv(SOCKET sock, void* buf, size_t len, int* perr, const timeval_t* wait = 0);

		int set_non_blocking(SOCKET sock, bool set);
		int set_close_on_exec(SOCKET sock, bool set);
	}

#if defined(HAVE_UNISTD_H)
	namespace POSIX
	{
		class LocalSocket :
				public SocketTempl<OOBase::LocalSocket>
		{
		public:
			LocalSocket(int fd) : SocketTempl<OOBase::LocalSocket>(fd)
			{}

			virtual OOBase::LocalSocket::uid_t get_uid();
		};
	}
#endif
}

#endif // OOBASE_BSD_SOCKET_H_INCLUDED_
