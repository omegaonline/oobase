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

#ifndef OOSVRBASE_LOGGER_H_INCLUDED_
#define OOSVRBASE_LOGGER_H_INCLUDED_

#include "../config-base.h"

namespace OOSvrBase
{
	namespace Logger
	{
		enum Priority
		{
			Error,
			Warning,
			Information,
			Debug
		};

		void open(const char* name, const char* pszSrcFile = NULL);
		void log(Priority priority, const char* fmt, ...);
		
#if !defined(DOXYGEN)
		struct filenum_t
		{
			filenum_t(Priority priority, const char* pszFilename, unsigned int nLine);
			void log(const char* fmt, ...);

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
#define LOG_DEBUG(expr) OOSvrBase::Logger::filenum_t(OOSvrBase::Logger::Debug,__FILE__,__LINE__).log expr
#define LOG_WARNING(expr) OOSvrBase::Logger::filenum_t(OOSvrBase::Logger::Warning,__FILE__,__LINE__).log expr
#define LOG_ERROR(expr) OOSvrBase::Logger::filenum_t(OOSvrBase::Logger::Error,__FILE__,__LINE__).log expr
#define LOG_ERROR_RETURN(expr,ret_val) return (LOG_ERROR(expr),ret_val)
#endif

#endif // OOSVRBASE_LOGGER_H_INCLUDED_
