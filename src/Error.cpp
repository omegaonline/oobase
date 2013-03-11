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

#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/Posix.h"
#include "../include/OOBase/Atomic.h"
#include "../include/OOBase/tr24731.h"

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

		LPVOID lpBuf = NULL;
		if (::FormatMessageW(
					dwFlags,
					hModule,
					dwErr,
					0,
					(LPWSTR)&lpBuf,
					0, NULL))
		{
			int len = WideCharToMultiByte(CP_UTF8,0,(LPWSTR)lpBuf,-1,err_buf,err_len,NULL,NULL);

			LocalFree(lpBuf);

			if (len <= 0)
				return false;

			for (--len; len > 0 && (err_buf[len-1]=='\r' || err_buf[len-1]=='\n'); --len)
				;

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

#if !defined(DOXYGEN) && !defined(OOBASE_NO_ERROR_HANDLERS)
	void unexpected()
	{
		OOBase::CallCriticalFailure(NULL,0,"Unexpected exception thrown");
	}

	void terminate()
	{
		OOBase::CallCriticalFailure(NULL,0,"Unhandled exception");
	}

	#if defined(_MSC_VER)
	void purecall_handler()
	{
		OOBase::CallCriticalFailure(NULL,0,"Pure call");
	}

	void invalid_parameter_handler(const wchar_t * /*expression*/, const wchar_t * /*function*/, const wchar_t * /*file*/, unsigned int /*line*/, uintptr_t /*pReserved*/)
	{
		OOBase::CallCriticalFailure(NULL,0,"CRT Invalid parameter");
	}

	void __cdecl abort_handler(int)
	{
		OOBase::CallCriticalFailure(NULL,0,"CRT aborted");
	}
	#endif

	int install_handlers()
	{
		std::set_unexpected(&unexpected);
		std::set_terminate(&terminate);

		#if defined(_WIN32)
			const DWORD new_mask = SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX;
			DWORD mask = SetErrorMode(new_mask);
			SetErrorMode(mask | new_mask);
		#endif

		#if defined(_MSC_VER)
			_set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);
			signal(SIGABRT,&abort_handler);

			_set_purecall_handler(&purecall_handler);
			_set_invalid_parameter_handler(&invalid_parameter_handler);
		#endif

		return 0;
	}
	const int s_err_handler = install_handlers();

