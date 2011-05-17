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

#include "../include/OOBase/utf8.h"
#include "../include/OOBase/SmartPtr.h"

#if defined(_WIN32)

size_t OOBase::measure_utf8(const char* sz, size_t len)
{
	if (!sz)
		return (len == size_t(-1) ? 1 : 0);

	int actual_len = MultiByteToWideChar(CP_UTF8,0,sz,static_cast<int>(len),NULL,0);
	if (actual_len < 1)
		actual_len = 0;

	return static_cast<size_t>(actual_len);
}

size_t OOBase::from_utf8(wchar_t* wsz, size_t wlen, const char* sz, size_t len)
{
	if (!wsz || !wlen)
		return measure_utf8(sz,len);

	if (!sz)
	{
		if (len == size_t(-1))
		{
			*wsz = L'\0';
			return 1;
		}
		else
			return 0;
	}

	int actual_len = MultiByteToWideChar(CP_UTF8,0,sz,static_cast<int>(len),wsz,static_cast<int>(wlen));
	if (actual_len < 1)
	{
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_INSUFFICIENT_BUFFER)
			return measure_utf8(sz,len);

		actual_len = 0;
	}

	return static_cast<size_t>(actual_len);
}

size_t OOBase::measure_native(const char* sz, size_t len)
{
	if (!sz)
		return (len == size_t(-1) ? 1 : 0);

	int actual_len = MultiByteToWideChar(CP_THREAD_ACP,0,sz,static_cast<int>(len),NULL,0);
	if (actual_len < 1)
		actual_len = 0;

	return static_cast<size_t>(actual_len);
}

size_t OOBase::from_native(wchar_t* wsz, size_t wlen, const char* sz, size_t len)
{
	if (!wsz || !wlen)
		return measure_native(sz,len);

	if (!sz)
	{
		if (len == size_t(-1))
		{
			*wsz = L'\0';
			return 1;
		}
		else
			return 0;
	}

	int actual_len = MultiByteToWideChar(CP_THREAD_ACP,0,sz,static_cast<int>(len),wsz,static_cast<int>(wlen));
	if (actual_len < 1)
	{
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_INSUFFICIENT_BUFFER)
			return measure_native(sz,len);

		actual_len = 0;
	}

	return static_cast<size_t>(actual_len);
}

size_t OOBase::measure_utf8(const wchar_t* wsz, size_t len)
{
	if (!wsz)
		return (len == size_t(-1) ? 1 : 0);

	int actual_len = WideCharToMultiByte(CP_UTF8,0,wsz,static_cast<int>(len),NULL,0,NULL,NULL);
	if (actual_len < 1)
		actual_len = 0;

	return static_cast<size_t>(actual_len);
}

size_t OOBase::to_utf8(char* sz, size_t len, const wchar_t* wsz, size_t wlen)
{
	if (!sz || !len)
		return measure_utf8(wsz,wlen);

	if (!wsz)
	{
		if (wlen == size_t(-1))
		{
			*sz = '\0';
			return 1;
		}
		else
			return 0;
	}

	int actual_len = WideCharToMultiByte(CP_UTF8,0,wsz,static_cast<int>(wlen),sz,static_cast<int>(len),NULL,NULL);
	if (actual_len < 1)
	{
		DWORD err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER)
			return measure_utf8(wsz,wlen);

		actual_len = 0;
	}

	return static_cast<size_t>(actual_len);
}

size_t OOBase::measure_native(const wchar_t* wsz, size_t len)
{
	if (!wsz)
		return (len == size_t(-1) ? 1 : 0);

	int actual_len = WideCharToMultiByte(CP_THREAD_ACP,0,wsz,static_cast<int>(len),NULL,0,NULL,NULL);
	if (actual_len < 1)
		actual_len = 0;

	return static_cast<size_t>(actual_len);
}

size_t OOBase::to_native(char* sz, size_t len, const wchar_t* wsz, size_t wlen)
{
	if (!sz || !len)
		return measure_native(wsz,wlen);

	if (!wsz)
	{
		if (wlen == size_t(-1))
		{
			*sz = '\0';
			return 1;
		}
		else
			return 0;
	}

	int actual_len = WideCharToMultiByte(CP_THREAD_ACP,0,wsz,static_cast<int>(wlen),sz,static_cast<int>(len),NULL,NULL);
	if (actual_len < 1)
	{
		DWORD err = GetLastError();
		if (err == ERROR_INSUFFICIENT_BUFFER)
			return measure_native(wsz,wlen);

		actual_len = 0;
	}

	return static_cast<size_t>(actual_len);
}

#else

#include <wchar.h>
#include <stdlib.h>

