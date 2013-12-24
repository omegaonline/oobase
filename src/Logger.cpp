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

#include "config-base.h"

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Once.h"
#include "../include/OOBase/Mutex.h"
#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/Win32Security.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Logger.h"
#include "../include/OOBase/Environment.h"
#include "../include/OOBase/tr24731.h"

#if defined(_WIN32)
#define getpid GetCurrentProcessId
#endif // _WIN32

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#endif

#if defined(HAVE_ASL_H)
#include <asl.h>
#elif defined(HAVE_SYSLOG_H)
// Syslog reuses these
#undef LOG_WARNING
#undef LOG_DEBUG
#include <syslog.h>
#include <stdlib.h>
#endif // HAVE_ASL_H/HAVE_SYSLOG_H

namespace
{
#if defined(_WIN32)
	class Win32Logger
	{
	public:
		Win32Logger() : m_hLog(NULL), m_pszSrcFile("")
		{};

		static void init();

		void open_console(const char* pszSrcFile);
		void open_system_log(const char* name, const char* pszSrcFile);
		void log(OOBase::Logger::Priority priority, const char* msg);
		const char* src() const { return m_pszSrcFile; }

	private:
		OOBase::Mutex m_lock;
		HANDLE        m_hLog;
		const char*   m_pszSrcFile;
	};
#endif

#if defined(HAVE_ASL_H)

	class ASLLogger
	{
#error Implement ASL logger
	};

#elif defined(HAVE_SYSLOG_H)

	class SysLogLogger
	{
	public:
		SysLogLogger() : m_pszSrcFile(""), m_use_term(false), m_syslog(false)
		{};

		static void init();

		void open_console(const char* pszSrcFile);
		void open_system_log(const char* name, const char* pszSrcFile);
		void log(OOBase::Logger::Priority priority, const char* msg);
		const char* src() const { return m_pszSrcFile; }

	private:
		OOBase::Mutex m_lock;
		const char*   m_pszSrcFile;
		bool          m_use_term;
		bool          m_syslog;
	};
#endif

	/** \typedef LoggerClass
	 *  The logger singleton.
	 *  The singleton is specialised by the platform specific implementation
	 *  e.g. \link Win32Logger \endlink , \link ASLLogger \endlink or \link SysLogLogger \endlink
	 */
#if defined(_WIN32)
	typedef Win32Logger LoggerClass;
#elif defined(HAVE_ASL_H)
	typedef ASLLogger LoggerClass;
#elif defined(HAVE_SYSLOG_H)
	typedef SysLogLogger LoggerClass;
#else
#error Implement platform native system logger
#endif

	static LoggerClass s_instance;

	static LoggerClass& LoggerInstance()
	{
		static OOBase::Once::once_t key = ONCE_T_INIT;
		OOBase::Once::Run(&key,&LoggerClass::init);
		return s_instance;
	}

#if defined(_WIN32)

	void Win32Logger::init()
	{
		s_instance.m_hLog = NULL;
	}

	void Win32Logger::open_console(const char* pszSrcFile)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		if (m_hLog)
		{
			DeregisterEventSource(m_hLog);
			m_hLog = NULL;
		}