#endif // !defined(DOXYGEN) && !defined(OOBASE_NO_ERROR_HANDLERS)

	int std_write(bool use_stderr, const char* sz, size_t len)
	{
		if (len == size_t(-1))
			len = strlen(sz);

		int err = s_err_handler; // Force usage of s_err_handler
		if (len > 0)
		{
#if defined(_WIN32)
			HANDLE h = GetStdHandle(use_stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
			if (h == INVALID_HANDLE_VALUE)
				err = GetLastError();
			else
			{
				DWORD dwMode = 0;
				if (GetConsoleMode(h,&dwMode))
				{
					DWORD dwWritten = 0;
					if (!WriteConsoleA(h,sz,(DWORD)len,&dwWritten,NULL))
						err = GetLastError();
				}
				else
				{
					DWORD dw = 0;
					if (!WriteFile(h,sz,(DWORD)len,&dw,NULL))
						err = GetLastError();
				}
			}

#elif defined(HAVE_UNISTD_H)
			int fd = use_stderr ? STDERR_FILENO : STDOUT_FILENO;
			ssize_t s = OOBase::POSIX::write(fd,sz,len);
			if (s == -1)
				err = errno;
#endif
		}
		return err;
	}

#if defined(_WIN32)
	int std_write(bool use_stderr, const wchar_t* wsz, size_t len)
	{
		if (len == size_t(-1))
			len = wcslen(wsz);

		int err = s_err_handler; // Force usage of s_err_handler
		if (len > 0)
		{
			HANDLE h = GetStdHandle(use_stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
			if (h == INVALID_HANDLE_VALUE)
				err = GetLastError();
			else
			{
				DWORD dwMode = 0;
				if (GetConsoleMode(h,&dwMode))
				{
					DWORD dwWritten = 0;
					if (!WriteConsoleW(h,wsz,(DWORD)len,&dwWritten,NULL))
						err = GetLastError();
				}
				else
				{
					DWORD dw = 0;
					if (!WriteFile(h,wsz,(DWORD)(len * sizeof(wchar_t)),&dw,NULL))
						err = GetLastError();
				}
			}
		}
		return err;
	}
#endif

	char s_err_buf[512];
}

const char* OOBase::system_error_text(int err)
{
	if (err == -1)
	{
#if defined(_WIN32)
		err = GetLastError();
#else
		err = errno;
#endif
	}

	static const char unknown_error[] = "Unknown error or error in processing";
	bool ok = false;

	size_t err_len = 0;
	char* err_buf = OOBase::detail::get_error_buffer(err_len);
	if (!err_buf)
	{
		err_buf = s_err_buf;
		err_len = sizeof(s_err_buf);
	}

	int offset = snprintf_s(err_buf,err_len,"(%d) ",err);
	if (offset == -1)
		offset = 0;

#if defined(_WIN32)
	if (!(err & 0xC0000000))
		ok = format_msg(err_buf+offset,err_len-offset,err,NULL);
	else
		ok = format_msg(err_buf+offset,err_len-offset,err,GetModuleHandleW(L"NTDLL.DLL"));
#else

#if defined(_GNU_SOURCE)
	char* s = strerror_r(err,err_buf+offset,err_len-offset);
	if (s)
	{
		memcpy(err_buf+offset,s,err_len-offset);
		err_buf[err_len-1] = '\0';
		ok = true;
	}
#elif defined(HAVE_PTHREAD)
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

int OOBase::stderr_write(const char* sz, size_t len)
{
	return std_write(true,sz,len);
}

int OOBase::stdout_write(const char* sz, size_t len)
{
	return std_write(false,sz,len);
}

#if defined(_WIN32)
int OOBase::stderr_write(const wchar_t* wsz, size_t len)
{
	return std_write(true,wsz,len);
}

int OOBase::stdout_write(const wchar_t* wsz, size_t len)
{
	return std_write(false,wsz,len);
}
#endif

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

	// Restore default behaviour in case of recursive critical failures!
	OnCriticalFailure pfn = SetCriticalFailure(&DefaultOnCriticalFailure);
	if (!(*pfn)(szBuf))
	{
#if defined(_WIN32)
		static wchar_t wszBuf[1024] = {0};
		int len = MultiByteToWideChar(CP_UTF8,0,szBuf,-1,wszBuf,sizeof(wszBuf)/sizeof(wszBuf[0]));
		if (len <= 0)
			OutputDebugStringA(szBuf);
		else
		{
			wszBuf[len-1] = L'\0';
			OutputDebugStringW(wszBuf);
		}

		// Try to detect if we are running as a service
		static wchar_t sn[] = L"";
		static SERVICE_TABLE_ENTRYW ste[] =
		{
			{sn, &DummyServiceMain },
			{NULL, NULL}
		};

		bool bReport = true;
		if (!StartServiceCtrlDispatcherW(ste) && GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			if (GetConsoleWindow() == NULL)
			{
				// We are not a console application...
				if (len > 0)
					::MessageBoxW(NULL,wszBuf,L"Critical error in application",MB_ICONERROR | MB_TASKMODAL | MB_OK | MB_SETFOREGROUND);
				else
					::MessageBoxA(NULL,szBuf,"Critical error in application",MB_ICONERROR | MB_TASKMODAL | MB_OK | MB_SETFOREGROUND);
				bReport = false;
			}
		}

		if (bReport)
		{
			if (len > 0)
				stderr_write(wszBuf);
			else
				stderr_write(szBuf);
		}
#else
		stderr_write(szBuf);
#endif
	}

#if defined(_WIN32)
	#if !defined(NDEBUG)
		if (IsDebuggerPresent()) DebugBreak();
	#endif
	// We don't want the stupid CRT abort message
	TerminateProcess(GetCurrentProcess(),EXIT_FAILURE);
#else
	#if !defined(NDEBUG)
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