namespace
{
	static const char utf8_data[256] =
	{
		// Key:
		//  0 = Invalid first byte
		//  1 = Single byte
		//  2 = 2 byte sequence
		//  3 = 3 byte sequence
		//  4 = 4 byte sequence
		
		// -1 = Continuation byte
		// -2 = Overlong 2 byte sequence
		// -3 = 3 byte overlong check (0x80..0x9F) as next byte fail
		// -4 = 3 byte reserved check (0xA0..0xBF) as next byte fail
		// -5 = 4 byte overlong check (0x80..0x8F) as next byte fail
		// -6 = 4 byte reserved check (0x90..0xBF) as next byte fail
		
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x00..0x0F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x10..0x1F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x20..0x2F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x30..0x3F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x40..0x4F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x50..0x5F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x60..0x6F
		 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x70..0x7F
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0x80..0x8F
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0x90..0x9F
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0xA0..0xAF
		-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 0xB0..0xBF
		-2,-2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xC0..0xCF
		 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xD0..0xDF
		-3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,-4, 3, 3,  // 0xE0..0xEF
		-5, 4, 4, 4,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // 0xF0..0xFF
	};

	static const unsigned int utf_subst_val = 0xFFFD;
}

size_t OOBase::measure_utf8(const char* start, size_t len)
{
	if (!start)
		return (len == size_t(-1) ? 1 : 0);

	const char* sz = start;
	size_t required_len = 0;
	unsigned char c;

	do
	{
		// Default to substitution value
		unsigned int wide_val = utf_subst_val;
		int l = 0;

		c = *sz++;
		
		switch (utf8_data[c])
		{
			case 1:
				wide_val = c;
				break;
			
			case 2: 
				wide_val = (c & 0x1F);
				l = 1;
				break;
			
			case -2:
				++sz;
				break;
				
			case 3:
				wide_val = (c & 0x0F);
				l = 2;
				break;
			
			case -3: 
				if (*sz >= 0x80 && *sz <= 0x9F)
					sz += 2;
				else
				{
					wide_val = (c & 0x0F);
					l = 2;
				}
				break;
					
			case -4:
				if (*sz >= 0xA0 && *sz <= 0xBF)
					sz += 2;
				else
				{
					wide_val = (c & 0x0F);
					l = 2;
				}
				break;
				
			case 4:
				wide_val = (c & 0x07);
				l = 3;
				break;
				
			case -5:
				if (*sz >= 0x80 && *sz <= 0x8F)
					sz += 3;
				else
				{
					wide_val = (c & 0x07);
					l = 3;
				}
				break;
			
			case -6:
				if (*sz >= 0x90 && *sz <= 0xBF)
					sz += 3;
				else
				{
					wide_val = (c & 0x07);
					l = 3;
				}
				break;		
			
			default:
				break;
		}
		
		while (l-- > 0)
		{
			c = *sz++;
			if (utf8_data[c] != -1)
			{
				wide_val = utf_subst_val;
				sz += l;
				break;
			}
			
			wide_val <<= 6;
			wide_val |= (c & 0x3F);
		}
		
		if (sizeof(wchar_t) == 2 && (wide_val & 0xFFFF0000))
		{
			// Oh god.. we're big and going to UTF-16
			required_len += 2;
		}
		else
			++required_len;
		
	} while (len == size_t(-1) ? c != '\0' : size_t(sz-start)<len);

	return required_len;
}

size_t OOBase::from_utf8(wchar_t* wsz, size_t wlen, const char* start, size_t len)
{
	if (!wsz || !wlen)
		return measure_utf8(start,len);

	if (!start)
	{
		if (len == size_t(-1))
		{
			*wsz = L'\0';
			return 1;
		}
		else
			return 0;
	}

	wchar_t* wp = wsz;
	const char* sz = start;
	size_t required_len = 0;

	unsigned char c;

	do
	{
		// Default to substitution value
		unsigned int wide_val = utf_subst_val;
		int l = 0;

		c = *sz++;
		
		switch (utf8_data[c])
		{
			case 1:
				wide_val = c;
				break;
			
			case 2: 
				wide_val = (c & 0x1F);
				l = 1;
				break;
			
			case -2:
				++sz;
				break;
				
			case 3:
				wide_val = (c & 0x0F);
				l = 2;
				break;
			
			case -3: 
				if (*sz >= 0x80 && *sz <= 0x9F)
					sz += 2;
				else
				{
					wide_val = (c & 0x0F);
					l = 2;
				}
				break;
					
			case -4:
				if (*sz >= 0xA0 && *sz <= 0xBF)
					sz += 2;
				else
				{
					wide_val = (c & 0x0F);
					l = 2;
				}
				break;
				
			case 4:
				wide_val = (c & 0x07);
				l = 3;
				break;
				
			case -5:
				if (*sz >= 0x80 && *sz <= 0x8F)
					sz += 3;
				else
				{
					wide_val = (c & 0x07);
					l = 3;
				}
				break;
			
			case -6:
				if (*sz >= 0x90 && *sz <= 0xBF)
					sz += 3;
				else
				{
					wide_val = (c & 0x07);
					l = 3;
				}
				break;		
			
			default:
				break;
		}
		
		while (l-- > 0)
		{
			c = *sz++;
			if (utf8_data[c] != -1)
			{
				wide_val = utf_subst_val;
				sz += l;
				break;
			}
			
			wide_val <<= 6;
			wide_val |= (c & 0x3F);
		}
		
		if (sizeof(wchar_t) == 2 && (wide_val & 0xFFFF0000))
		{
			// Oh god.. we're big and going to UTF-16
			required_len += 2;
			wide_val -= 0x10000;

			if (required_len < wlen)
			{
#if (OMEGA_BYTE_ORDER != OMEGA_BIG_ENDIAN)
				*wp++ = static_cast<wchar_t>((wide_val >> 10) | 0xD800);
				*wp++ = static_cast<wchar_t>((wide_val & 0x3ff) | 0xDC00);
#else
				*wp++ = static_cast<wchar_t>((wide_val & 0x3ff) | 0xDC00);
				*wp++ = static_cast<wchar_t>((wide_val >> 10) | 0xD800);
#endif
			}
		}
		else
		{
			++required_len;
			if (required_len < wlen)
				*wp++ = static_cast<wchar_t>(wide_val);
		}
	} 
	while (len == size_t(-1) ? c != '\0' : size_t(sz-start)<len);

	return required_len;
}