		if (pszSrcFile)
			m_pszSrcFile = pszSrcFile;
	}

	void Win32Logger::open_system_log(const char* name, const char* pszSrcFile)
	{
		open_console(pszSrcFile);

		wchar_t szPath[MAX_PATH];
		if (!GetModuleFileNameW(NULL,szPath,MAX_PATH))
			return;

		// Create the relevant registry keys if they don't already exist
		OOBase::StackAllocator<256> allocator;
		OOBase::LocalString strName(allocator);
		int err = strName.assign("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
		if (err == 0)
			err = strName.append(name);

		if (err != 0)
			OOBase_CallCriticalFailure(err);

		HKEY hk;
		DWORD dwDisp;
		if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,strName.c_str(),0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&hk,&dwDisp) == ERROR_SUCCESS)
		{
			RegSetValueExW(hk,L"EventMessageFile",0,REG_SZ,(LPBYTE)szPath,(DWORD)(wcslen(szPath)+1)*sizeof(wchar_t));

			DWORD dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
			RegSetValueExW(hk,L"TypesSupported",0,REG_DWORD,(LPBYTE)&dwData,sizeof(DWORD));

			RegCloseKey(hk);
		}

		// Get our event source
		m_hLog = RegisterEventSourceA(NULL,name);
	}

	WORD get_console_attrs(bool use_stderr)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		info.wAttributes = 0;
		HANDLE h = GetStdHandle(use_stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
		if (h != INVALID_HANDLE_VALUE)
			GetConsoleScreenBufferInfo(h,&info);

		return info.wAttributes;
	}

	void set_console_attrs(bool use_stderr, WORD attrs)
	{
		HANDLE h = GetStdHandle(use_stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
		if (h != INVALID_HANDLE_VALUE)
			SetConsoleTextAttribute(h,attrs);
	}

	void Win32Logger::log(OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		OOBase::StackAllocator<512> allocator;
		OOBase::TempPtr<wchar_t> wmsg(allocator);
		OOBase::Win32::utf8_to_wchar_t(msg,wmsg);

		if (m_hLog && priority != OOBase::Logger::Debug)
		{
			WORD wType = 0;
			switch (priority)
			{
			case OOBase::Logger::Error:
				wType = EVENTLOG_ERROR_TYPE;
				break;

			case OOBase::Logger::Warning:
				wType = EVENTLOG_WARNING_TYPE;
				break;

			case OOBase::Logger::Information:
				wType = EVENTLOG_INFORMATION_TYPE;
				break;

			default:
				break;
			}

			OOBase::LocalPtr<TOKEN_USER,OOBase::FreeDestructor<OOBase::AllocatorInstance> > ptrSIDProcess(allocator);
			PSID psid = NULL;
			OOBase::Win32::SmartHandle hProcessToken;

			if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hProcessToken))
			{
				if (OOBase::Win32::GetTokenInfo(hProcessToken,TokenUser,ptrSIDProcess) == 0)
					psid = ptrSIDProcess->User.Sid;
			}

			if (wmsg)
			{
				const wchar_t* arrBufs[2] = { static_cast<wchar_t*>(wmsg), NULL };
				ReportEventW(m_hLog,wType,0,0,psid,1,0,arrBufs,NULL);
			}
			else
			{
				const char* arrBufs[2] = { msg, NULL };
				ReportEventA(m_hLog,wType,0,0,psid,1,0,arrBufs,NULL);
			}
		}

		if (wmsg)
		{
			OutputDebugStringW(wmsg);
			OutputDebugStringW(L"\n");
		}
		else
		{
			OutputDebugStringA(msg);
			OutputDebugStringA("\n");
		}
		
		WORD attrs = 0;
		switch (priority)
		{
		case OOBase::Logger::Error:
			attrs = get_console_attrs(true);
			set_console_attrs(true,FOREGROUND_RED);
			OOBase::stderr_write("Error: ");
			if (wmsg)
				OOBase::stderr_write(wmsg);
			else
				OOBase::stderr_write(msg);
			OOBase::stderr_write("\n");
			set_console_attrs(true,attrs);
			break;

		case OOBase::Logger::Warning:
			attrs = get_console_attrs(false);
			set_console_attrs(false,FOREGROUND_RED | FOREGROUND_GREEN);
			OOBase::stdout_write("Warning: ");
			if (wmsg)
				OOBase::stdout_write(wmsg);
			else
				OOBase::stdout_write(msg);
			OOBase::stdout_write("\n");
			set_console_attrs(false,attrs);
			break;

		case OOBase::Logger::Information:
			if (wmsg)
				OOBase::stdout_write(wmsg);
			else
				OOBase::stdout_write(msg);
			OOBase::stdout_write("\n");
			break;

#if !defined(NDEBUG)
		case OOBase::Logger::Debug:
			attrs = get_console_attrs(false);
			set_console_attrs(false,FOREGROUND_GREEN);
			OOBase::stdout_write("Debug: ");
			if (wmsg)
				OOBase::stdout_write(wmsg);
			else
				OOBase::stdout_write(msg);
			OOBase::stdout_write("\n");
			set_console_attrs(false,attrs);
			return;
#endif

		default:
			break;
		}
	}

#endif // _WIN32

