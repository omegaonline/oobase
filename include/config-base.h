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

#ifndef OOBASE_CONFIG_BASE_H_INCLUDED_
#define OOBASE_CONFIG_BASE_H_INCLUDED_

#if defined(_MSC_VER)
	#include "oobase-msvc.h"
#elif !defined(DOXYGEN)
	// Autoconf
	#include <oobase-autoconf.h>
#endif

#include "OOBase/Base.h"

#if defined(HAVE_WINDOWS_H)
	#if !defined(_WIN32)
	#error No _WIN32 ?!?
	#endif
#endif

#if defined(_WIN32)
	// Remove the unistd include - we are windows
	#if defined(HAVE_UNISTD_H)
	#undef HAVE_UNISTD_H
	#endif
#endif

////////////////////////////////////////
// Bring in POSIX if possible
#if defined(HAVE_UNISTD_H)
	#include <unistd.h>

	// check for POSIX.1 IEEE 1003.1
	#if !defined(_POSIX_VERSION)
	#error <unistd.h> is not POSIX compliant?
	#endif

	// Check pthreads
	#if defined (HAVE_PTHREAD)
	#include <pthread.h>
	#if !defined(_POSIX_THREADS)
	#error Your pthreads does not appears to be POSIX compliant?
	#endif
	#endif
#endif

#endif // OOBASE_CONFIG_BASE_H_INCLUDED_
