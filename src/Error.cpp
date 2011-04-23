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
#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/String.h"

namespace OOBase
{
	// This is the critical failure hook
	void OnCriticalFailure(const char* msg);

	namespace detail
	{
		char* get_error_buffer(size_t& len);
	}
}

#if defined(_WIN32)

#include <process.h>

namespace
{
	bool format_msg(char* err_buf, size_t err_len, DWORD dwErr, HMODULE hModule)
	{
		DWORD dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK;
		if (hModule)
			dwFlags |= FORMAT_MESSAGE_FROM_HMODULE;
		else
			dwFlags |= FORMAT_MESSAGE_FROM_SYSTEM;

		LPVOID lpBuf;
		if (::FormatMessageA(
					dwFlags,
					hModule,
					dwErr,
					0,
					(LPSTR)&lpBuf,
					0,  NULL))
		{
			OOBase::SmartPtr<char,OOBase::Win32::LocalAllocDestructor<char> > buf = static_cast<char*>(lpBuf);
			size_t len = strlen(buf);
			while (len > 0 && (buf[len-1]=='\r' || buf[len-1]=='\n'))
				--len;

			if (len >= err_len)
				len = err_len - 1;

			memcpy(err_buf,buf,len);
			err_buf[len] = '\0';
		
			return true;
		}
		else
		{
			return false;
		}
	}
}
#endif // _WIN32

const char* OOBase::system_error_text(int err)
{
	static const char unknown_error[] = "Unknown error or error in processing";
#if defined(_WIN32)

	if (err == -1)
		err = GetLastError();

	size_t err_len = 0;
	char* err_buf = OOBase::detail::get_error_buffer(err_len);
	
	int offset = snprintf_s(err_buf,err_len,"(%x) ",err);
	if (offset == -1)
		offset = 0;

	bool ok = false;
	if (!(err & 0xC0000000))
		ok = format_msg(err_buf+offset,err_len-offset,err,NULL);
	else
		ok = format_msg(err_buf+offset,err_len-offset,err,GetModuleHandleW(L"NTDLL.DLL"));

	if (!ok)
		memcpy(err_buf,unknown_error,sizeof(unknown_error));

	return err_buf;

#else

	if (err == -1)
		err = errno;

	size_t err_len = 0;
	char* err_buf = OOBase::detail::get_error_buffer(err_len);
	
	int offset = snprintf_s(err_buf,err_len,"(%d) ",err);
	if (offset == -1)
		offset = 0;

	bool ok = false;
#if defined(HAVE_PTHREADS_H)
	ok = (strerror_r(err_buf+offset,err_len-offset,err) == 0);
#elif defined(HAVE_TR_24731)
	ok = (strerror_s(err_buf+offset,err_len-offset,err) == 0);
#else
	const char* str_err = ::strerror(err);
	if (str_err)
	{
		memcpy(err_buf+offset,str_err,err_len-offset);
		ok = true;
	}
#endif

	if (!ok)
		memcpy(err_buf,unknown_error,sizeof(unknown_error));

	return err_buf;

#endif
}

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, int err)
{
	CallCriticalFailure(pszFile,nLine,OOBase::system_error_text(err));
}

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, const char* msg)
{
	static char szBuf[1024] = {0};
	snprintf_s(szBuf,sizeof(szBuf),"%s(%u): %s",pszFile,nLine,msg);
	
	OOBase::OnCriticalFailure(szBuf);

	abort();
}