#if defined(HAVE_SYSLOG_H)

	void SysLogLogger::init()
	{
		s_instance.m_use_term = (::getenv("TERM") != NULL);
	}

	void SysLogLogger::open_console(const char* pszSrcFile)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		if (m_syslog)
			closelog();

		m_syslog = false;

		if (pszSrcFile)
			m_pszSrcFile = pszSrcFile;
	}

	void SysLogLogger::open_system_log(const char* name, const char* pszSrcFile)
	{
		open_console(pszSrcFile);

		m_syslog = true;
	
		openlog(name,LOG_NDELAY,LOG_DAEMON | LOG_CONS);
	}

	void SysLogLogger::log(OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		if (m_syslog)
		{
			int wType = 0;
			switch (priority)
			{
			case OOBase::Logger::Error:
				wType = LOG_MAKEPRI(LOG_DAEMON,LOG_ERR);
				break;

			case OOBase::Logger::Warning:
				wType = LOG_MAKEPRI(LOG_DAEMON,LOG_WARNING);
				break;

			case OOBase::Logger::Information:
				wType = LOG_MAKEPRI(LOG_DAEMON,LOG_INFO);
				break;

			case OOBase::Logger::Debug:
				wType = LOG_MAKEPRI(LOG_DAEMON,LOG_DEBUG);
				break;

			default:
				break;
			}

			syslog(wType,"%s",msg);
		}

		bool is_a_tty;
		switch (priority)
		{
		case OOBase::Logger::Error:
			is_a_tty = (isatty(STDERR_FILENO) == 1);
			if (m_use_term && is_a_tty) OOBase::stderr_write("\x1b[31m");
			OOBase::stderr_write("Error: ");
			OOBase::stderr_write(msg);
			if (m_use_term && is_a_tty) OOBase::stderr_write("\x1b[0m");
			OOBase::stderr_write("\n");
			break;

		case OOBase::Logger::Warning:
			is_a_tty = (isatty(STDOUT_FILENO) == 1);
			if (m_use_term && is_a_tty) OOBase::stdout_write("\x1b[33m");
			OOBase::stdout_write("Warning: ");
			OOBase::stdout_write(msg);
			if (m_use_term && is_a_tty) OOBase::stdout_write("\x1b[0m");
			OOBase::stdout_write("\n");
			break;

		case OOBase::Logger::Information:
			OOBase::stdout_write(msg);
			OOBase::stdout_write("\n");
			break;

#if !defined(NDEBUG)
		case OOBase::Logger::Debug:
			is_a_tty = (isatty(STDOUT_FILENO) == 1);
			if (m_use_term && is_a_tty) OOBase::stdout_write("\x1b[32m");
			OOBase::stdout_write("Debug: ");
			OOBase::stdout_write(msg);
			if (m_use_term && is_a_tty) OOBase::stdout_write("\x1b[0m");
			OOBase::stdout_write("\n");
			break;
#endif

		default:
			break;
		}
	}

#endif // HAVE_SYSLOG_H

} // Anonymous namespace

void OOBase::Logger::open_console_log(const char* pszSrcFile)
{
	LoggerInstance().open_console(pszSrcFile);
}

void OOBase::Logger::open_system_log(const char* name, const char* pszSrcFile)
{
	LoggerInstance().open_system_log(name,pszSrcFile);
}

void OOBase::Logger::log(Priority priority, const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);

	log(priority,fmt,args);

	va_end(args);
}

void OOBase::Logger::log(Priority priority, const char* fmt, va_list args)
{
	StackAllocator<512> allocator;
	TempPtr<char> ptr(allocator);
	if (temp_vprintf(ptr,fmt,args) == 0)
		LoggerInstance().log(priority,ptr.get());
	else
		LoggerInstance().log(priority,fmt);
}

OOBase::Logger::filenum_t::filenum_t(Priority priority, const char* pszFilename, unsigned int nLine) :
		m_priority(priority),
		m_pszFilename(pszFilename),
		m_nLine(nLine)
{
}

void OOBase::Logger::filenum_t::log(const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);

	StackAllocator<512> allocator;
	TempPtr<char> msg(allocator);
	int err = temp_vprintf(msg,fmt,args);

	va_end(args);

	if (err == 0)
	{
		if (m_pszFilename)
		{
			const char* pszSrcFile = LoggerInstance().src();
			size_t s=0;
			for (;;)
			{
				size_t s1 = s;
				while (pszSrcFile[s1] == m_pszFilename[s1] && pszSrcFile[s1] != '\0' && pszSrcFile[s1] != '\\' && pszSrcFile[s1] != '/')
					++s1;

				if (pszSrcFile[s1] == '\\' || pszSrcFile[s1] == '/')
					s = s1+1;
				else
					break;
			}
			m_pszFilename += s;
		}

		TempPtr<char> header(allocator);
		if (OOBase::temp_printf(header,"%s(%u): %s",m_pszFilename,m_nLine,msg.get()) == 0)
			LoggerInstance().log(m_priority,header.get());
	}
}
