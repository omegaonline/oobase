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

#include "../include/OOBase/Logger.h"
#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/Once.h"
#include "../include/OOBase/Mutex.h"

#include "../include/OOBase/Win32Security.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Environment.h"
#include "../include/OOBase/tr24731.h"

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
	// Force the clock to start at init
	OOBase::Clock s_start_time;

	class Logger
	{
	public:
		Logger() : m_pszSrcFile(NULL)
		{}

		static void init();

		void log(OOBase::Logger::Priority priority, const char* msg);
		const char* src() const { return m_pszSrcFile; }
		void set_src(const char* pszSrcFile) { m_pszSrcFile = pszSrcFile; }

		bool connect(void (*callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg), void* param);
		bool disconnect(void (*callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg), void* param);

		void swap(Logger& rhs)
		{
			OOBase::swap(m_lock,rhs.m_lock);
			OOBase::swap(m_pszSrcFile,rhs.m_pszSrcFile);
		}

	private:
		OOBase::Mutex m_lock;
		const char*   m_pszSrcFile;

		class OutputShim
		{
		public:
			OutputShim(void (*callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg), void* param) :
				m_callback(callback),
				m_param(param)
			{}

			OutputShim(const OutputShim& rhs) : m_callback(rhs.m_callback), m_param(rhs.m_param)
			{}

			OutputShim& operator = (const OutputShim& rhs)
			{
				OutputShim(rhs).swap(*this);
				return *this;
			}

			void swap(OutputShim& rhs)
			{
				OOBase::swap(m_callback,rhs.m_callback);
				OOBase::swap(m_param,rhs.m_param);
			}

			bool operator == (const OutputShim& rhs) const
			{
				return m_callback == rhs.m_callback && m_param == rhs.m_param;
			}

			void (*m_callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg);
			void* m_param;
		};
		OOBase::Vector<OutputShim> m_vecOutputs;
	};

	typedef OOBase::Singleton<Logger,OOBase::Module> LOGGER;
}

template class OOBase::Singleton<Logger,OOBase::Module>;

bool Logger::connect(void (*callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg), void* param)
{
	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	return m_vecOutputs.push_back(OutputShim(callback,param));
}

bool Logger::disconnect(void (*callback)(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg), void* param)
{
	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	return m_vecOutputs.remove(OutputShim(callback,param)) > 0;
}

void Logger::log(OOBase::Logger::Priority priority, const char* msg)
{
	timeval t = {0};
	s_start_time.get_timeval(t);

	OOBase::Guard<OOBase::Mutex> guard(m_lock);

	for (OOBase::Vector<OutputShim>::const_iterator i=m_vecOutputs.cbegin(); i != m_vecOutputs.cend(); ++i)
		(*(i->m_callback))(i->m_param,t,priority,msg);
}

void OOBase::Logger::set_source_file(const char* pszSrcFile)
{
	::Logger* logger = LOGGER::instance_ptr();
	if (logger)
		logger->set_src(pszSrcFile);
}

bool OOBase::Logger::connect(void (*callback)(void* param, const ::timeval& t, Priority priority, const char* msg), void* param)
{
	::Logger* logger = LOGGER::instance_ptr();
	if (!logger)
		return false;

	return logger->connect(callback,param);
}

