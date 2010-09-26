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

#include "../include/OOBase/Posix.h"

#if defined (HAVE_UNISTD_H)

#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif /* HAVE_FCNTL_H */

#if defined(HAVE_SYS_FCNTL_H)
#include <sys/fcntl.h>
#endif /* HAVE_SYS_FCNTL_H */

int OOBase::POSIX::set_close_on_exec(int sock, bool set)
{
#if (defined(HAVE_FCNTL_H) || defined(HAVE_SYS_FCNTL_H))
	int flags = fcntl(sock,F_GETFD);
	if (flags == -1)
		return errno;

	flags = (set ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC);
	if (fcntl(sock,F_GETFD,flags) == -1)
		return errno;
#else
	(void)set;
	(void)sock;
#endif

	return 0;
}

#endif
