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
#else
	// Autoconf
	#include <oobase-autoconf.h>
#endif

////////////////////////////////////////
// Bring in C standard libraries
//
#define __STDC_WANT_LIB_EXT1__ 1
#define __STDC_WANT_SECURE_LIB__ 1
#include <string.h>

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

////////////////////////////////////////
// Bring in C++ standard libraries
//
#ifdef __cplusplus
#include <string>
#include <sstream>
#include <iomanip>
#include <locale>
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
	#if defined (HAVE_PTHREADS_H) && !defined(_POSIX_THREADS)
	#error Your pthreads does not appears to be POSIX compliant?
	#endif
#endif

////////////////////////////////////////
// Byte-order (endian-ness) determination.
# if defined(BYTE_ORDER)
#   if (BYTE_ORDER == LITTLE_ENDIAN)
#     define OMEGA_LITTLE_ENDIAN 0x0123
#     define OMEGA_BYTE_ORDER OMEGA_LITTLE_ENDIAN
#   elif (BYTE_ORDER == BIG_ENDIAN)
#     define OMEGA_BIG_ENDIAN 0x3210
#     define OMEGA_BYTE_ORDER OMEGA_BIG_ENDIAN
#   else
#     error: unknown BYTE_ORDER!
#   endif /* BYTE_ORDER */
# elif defined(_BYTE_ORDER)
#   if (_BYTE_ORDER == _LITTLE_ENDIAN)
#     define OMEGA_LITTLE_ENDIAN 0x0123
#     define OMEGA_BYTE_ORDER OMEGA_LITTLE_ENDIAN
#   elif (_BYTE_ORDER == _BIG_ENDIAN)
#     define OMEGA_BIG_ENDIAN 0x3210
#     define OMEGA_BYTE_ORDER OMEGA_BIG_ENDIAN
#   else
#     error: unknown _BYTE_ORDER!
#   endif /* _BYTE_ORDER */
# elif defined(__BYTE_ORDER)
#   if (__BYTE_ORDER == __LITTLE_ENDIAN)
#     define OMEGA_LITTLE_ENDIAN 0x0123
#     define OMEGA_BYTE_ORDER OMEGA_LITTLE_ENDIAN
#   elif (__BYTE_ORDER == __BIG_ENDIAN)
#     define OMEGA_BIG_ENDIAN 0x3210
#     define OMEGA_BYTE_ORDER OMEGA_BIG_ENDIAN
#   else
#     error: unknown __BYTE_ORDER!
#   endif /* __BYTE_ORDER */
# else /* ! BYTE_ORDER && ! __BYTE_ORDER */
  // We weren't explicitly told, so we have to figure it out . . .
#   if defined(i386) || defined(__i386__) || defined(_M_IX86) || \
     defined(vax) || defined(__alpha) || defined(__LITTLE_ENDIAN__) ||\
     defined(ARM) || defined(_M_IA64) || \
     defined(_M_AMD64) || defined(__amd64)
    // We know these are little endian.
#     define OMEGA_LITTLE_ENDIAN 0x0123
#     define OMEGA_BYTE_ORDER OMEGA_LITTLE_ENDIAN
#   else
    // Otherwise, we assume big endian.
#     define OMEGA_BIG_ENDIAN 0x3210
#     define OMEGA_BYTE_ORDER OMEGA_BIG_ENDIAN
#   endif
# endif /* ! BYTE_ORDER && ! __BYTE_ORDER */

////////////////////////////////////////
// Forward declare custom error handling

#ifdef __cplusplus

#define OOBase_CallCriticalFailure(expr) \
	OOBase::CallCriticalFailure(__FILE__,__LINE__,expr)

#define OOBASE_NEW_T_CRITICAL(TYPE,POINTER,CONSTRUCTOR) \
	do { \
		void* OOBASE_NEW_ptr = ::OOBase::Allocate(sizeof(TYPE),0,__FILE__,__LINE__); \
		if (!OOBASE_NEW_ptr) { ::OOBase::CallCriticalFailureMem(__FILE__,__LINE__); } \
		try { POINTER = new (OOBASE_NEW_ptr) CONSTRUCTOR; } catch (...) { ::OOBase::Free(OOBASE_NEW_ptr,0); throw; } \
	} while ((void)0,false)

#define OOBASE_NEW_T2(TYPE,POINTER,CONSTRUCTOR) \
	POINTER = new (::OOBase::Allocate(sizeof(TYPE),0,__FILE__,__LINE__)) CONSTRUCTOR

#define OOBASE_NEW_T(TYPE,POINTER,CONSTRUCTOR) \
	do { \
		void* OOBASE_NEW_ptr = ::OOBase::Allocate(sizeof(TYPE),0,__FILE__,__LINE__); \
		try { POINTER = new (OOBASE_NEW_ptr) CONSTRUCTOR; } catch (...) { ::OOBase::Free(OOBASE_NEW_ptr,0); throw; } \
	} while ((void)0,false)

#define OOBASE_NEW_T_RETURN(TYPE,CONSTRUCTOR) \
	do { \
		void* OOBASE_NEW_ptr = ::OOBase::Allocate(sizeof(TYPE),0,__FILE__,__LINE__); \
		try { return new (OOBASE_NEW_ptr) CONSTRUCTOR; } catch (...) { ::OOBase::Free(OOBASE_NEW_ptr,0); throw; } \
	} while ((void)0,false)

#define OOBASE_DELETE(TYPE,POINTER) \
	do { \
		if (POINTER) \
		{ \
			POINTER->~TYPE(); \
			::OOBase::Free(POINTER,0); \
		} \
	} while ((void)0,false)

namespace OOBase
{
	void CallCriticalFailure(const char* pszFile, unsigned int nLine, const char*);
	void CallCriticalFailure(const char* pszFile, unsigned int nLine, int);

	std::string strerror(int err);
	std::string system_error_text(int err);

	// flags: 0 - C++ object - align to size, no reallocation
	//        1 - Buffer - align 32, reallocation
	//        2 - Stack-local buffer - align 32, no reallocation
	void* Allocate(size_t len, int flags, const char* file = 0, unsigned int line = 0);
	void Free(void* mem, int flags);

	void CallCriticalFailureMem(const char* pszFile, unsigned int nLine);

	template <typename T>
	class DeleteDestructor
	{
	public:
		static void destroy(T* ptr)
		{
			OOBASE_DELETE(T,ptr);
		}

		static void destroy_void(void* ptr)
		{
			destroy(static_cast<T*>(ptr));
		}
	};

	template <int T>
	class FreeDestructor
	{
	public:
		static void destroy(void* ptr)
		{
			::OOBase::Free(ptr,T);
		}
	};
}

#if !defined(HAVE_STATIC_ASSERT)
#define static_assert(expr,msg) \
	{ struct oobase_static_assert { char static_check[expr ? 1 : -1]; }; }
#endif

#endif

#endif // OOBASE_CONFIG_BASE_H_INCLUDED_
