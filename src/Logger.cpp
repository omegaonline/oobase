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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Once.h"
#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/SecurityWin32.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Logger.h"

#include "tr24731.h"

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

namespace
{
#if defined(_WIN32)
	class Win32Logger
	{
	public:
		static void init();

		void open(const char* name, const char* pszSrcFile);
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
#error Fix me!
	};

#elif defined(HAVE_SYSLOG_H)

	class SysLogLogger
	{
	public:
		static void init();

		void open(const char* name, const char* pszSrcFile);
		void log(OOBase::Logger::Priority priority, const char* msg);
		const char* src() const { return m_pszSrcFile; }

	private:
		OOBase::Mutex m_lock;
		const char*   m_pszSrcFile;
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
#error Fix me!
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
		s_instance.m_pszSrcFile = "";
	}

	void Win32Logger::open(const char* name, const char* pszSrcFile)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		if (pszSrcFile)
			m_pszSrcFile = pszSrcFile;

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

	void Win32Logger::log(OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

#if !defined(_DEBUG)
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

			const char* arrBufs[2] = { msg, NULL };

			OOBase::SmartPtr<TOKEN_USER,OOBase::CrtAllocator> ptrSIDProcess;
			PSID psid = NULL;
			OOBase::Win32::SmartHandle hProcessToken;

			if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hProcessToken))
			{
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

		if (priority == OOBase::Logger::Debug)
#endif
		{
			OutputDebugStringA(msg);
			OutputDebugStringA("\n");
		}

		switch (priority)
		{
		case OOBase::Logger::Error:
			OOBase::stderr_write("Error: ");
			OOBase::stderr_write(msg);
			OOBase::stderr_write("\n");
			break;

		case OOBase::Logger::Warning:
			OOBase::stdout_write("Warning: ");
			OOBase::stderr_write(msg);
			OOBase::stderr_write("\n");
			break;

#if !defined(_DEBUG)
		case OOBase::Logger::Debug:
			return;
#endif

		default:
			break;
		}
	}

#endif // _WIN32

#if defined(HAVE_ASL_H)

#error Implementation here!

#elif defined(HAVE_SYSLOG_H)

	void SysLogLogger::init()
	{
		s_instance.m_pszSrcFile = "";
	}

	void SysLogLogger::open(const char* name, const char* pszSrcFile)
	{
		if (pszSrcFile)
			m_pszSrcFile = pszSrcFile;

		openlog(name,LOG_NDELAY,LOG_DAEMON | LOG_CONS);
	}

	void SysLogLogger::log(OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

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

		switch (priority)
		{
		case OOBase::Logger::Error:
			OOBase::stderr_write("Error: ");
			OOBase::stderr_write(msg);
			OOBase::stderr_write("\n");
			break;

		case OOBase::Logger::Warning:
			OOBase::stdout_write("Warning: ");
			OOBase::stdout_write(msg);
			OOBase::stdout_write("\n");
			break;

#if !defined(_DEBUG)
		case OOBase::Logger::Debug:
			return;
#endif

		default:
			break;
		}
	}

#endif // HAVE_SYSLOG_H

} // Anonymous namespace

void OOBase::Logger::open(const char* name, const char* pszSrcFile)
{
	LoggerInstance().open(name,pszSrcFile);
}

void OOBase::Logger::log(Priority priority, const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);

	OOBase::LocalString msg;
	int err = msg.vprintf(fmt,args);

	va_end(args);

	if (err == 0)
		LoggerInstance().log(priority,msg.c_str());
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

	OOBase::LocalString msg;
	int err = msg.vprintf(fmt,args);

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

		OOBase::LocalString header;
		if (header.printf("%s(%u): %s",m_pszFilename,m_nLine,msg.c_str()) == 0)
			LoggerInstance().log(m_priority,header.c_str());
	}
}