bool OOBase::Logger::disconnect(void (*callback)(void* param, const ::timeval& t, Priority priority, const char* msg), void* param)
{
	::Logger* logger = LOGGER::instance_ptr();
	if (!logger)
		return false;

	return logger->disconnect(callback,param);
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
	::Logger* logger = LOGGER::instance_ptr();
	if (logger)
	{
		ScopedArrayPtr<char> ptr;
		if (OOBase::vprintf(ptr,fmt,args) == 0)
			logger->log(priority,ptr.get());
		else
			logger->log(priority,fmt);
	}
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

	ScopedArrayPtr<char> msg;
	int err = OOBase::vprintf(msg,fmt,args);

	va_end(args);

	::Logger* logger = LOGGER::instance_ptr();
	if (logger && err == 0)
	{
		if (m_pszFilename)
		{
			const char* pszSrcFile = logger->src();
			size_t s=0;
			for (;;)
			{
#if defined(_WIN32)
				size_t s1 = s;
				while (pszSrcFile[s1] == m_pszFilename[s1] && pszSrcFile[s1] != '\0' && pszSrcFile[s1] != '\\')
					++s1;

				if (pszSrcFile[s1] == '\\')
					s = s1+1;
				else
					break;
#else
				size_t s1 = s;
				while (pszSrcFile[s1] == m_pszFilename[s1] && pszSrcFile[s1] != '\0' && pszSrcFile[s1] != '/')
					++s1;

				if (pszSrcFile[s1] == '/')
					s = s1+1;
				else
					break;
#endif				
			}
			m_pszFilename += s;
		}

		ScopedArrayPtr<char> header;
		if (OOBase::printf(header,"%s(%u): %s",m_pszFilename,m_nLine,msg.get()) == 0)
			logger->log(m_priority,header.get());
	}
}

#if defined(_WIN32)

namespace
{
	void onSysLog(void* p, const ::timeval&, OOBase::Logger::Priority priority, const char* msg)
	{
		if (priority > OOBase::Logger::Debug)
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

			OOBase::UniquePtr<TOKEN_USER,OOBase::ThreadLocalAllocator> ptrSIDProcess;
			PSID psid = NULL;
			OOBase::Win32::SmartHandle hProcessToken;

			if (OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hProcessToken))
			{
				if (OOBase::Win32::GetTokenInfo(hProcessToken,TokenUser,ptrSIDProcess) == 0)
					psid = ptrSIDProcess->User.Sid;
			}

			const char* arrBufs[2] = { msg, NULL };
			ReportEventA((HANDLE)p,wType,0,0,psid,1,0,arrBufs,NULL);
		}
	}

	void formatMsg(OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512>& out, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		const char* tag = "Error: ";

		switch (priority)
		{
		case OOBase::Logger::Warning:
			tag = "Warning: ";
			break;

		case OOBase::Logger::Information:
			tag = "";
			break;

		case OOBase::Logger::Debug:
			tag = "Debug: ";
			break;

		case OOBase::Logger::Error:
		default:
			// Defaults;
			break;
		}

		OOBase::printf(out,"[%.02lu:%.02lu:%.02lu.%06lu] %s%s\n",t.tv_sec / 3600,t.tv_sec / 60 % 60,t.tv_sec % 60,t.tv_usec,tag,msg);
	}

	void onDebug(void*, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512> out;
		formatMsg(out,t,priority,msg);

		OutputDebugStringA(out.get());
	}

	WORD get_console_attrs(HANDLE h)
	{
		CONSOLE_SCREEN_BUFFER_INFO info;
		info.wAttributes = 0;
		GetConsoleScreenBufferInfo(h,&info);
		return info.wAttributes;
	}

	void onConsole(DWORD nStdHandle, uintptr_t param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Logger::Priority min = static_cast<OOBase::Logger::Priority>((param & 0xFF00) >> 8);
		OOBase::Logger::Priority max = static_cast<OOBase::Logger::Priority>(param & 0xFF);

		if (priority >= min && priority <= max)
		{
			HANDLE h = GetStdHandle(nStdHandle);
			OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512> out;
			formatMsg(out,t,priority,msg);

			WORD attrs;
			switch (priority)
			{
			case OOBase::Logger::Warning:
				attrs = get_console_attrs(h);
				SetConsoleTextAttribute(h,FOREGROUND_RED | FOREGROUND_GREEN);
				OOBase::stdout_write(out.get());
				SetConsoleTextAttribute(h,attrs);
				break;

			case OOBase::Logger::Information:
				OOBase::stdout_write(out.get());
				break;

			case OOBase::Logger::Debug:
				attrs = get_console_attrs(h);
				SetConsoleTextAttribute(h,FOREGROUND_GREEN);
				OOBase::stdout_write(out.get());
				SetConsoleTextAttribute(h,attrs);
				break;

			case OOBase::Logger::Error:
			default:
				attrs = get_console_attrs(h);
				SetConsoleTextAttribute(h,FOREGROUND_RED);
				OOBase::stdout_write(out.get());
				SetConsoleTextAttribute(h,attrs);
				break;
			}
		}
	}

	void onStdout(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		onConsole(STD_OUTPUT_HANDLE,reinterpret_cast<uintptr_t>(param),t,priority,msg);
	}

	void onStderr(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		onConsole(STD_ERROR_HANDLE,reinterpret_cast<uintptr_t>(param),t,priority,msg);
	}
}

