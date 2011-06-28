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

#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <exception>

#if defined(_WIN32)
#include <process.h>
#endif

namespace OOBase
{
	namespace detail
	{
		char* get_error_buffer(size_t& len);
	}
}

namespace
{
#if defined(_WIN32)

	VOID WINAPI DummyServiceMain(DWORD /*dwArgc*/, LPWSTR* /*lpszArgv*/) { }

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

#endif // _WIN32

	bool DefaultOnCriticalFailure(const char* /*msg*/) 
	{ 
		return false; 
	}

	static bool (*s_pfnCriticalFailure)(const char*) = &DefaultOnCriticalFailure;

#if !defined(DOXYGEN)
	void unexpected()
	{
		OOBase::CallCriticalFailure(NULL,0,"Unexpected exception thrown");
	}

	void terminate()
	{
		OOBase::CallCriticalFailure(NULL,0,"Unhandled exception");
	}

#if !defined(OOBASE_NO_ERROR_HANDLERS)
	static struct install_handlers
	{
		install_handlers()
		{
			std::set_unexpected(&unexpected);
			std::set_terminate(&terminate);

			#if defined(_WIN32)
				const DWORD new_mask = SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX;
				DWORD mask = SetErrorMode(new_mask);
				SetErrorMode(mask | new_mask);
			#endif
		}
	} s_err_handler;
#endif // !defined(OOBASE_NO_ERROR_HANDLERS)

#endif // !defined(DOXYGEN)
}

const char* OOBase::system_error_text(int err)
{
	static const char unknown_error[] = "Unknown error or error in processing";
	bool ok = false;

	size_t err_len = 0;
	char* err_buf = OOBase::detail::get_error_buffer(err_len);

	int offset = snprintf_s(err_buf,err_len,"(%x) ",err);
	if (offset == -1)
		offset = 0;

#if defined(_WIN32)

	if (err == -1)
		err = GetLastError();

	if (!(err & 0xC0000000))
		ok = format_msg(err_buf+offset,err_len-offset,err,NULL);
	else
		ok = format_msg(err_buf+offset,err_len-offset,err,GetModuleHandleW(L"NTDLL.DLL"));

#else

	if (err == -1)
		err = errno;

#if defined(HAVE_PTHREAD)
	ok = (strerror_r(err,err_buf+offset,err_len-offset) == 0);
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
#endif

	if (!ok)
		memcpy(err_buf,unknown_error,sizeof(unknown_error));

	return err_buf;
}

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, int err)
{
	CallCriticalFailure(pszFile,nLine,OOBase::system_error_text(err));
}

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, const char* msg)
{
	static char szBuf[1024] = {0};

	if (pszFile)
	{
		static const char szOurFile[] = __FILE__;
		size_t s=0;
		for (;;)
		{
			size_t s1 = s;
			while (szOurFile[s1] == pszFile[s1] && szOurFile[s1] != '\\' && szOurFile[s1] != '/')
				++s1;

			if (szOurFile[s1] == '\\' || szOurFile[s1] == '/')
				s = s1+1;
			else
				break;
		}

		snprintf_s(szBuf,sizeof(szBuf),
			"Critical error '%s' has occurred in the application at %s(%u).  "
			"The application will now terminate.\n",msg,pszFile+s,nLine);
	}
	else
	{
		snprintf_s(szBuf,sizeof(szBuf),
			"Critical error '%s' has occurred in the application.  "
			"The application will now terminate.\n",msg);
	}

	bool bReport = false;
	if (!(*s_pfnCriticalFailure)(szBuf))
	{
		bReport = true;

#if defined(_WIN32)
		// Output to the debugger
		OutputDebugStringA(szBuf);

		// Try to detect if we are running as a service
		static wchar_t sn[] = L"";
		static SERVICE_TABLE_ENTRYW ste[] =
		{
			{sn, &DummyServiceMain },
			{NULL, NULL}
		};

		if (!StartServiceCtrlDispatcherW(ste) && GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			if (GetConsoleWindow() == NULL)
			{
				// We are not a console application...
				::MessageBoxA(NULL,szBuf,"Critical error in application",MB_ICONERROR | MB_TASKMODAL | MB_OK | MB_SETFOREGROUND);
				bReport = false;
			}
		}
#endif
	}

	if (bReport)
	{
		fputs(szBuf,stderr);
		fflush(stderr);
	}

#if defined(_WIN32)
	#if defined(_DEBUG)
		if (IsDebuggerPresent()) DebugBreak();
	#endif
	// We don't want the stupid CRT abort message
	TerminateProcess(GetCurrentProcess(),EXIT_FAILURE);
#else
	#if defined(_DEBUG)
		raise(SIGINT);
	#endif
	abort();
#endif
}

OOBase::OnCriticalFailure OOBase::SetCriticalFailure(OnCriticalFailure pfn)
{
	OnCriticalFailure oldVal = NULL;
	do
	{
		oldVal = s_pfnCriticalFailure;
	} while (Atomic<OnCriticalFailure>::CompareAndSwap(s_pfnCriticalFailure,oldVal,pfn) != oldVal);

	return oldVal;
}
