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

////////////////////////////////////////
// Try to work out what's going on with MS Windows
#if defined(HAVE_WINDOWS_H)
	#if !defined(_WIN32)
	#error No _WIN32 ?!?
	#endif

	// Prevent inclusion of old winsock
	#define _WINSOCKAPI_

	// Reduce the amount of windows we include
	#define WIN32_LEAN_AND_MEAN
	#define STRICT

	// We support Vista API's
	#if !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0600
	#elif _WIN32_WINNT < 0x0500
	#error OOBase requires _WIN32_WINNT >= 0x0500!
	#endif

	// We require IE 5 or later
	#if !defined(_WIN32_IE)
	#define _WIN32_IE 0x0500
	#elif _WIN32_IE < 0x0500
	#error OOBase requires _WIN32_IE >= 0x0500!
	#endif

	#include <windows.h>

	#if !defined(WINVER)
	#error No WINVER?!?
	#elif (WINVER < 0x0500)
	#if defined(__MINGW32__)
	// MinGW gets WINVER wrong...
	#undef WINVER
	#define WINVER 0x0500
	#else
	#error OOBase requires WINVER >= 0x0500!
	#endif
	#endif

	// Check for obsolete windows versions
	#if defined(_WIN32_WINDOWS)
	#error You cannot build Omega Online for Windows 95/98/Me!
	#endif

	// Remove the unistd include - we are windows
	#if defined(HAVE_UNISTD_H)
	#undef HAVE_UNISTD_H
	#endif

#endif // HAVE_WINDOWS_H

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

////////////////////////////////////////
// Some standard headers.
#include <assert.h>
#include <errno.h>
#include <stdarg.h>

////////////////////////////////////////
// Define some standard errors to avoid lots of #ifdefs.

#if !defined(ERROR_OUTOFMEMORY) && defined(ENOMEM) && !defined(DOXYGEN)
#define ERROR_OUTOFMEMORY ENOMEM
#endif

////////////////////////////////////////
// Forward declare custom error handling

#ifdef __cplusplus

/// Call OOBase::CallCriticalFailure passing __FILE__ and __LINE__
#define OOBase_CallCriticalFailure(expr) \
	OOBase::CallCriticalFailure(__FILE__,__LINE__,expr)

#if !defined(OOBASE_NORETURN)
#define OOBASE_NORETURN
#endif

namespace OOBase
{
	void OOBASE_NORETURN CallCriticalFailure(const char* pszFile, unsigned int nLine, const char*);
	void OOBASE_NORETURN CallCriticalFailure(const char* pszFile, unsigned int nLine, int);

	/// Return the system supplied error string from error code 'err' . If err == -1, use errno or GetLastError()
	const char* system_error_text(int err = -1);
	int stderr_write(const char* sz, size_t len = -1);
	int stdout_write(const char* sz, size_t len = -1);

	typedef bool (*OnCriticalFailure)(const char*);

	/// Override the default critical failure behaviour
	OnCriticalFailure SetCriticalFailure(OnCriticalFailure pfn);
}

#if defined(DOXYGEN)
/// Compile time assertion, assert that expr == true
#define static_assert(expr,msg)
#elif !defined(HAVE_STATIC_ASSERT)
#define static_assert(expr,msg) \
	{ struct oobase_static_assert { char static_check[expr ? 1 : -1]; }; }
#endif

#endif

#endif // OOBASE_CONFIG_BASE_H_INCLUDED_