int OOBase::Logger::connect_system_log(const char* name, const char*)
{
	wchar_t szPath[MAX_PATH];
	if (!GetModuleFileNameW(NULL,szPath,MAX_PATH))
		return GetLastError();

	// Create the relevant registry keys if they don't already exist
	OOBase::ScopedString strName;
	int err = strName.printf("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",name);
	if (err != 0)
		return err;

	HKEY hk;
	DWORD dwDisp;
	if (RegCreateKeyExA(HKEY_LOCAL_MACHINE,strName.c_str(),0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&hk,&dwDisp) == ERROR_SUCCESS)
	{
		RegSetValueExW(hk,L"EventMessageFile",0,REG_SZ,(LPBYTE)szPath,(DWORD)(wcslen(szPath)+1)*sizeof(wchar_t));

		DWORD dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
		RegSetValueExW(hk,L"TypesSupported",0,REG_DWORD,(LPBYTE)&dwData,sizeof(DWORD));

		RegCloseKey(hk);
	}

	// Get our event source - We leak this on purpose
	HANDLE hLog = RegisterEventSourceA(NULL,name);
	if (!hLog)
		return GetLastError();

	return Logger::connect(&onSysLog,hLog) ? 0 : system_error();
}

bool OOBase::Logger::connect_debug_log()
{
	return Logger::connect(&onDebug,NULL);
}

bool OOBase::Logger::connect_stdout_log(Priority min, Priority max)
{
	uintptr_t mask = (uintptr_t(min) << 8) | max;

	return Logger::connect(&onStdout,reinterpret_cast<void*>(mask));
}

bool OOBase::Logger::connect_stderr_log(Priority min, Priority max)
{
	uintptr_t mask = (uintptr_t(min) << 8) | max;

	return Logger::connect(&onStderr,reinterpret_cast<void*>(mask));
}

#elif defined(HAVE_UNISTD_H)

#if defined(HAVE_ASL_H)

#error Implement ASL logger

#elif defined(HAVE_SYSLOG_H)

namespace
{
	void onSysLog(void*, const ::timeval&, OOBase::Logger::Priority priority, const char* msg)
	{
		int wType = LOG_ERR;
		switch (priority)
		{
		case OOBase::Logger::Error:
			wType = LOG_ERR;
			break;

		case OOBase::Logger::Warning:
			wType = LOG_WARNING;
			break;

		case OOBase::Logger::Information:
			wType = LOG_INFO;
			break;

		case OOBase::Logger::Debug:
			wType = LOG_DEBUG;
			break;

		default:
			break;
		}

		syslog(wType,"%s",msg);
	}
}

int OOBase::Logger::connect_system_log(const char* name, const char* category)
{
	struct
	{
		const char* name;
		int id;
	} facilitynames[] = {
		{ "daemon", LOG_DAEMON },
		{ "user", LOG_USER },
		{ "local0", LOG_LOCAL0 },
		{ "local1", LOG_LOCAL1 },
		{ "local2", LOG_LOCAL2 },
		{ "local3", LOG_LOCAL3 },
		{ "local4", LOG_LOCAL4 },
		{ "local5", LOG_LOCAL5 },
		{ "local6", LOG_LOCAL6 },
		{ "local7", LOG_LOCAL7 },
		{ NULL, -1 }
	};
	int id = LOG_USER;
	for (size_t i = 0; category && facilitynames[i].name; ++i)
	{
		if (strcmp(category,facilitynames[i].name) == 0)
		{
			id = facilitynames[i].id;
			break;
		}
	}
	openlog(name,LOG_NDELAY,id | LOG_CONS | LOG_PID);

	return Logger::connect(&onSysLog,NULL) ? 0 : system_error();
}

