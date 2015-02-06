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

#ifndef OOBASE_LOGGER_H_INCLUDED_
#define OOBASE_LOGGER_H_INCLUDED_

#include "Base.h"

#if defined(_WIN32)
#if defined(__MINGW32__) && defined(_WINSOCKAPI_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#endif

namespace OOBase
{
	namespace Logger
	{
		enum Priority
		{
			Error = 3,
			Warning = 2,
			Information = 1,
			Debug = 0
		};

		int connect_stdout_log(Priority min = Debug, Priority max = Information);
		int connect_stderr_log(Priority min = Warning, Priority max = Error);
		int connect_debug_log();
		int connect_system_log(const char* name, const char* category);

		void set_source_file(const char* pszSrcFile);

		int connect(void (*callback)(void* param, const ::timeval& t, Priority priority, const char* msg), void* param);
		bool disconnect(void (*callback)(void* param, const ::timeval& t, Priority priority, const char* msg), void* param);

		void log(Priority priority, const char* fmt, ...) OOBASE_FORMAT(printf,2,3);
		void log(Priority priority, const char* fmt, va_list args);

#if !defined(DOXYGEN)
		struct filenum_t
		{
			filenum_t(Priority priority, const char* pszFilename, unsigned int nLine);

			void log(const char* fmt, ...) OOBASE_FORMAT(printf,2,3);

			Priority     m_priority;
			const char*  m_pszFilename;
			unsigned int m_nLine;
		};
#endif // !defined(DOXYGEN)
	}
}

#if defined(DOXYGEN)
/// Log a Debug message including file and line number
#define LOG_DEBUG(expr)
/// Log a Warning message including file and line number
#define LOG_WARNING(expr)
/// Log an Error message including file and line number
#define LOG_ERROR(expr)
/// Log an Error message including file and line number, and return ret_val.
#define LOG_ERROR_RETURN(expr,ret_val)
#else
#define LOG_DEBUG(expr) OOBase::Logger::filenum_t(OOBase::Logger::Debug,__FILE__,__LINE__).log expr
#define LOG_WARNING(expr) OOBase::Logger::filenum_t(OOBase::Logger::Warning,__FILE__,__LINE__).log expr
#define LOG_ERROR(expr) OOBase::Logger::filenum_t(OOBase::Logger::Error,__FILE__,__LINE__).log expr
#define LOG_ERROR_RETURN(expr,ret_val) return (LOG_ERROR(expr),ret_val)
#define LOG_WARNING_RETURN(expr,ret_val) return (LOG_WARNING(expr),ret_val)
#endif

#endif // OOBASE_LOGGER_H_INCLUDED_