size_t OOBase::measure_utf8(const wchar_t* wsz, size_t len)
{
	if (!wsz)
		return (len == size_t(-1) ? 1 : 0);

	size_t required_len = 0;
	for (const wchar_t* p=wsz; len == size_t(-1) ? *p!=L'\0' : size_t(p-wsz)<len;)
	{
		unsigned int v = *p++;

		if (sizeof(wchar_t) == 2)
		{
			if (v >= 0xD800 && v <= 0xDBFF)
			{
				// Surrogate pair
				v = (v & 0x27FF) << 10;
				if (len == size_t(-1) ? *p==L'\0' : size_t(p-wsz)>=len)
					break;

				v += ((*p++ & 0x23FF) >> 10) + 0x10000;
			}
			else if (v >= 0xDC00 && v <= 0xDFFF)
			{
				// Surrogate pair
				v = (v & 0x23FF) >> 10;
				if (len == size_t(-1) ? *p==L'\0' : size_t(p-wsz)>=len)
					break;

				v += ((*p++ & 0x27FF) << 10) + 0x10000;
			}
		}

		if (v <= 0x7f)
			++required_len;
		else if (v <= 0x7FF)
			required_len += 2;
		else if (v <= 0xFFFF)
			required_len += 3;
		else if (v <= 0x10FFFF)
			required_len += 4;
		else
			required_len += 3;
	}

	if (len == size_t(-1))
		++required_len;

	return required_len;
}

size_t OOBase::to_utf8(char* sz, size_t len, const wchar_t* wsz, size_t wlen)
{
	if (!sz || !len)
		return measure_utf8(wsz,wlen);

	if (!wsz)
	{
		if (wlen == size_t(-1))
		{
			*sz = '\0';
			return 1;
		}
		else
			return 0;
	}

	char* cp = sz;
	size_t required_len = 0;
	for (const wchar_t* p=wsz; wlen == size_t(-1) ? *p!=L'\0' : size_t(p-wsz)<wlen;)
	{
		unsigned int v = *p++;
		if (sizeof(wchar_t) == 2)
		{
			if (v >= 0xD800 && v <= 0xDBFF)
			{
				// Surrogate pair
				unsigned int hi = (v & 0x3FF);
				if (wlen == size_t(-1) ? *p==L'\0' : size_t(p-wsz)>=wlen)
					break;

				unsigned int lo = (*p++ & 0x3FF);

				v = ((hi << 10) | lo) + 0x10000;
			}
			else if (v >= 0xDC00 && v <= 0xDFFF)
			{
				// Surrogate pair
				unsigned int lo = (v & 0x3FF);
				if (wlen == size_t(-1) ? *p==L'\0' : size_t(p-wsz)>=wlen)
					break;

				unsigned int hi = (*p++ & 0x3FF);

				v = ((hi << 10) | lo) + 0x10000;
			}
		}

		if (v <= 0x7f)
		{
			++required_len;
			if (required_len < len)
				*cp++ = static_cast<char>(v);
		}
		else if (v <= 0x7FF)
		{
			required_len += 2;
			if (required_len < len)
			{
				*cp++ = static_cast<char>(v >> 6) | 0xc0;
				*cp++ = static_cast<char>(v & 0x3f) | 0x80;
			}
		}
		else if (v <= 0xFFFF)
		{
			required_len += 3;
			if (required_len < len)
			{
				// Invalid range
				if (v > 0xD7FF && v < 0xE00)
				{
					*cp++ = '\xEF';
					*cp++ = '\xBF';
					*cp++ = '\xBD';
				}
				else
				{
					*cp++ = static_cast<char>(v >> 12) | 0xe0;
					*cp++ = static_cast<char>((v & 0xfc0) >> 6) | 0x80;
					*cp++ = static_cast<char>(v & 0x3f) | 0x80;
				}
			}
		}
		else if (v <= 0x10FFFF)
		{
			required_len += 4;
			if (required_len < len)
			{
				*cp++ = static_cast<char>(v >> 18) | 0xf0;
				*cp++ = static_cast<char>((v & 0x3f000) >> 12) | 0x80;
				*cp++ = static_cast<char>((v & 0xfc0) >> 6) | 0x80;
				*cp++ = static_cast<char>(v & 0x3f) | 0x80;
			}
		}
		else
		{
			required_len += 3;
			if (required_len < len)
			{
				*cp++ = '\xEF';
				*cp++ = '\xBF';
				*cp++ = '\xBD';
			}
		}
	}

	if (wlen == size_t(-1))
	{
		++required_len;
		if (required_len < len)
			*cp = '\0';
	}

	return required_len;
}