#endif // defined(HAVE_SYSLOG_H)

namespace
{
	void formatMsg(OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512>& out, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg, bool use_colour)
	{
		const char* on_col = "\x1b[31m";
		const char* off_col = "\x1b[0m";
		const char* tag = "Error: ";

		switch (priority)
		{
		case OOBase::Logger::Warning:
			on_col = "\x1b[33m";
			tag = "Warning: ";
			break;

		case OOBase::Logger::Information:
			tag = "";
			use_colour = false;
			break;

		case OOBase::Logger::Debug:
			on_col = "\x1b[32m";
			tag = "Debug: ";
			break;

		case OOBase::Logger::Error:
		default:
			// Defaults;
			break;
		}

		if (use_colour)
			OOBase::printf(out,"%s[%.02lu:%.02lu:%.02lu.%06lu] %s%s%s\n",on_col,t.tv_sec / 3600,t.tv_sec / 60 % 60,t.tv_sec % 60,t.tv_usec,tag,msg,off_col);
		else
			OOBase::printf(out,"[%.02lu:%.02lu:%.02lu.%06lu] %s%s\n",t.tv_sec / 3600,t.tv_sec / 60 % 60,t.tv_sec % 60,t.tv_usec,tag,msg);
	}

	void onStdout(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Logger::Priority min = static_cast<OOBase::Logger::Priority>((reinterpret_cast<uintptr_t>(param) & 0xFF00) >> 8);
		OOBase::Logger::Priority max = static_cast<OOBase::Logger::Priority>((reinterpret_cast<uintptr_t>(param) & 0xFF));

		if (priority >= min && priority <= max)
		{
			bool use_colour = (reinterpret_cast<uintptr_t>(param) & 0x10000) != 0;

			OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512> out;
			formatMsg(out,t,priority,msg,use_colour);

			OOBase::stdout_write(out.get());
		}
	}

	void onStderr(void* param, const ::timeval& t, OOBase::Logger::Priority priority, const char* msg)
	{
		OOBase::Logger::Priority min = static_cast<OOBase::Logger::Priority>((reinterpret_cast<uintptr_t>(param) & 0xFF00) >> 8);
		OOBase::Logger::Priority max = static_cast<OOBase::Logger::Priority>((reinterpret_cast<uintptr_t>(param) & 0xFF));

		if (priority >= min && priority <= max)
		{
			bool use_colour = (reinterpret_cast<uintptr_t>(param) & 0x10000) != 0;

			OOBase::ScopedArrayPtr<char,OOBase::ThreadLocalAllocator,512> out;
			formatMsg(out,t,priority,msg,use_colour);

			OOBase::stdout_write(out.get());
		}
	}
}

bool OOBase::Logger::connect_stdout_log(Priority min, Priority max)
{
	uintptr_t use_colour = (((::getenv("TERM") != NULL) && (isatty(STDERR_FILENO) == 1)) ? 0x10000 : 0);
	uintptr_t mask = use_colour | (uintptr_t(min) << 8) | max;

	return Logger::connect(&onStdout,reinterpret_cast<void*>(mask));
}

bool OOBase::Logger::connect_stderr_log(Priority min, Priority max)
{
	uintptr_t use_colour = (((::getenv("TERM") != NULL) && (isatty(STDERR_FILENO) == 1)) ? 0x10000 : 0);
	uintptr_t mask = use_colour | (uintptr_t(min) << 8) | max;

	return Logger::connect(&onStderr,reinterpret_cast<void*>(mask));
}

bool OOBase::Logger::connect_debug_log()
{
	return true;
}

#endif
