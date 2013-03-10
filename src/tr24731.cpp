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

#include "../include/OOBase/tr24731.h"

#if defined(HAVE_TR_24731) && (!defined(__STDC_LIB_EXT1__) || (__STDC_LIB_EXT1__ < 200509L))
int OOBase::vsnprintf_s_fixed(char* s, rsize_t n, const char* format, va_list arg)
{
	if (!s || !format)
	{
		errno = 22 /*EINVAL*/;
		return -1;
	}

	int r = _vsnprintf_s(s,n,_TRUNCATE,format,arg);
	if (r == -1 && errno == 0)
		return static_cast<int>(n) * 2;
	else
		return r;
}

int OOBase::snprintf_s_fixed(char* s, size_t n, const char* format, ...)
{
	va_list args;
	va_start(args,format);

	int ret = vsnprintf_s_fixed(s,n,format,args);

	va_end(args);

	return ret;
}
#endif

#if !defined(HAVE_TR_24731)

int vsnprintf_s(char* s, size_t n, const char* format, va_list arg)
{
	if (!s || !format || !n)
	{
		errno = EINVAL;
		return -1;
	}

	s[0] = '\0';
	int r = vsnprintf(s,n,format,arg);

	// MSVCRT returns -1 when truncating
	if (r == -1)
		r = n * 2;

	s[n-1] = '\0';
	return r;
}

int snprintf_s(char* s, size_t n, const char* format, ...)
{
	va_list args;
	va_start(args,format);

	int ret = vsnprintf_s(s,n,format,args);

	va_end(args);

	return ret;
}

#endif