size_t OOBase::measure_native(const char* sz, size_t len)
{
	if (!sz)
		return (len == size_t(-1) ? 1 : 0);

	wchar_t wc[2];
	mbstate_t state = {0};
	size_t required_len = 0;
	size_t c = MB_LEN_MAX;

	for (const char* p=sz; size_t(p-sz)<len;)
	{
		if (len != size_t(-1))
			c = len - size_t(p-sz);

		size_t count = mbrtowc(wc,p,c,&state);
		if (count == 0)
		{
			// Null
			++required_len;
			if (len == size_t(-1))
				break;

			++p;
		}
		else if (count > 0)
		{
			required_len += count;
			p += count;
		}
		else if (count == (size_t)-2)
		{
			if (len == size_t(-1))
				break;

			c *= 2;
		}
		else
			break;
	}

	return required_len;
}

size_t OOBase::from_native(wchar_t* wsz, size_t wlen, const char* sz, size_t len)
{
	if (!wsz || !wlen)
		return measure_native(sz,len);

	if (!sz)
	{
		if (len == size_t(-1))
		{
			*wsz = L'\0';
			return 1;
		}
		else
			return 0;
	}

	wchar_t* wp = wsz;
	mbstate_t state = {0};
	size_t required_len = 0;
	size_t c = MB_LEN_MAX;

	for (const char* p=sz; size_t(p-sz)<len && size_t(wp-wsz)<wlen;)
	{
		if (len != size_t(-1))
			c = len - size_t(p-sz);

		size_t count = mbrtowc(wp++,p,c,&state);
		if (count == 0)
		{
			// Null
			++required_len;
			if (len == size_t(-1))
				break;

			++p;
		}
		else if (count > 0)
		{
			required_len += count;
			p += count;
		}
		else if (count == (size_t)-2)
		{
			if (len == size_t(-1))
				break;

			c *= 2;
		}
		else
			break;
	}

	return required_len;
}

size_t OOBase::measure_native(const wchar_t* wsz, size_t len)
{
	if (!wsz)
		return (len == size_t(-1) ? 1 : 0);

	char c[MB_LEN_MAX];
	mbstate_t state = {0};
	size_t required_len = 0;
	for (const wchar_t* wp=wsz; size_t(wp-wsz)<len;)
	{
		size_t count = wcrtomb(c,*wp++,&state);
		if (count == size_t(-1))
			break;

		required_len += count;	
	}

	return required_len;
}

size_t OOBase::to_native(char* sz, size_t len, const wchar_t* wsz, size_t wlen)
{
	if (!sz || !len)
		return measure_native(wsz,wlen);

	if (!wsz)
	{
		if (wlen == size_t(-1))
		{
			*sz = '\0';
			return 1;
		}
		else
			return 0;
	}

	char c[MB_LEN_MAX];
	char* p = sz;
	mbstate_t state = {0};
	size_t required_len = 0;
	for (const wchar_t* wp=wsz; wlen == size_t(-1) ? *wp!=L'\0' : size_t(wp-wsz)<wlen;)
	{
		size_t count = wcrtomb(c,*wp++,&state);
		if (count == size_t(-1))
			break;

		required_len += count;

		// Check for overrun - don't write partial sequences...
		if (size_t(p-sz) + count > len)
			break;
		else if (wlen == size_t(-1) && size_t(p-sz) + count == len)
			break;
							
		memcpy(p,c,count);
		p += count;
	}

	if (wlen == size_t(-1))
	{
		++required_len;
		if (required_len < len)
			*p = '\0';
	}

	return required_len;
}

#endif
