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

#include "../Socket.h"

namespace OOBase
{
	namespace BSD
	{
		// Try to align winsock and BSD ;)
#if defined(_WIN32)
		typedef SOCKET socket_t;
#else
		typedef int socket_t;
		#define INVALID_SOCKET (-1)
#endif

		socket_t create_socket(int family, int socktype, int protocol, int* perr);
		int set_non_blocking(socket_t sock, bool set);
		int set_close_on_exec(socket_t sock, bool set);
	}
}

#endif // OOBASE_BSD_SOCKET_H_INCLUDED_
