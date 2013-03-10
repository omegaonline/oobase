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

#ifndef OOBASE_TR24731_H_INCLUDED_
#define OOBASE_TR24731_H_INCLUDED_

#include "../config-base.h"

#if !defined(DOXYGEN)
#define __STDC_WANT_LIB_EXT1__ 1

#if !defined(__STDC_WANT_SECURE_LIB__)
#define __STDC_WANT_SECURE_LIB__ 1
#endif
#endif

#include <stdio.h>

#if !defined(HAVE_TR_24731) && !defined(DOXYGEN)
#if (defined(__STDC_LIB_EXT1__) && (__STDC_LIB_EXT1__ >= 200509L)) || (defined(__STDC_SECURE_LIB__) && (__STDC_SECURE_LIB__ >= 200411L))
#define HAVE_TR_24731 1
#endif
#endif

// These are missing from the earlier draft...
#if defined(HAVE_TR_24731) && (!defined(__STDC_LIB_EXT1__) || (__STDC_LIB_EXT1__ < 200509L))

#if !defined(_MSC_VER)
#define rsize_t size_t
#endif

namespace OOBase
{
	int vsnprintf_s_fixed(char* s, rsize_t n, const char* format, va_list arg);
	int snprintf_s_fixed(char* s, size_t n, const char* format, ...);
}
#define vsnprintf_s OOBase::vsnprintf_s_fixed
#define snprintf_s OOBase::snprintf_s_fixed
#endif

#if !defined(HAVE_TR_24731)
int vsnprintf_s(char* s, size_t n, const char* format, va_list arg);
int snprintf_s(char* s, size_t n, const char* format, ...);
#endif

#endif // OOBASE_TR24731_H_INCLUDED_
