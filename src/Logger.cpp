///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/tr24731.h"
#include "../include/OOBase/String.h"

#include "../include/OOSvrBase/Logger.h"
#include "../include/OOSvrBase/SecurityWin32.h"

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
#endif // HAVE_ASL_H/HAVE_SYSLOG_H

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

namespace
{
#if defined(_WIN32)
	class Win32Logger
	{
	public:
		Win32Logger();
		~Win32Logger();

		void open(const char* name);
		void log(OOSvrBase::Logger::Priority priority, const char* msg);

	private:
		OOBase::Mutex m_lock;
		HANDLE        m_hLog;
	};
#endif

#if defined(HAVE_ASL_H)
	
	class ASLLogger
	{
#error Fix me!
	};

#elif defined(HAVE_SYSLOG_H)

	class SysLogLogger
	{
	public:
		~SysLogLogger();

		void open(const char* name);
		void log(OOSvrBase::Logger::Priority priority, const char* fmt, va_list args);

	private:
		OOBase::Mutex m_lock;
	};
#endif

	class CrtFreeDestructor
	{
	public:
		static void destroy(void* ptr)
		{
			OOBase::CrtAllocator::free(ptr);
		}
	};

	/** \typedef LOGGER
	 *  The logger singleton.
	 *  The singleton is specialised by the platform specific implementation
	 *  e.g. \link Win32Logger \endlink , \link ASLLogger \endlink or \link SysLogLogger \endlink
	 */
#if defined(_WIN32)
	typedef OOBase::Singleton<Win32Logger,OOBase::Module> LOGGER;
#elif defined(HAVE_ASL_H)
	typedef OOBase::Singleton<ASLLogger,OOBase::Module> LOGGER;
#elif defined(HAVE_SYSLOG_H)
	typedef OOBase::Singleton<SysLogLogger,OOBase::Module> LOGGER;
#else
#error Fix me!
#endif

#if defined(_WIN32)

	Win32Logger::Win32Logger() :
			m_hLog(NULL)
	{
	}

	Win32Logger::~Win32Logger()
	{
		if (m_hLog)
			DeregisterEventSource(m_hLog);
	}

	void Win32Logger::open(const char* name)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		if (m_hLog)
		{
			DeregisterEventSource(m_hLog);
			m_hLog = NULL;
		}

		wchar_t szPath[MAX_PATH];
		if (!GetModuleFileNameW(NULL,szPath,MAX_PATH))
			return;

		// Create the relevant registry keys if they don't already exist
		OOBase::LocalString strName;
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

	void Win32Logger::log(OOSvrBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		WORD wType = 0;
		switch (priority)
		{
		case OOSvrBase::Logger::Error:
			wType = EVENTLOG_ERROR_TYPE;
			break;

		case OOSvrBase::Logger::Warning:
			wType = EVENTLOG_WARNING_TYPE;
			break;

		case OOSvrBase::Logger::Information:
			wType = EVENTLOG_INFORMATION_TYPE;
			break;

		default:
			break;
		}

#if !defined(_DEBUG)
		if (m_hLog && priority != OOSvrBase::Logger::Debug)
		{
			const char* arrBufs[2] = { msg.c_str(), NULL };

			PSID psid = NULL;
			OOBase::Win32::SmartHandle hProcessToken;
			
			if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hProcessToken))
			{
				OOBase::SmartPtr<TOKEN_USER,CrtFreeDestructor> ptrSIDProcess;
				for (DWORD dwLen = 256;;)
				{
					ptrSIDProcess = static_cast<TOKEN_USER*>(OOBase::CrtAllocator::allocate(dwLen));
					if (!ptrSIDProcess)
						break;

					if (GetTokenInformation(hProcessToken,TokenUser,ptrSIDProcess,dwLen,&dwLen))
						break;

					DWORD err = GetLastError();

					ptrSIDProcess = NULL;

					SetLastError(err);
						
					if (err != ERROR_INSUFFICIENT_BUFFER)
						break;
				}

				if (ptrSIDProcess)
					psid = ptrSIDProcess->User.Sid;
			}

			ReportEventA(m_hLog,wType,0,0,psid,1,0,arrBufs,NULL);
		}

		if (priority == OOSvrBase::Logger::Debug)
#endif
		{
			OutputDebugStringA(msg);
			OutputDebugStringA("\n");
		}

		FILE* out_file = stdout;
		switch (priority)
		{
		case OOSvrBase::Logger::Error:
			out_file = stderr;
			fputs("Error: ",out_file);
			break;

		case OOSvrBase::Logger::Warning:
			fputs("Warning: ",out_file);
			break;

#if !defined(_DEBUG)
		case OOSvrBase::Logger::Debug:
			return;
#endif

		default:
			break;
		}
		fputs(msg,out_file);
		fputs("\n",out_file);
		fflush(out_file);
	}

#endif // _WIN32

#if defined(HAVE_ASL_H)

#error Implementation here!

#elif defined(HAVE_SYSLOG_H)

	SysLogLogger::~SysLogLogger()
	{
		closelog();
	}

	void SysLogLogger::open(const char* name)
	{
		openlog(name,LOG_NDELAY,LOG_DAEMON);
	}

	void SysLogLogger::log(OOSvrBase::Logger::Priority priority, const char* fmt, va_list args)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		int wType = 0;
		switch (priority)
		{
		case OOSvrBase::Logger::Error:
			wType = LOG_MAKEPRI(LOG_DAEMON,LOG_ERR);
			break;

		case OOSvrBase::Logger::Warning:
			wType = LOG_MAKEPRI(LOG_DAEMON,LOG_WARNING);
			break;

		case OOSvrBase::Logger::Information:
			wType = LOG_MAKEPRI(LOG_DAEMON,LOG_INFO);
			break;

		case OOSvrBase::Logger::Debug:
			wType = LOG_MAKEPRI(LOG_DAEMON,LOG_DEBUG);
			break;

		default:
			break;
		}

		FILE* out_file = stdout;
		switch (priority)
		{
		case OOSvrBase::Logger::Error:
			out_file = stderr;
			fputs("Error: ",out_file);
			break;

		case OOSvrBase::Logger::Warning:
			fputs("Warning: ",out_file);
			break;

#if !defined(_DEBUG)
		case OOSvrBase::Logger::Debug:
			return;
#endif

		default:
			break;
		}

		fputs(string_printf(fmt,args).c_str(),out_file);
		fputs("\n",out_file);
		fflush(out_file);
	}

#endif // HAVE_SYSLOG_H

} // Anonymous namespace

void OOSvrBase::Logger::open(const char* name)
{
	LOGGER::instance().open(name);
}

void OOSvrBase::Logger::log(Priority priority, const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);

	OOBase::LocalString msg;
	int err = msg.vprintf(fmt,args);

	va_end(args);

	if (err == 0)
		LOGGER::instance().log(priority,msg.c_str());
}

void OOSvrBase::Logger::filenum_t::log(const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);

	OOBase::LocalString msg;
	int err = msg.vprintf(fmt,args);

	va_end(args);

	if (err == 0)
	{
		OOBase::LocalString header;
		if (header.printf("[%u] %s(%u): %s",getpid(),m_pszFilename,m_nLine,msg.c_str()) == 0)
			LOGGER::instance().log(m_priority,header.c_str());
	}
}
