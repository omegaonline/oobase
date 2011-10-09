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

#include "../include/OOBase/Socket.h"

#if defined(HAVE_UNISTD_H)
#include <sys/un.h>
#endif

namespace OOBase
{
	namespace BSD
	{
		int connect(socket_t sock, const sockaddr* addr, size_t addrlen, const OOBase::Countdown& countdown = OOBase::Countdown());

		int set_non_blocking(socket_t sock, bool set);
	}

#if defined(HAVE_UNISTD_H)
	namespace POSIX
	{
		void create_unix_socket_address(sockaddr_un& addr, socklen_t& len, const char* path);

		int get_peer_uid(int fd, uid_t& uid);
	}
#endif
}

#endif // OOBASE_BSD_SOCKET_H_INCLUDED_
