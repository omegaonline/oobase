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

#include "../include/OOBase/Posix.h"

#if defined(HAVE_UNISTD_H)

#include "../include/OOBase/CustomNew.h"

OOBase::POSIX::pw_info::pw_info(uid_t uid) :
		m_pwd(NULL), m_buf_len(1024)
{
#if defined(_SC_GETPW_R_SIZE_MAX) && defined(HAVE_UNISTD_H)
	m_buf_len = sysconf(_SC_GETPW_R_SIZE_MAX) + 1;
#endif

	// _SC_GETPW_R_SIZE_MAX is defined on Mac OS X. However,
	// sysconf(_SC_GETPW_R_SIZE_MAX) returns an error. Therefore, the
	// constant is used as below when error was retured.
	if (m_buf_len <= 0)
		m_buf_len = 1024;

	m_buffer = static_cast<char*>(OOBase::HeapAllocate(m_buf_len));
	if (!m_buffer)
		OOBase_CallCriticalFailure(ENOMEM);

	if (::getpwuid_r(uid,&m_pwd2,m_buffer,m_buf_len,&m_pwd) != 0)
		m_pwd = NULL;
}

OOBase::POSIX::pw_info::pw_info(const char* uname) :
		m_pwd(NULL), m_buf_len(1024)
{
#if defined(_SC_GETPW_R_SIZE_MAX) && defined(HAVE_UNISTD_H)
	m_buf_len = sysconf(_SC_GETPW_R_SIZE_MAX) + 1;
#endif

	// _SC_GETPW_R_SIZE_MAX is defined on Mac OS X. However,
	// sysconf(_SC_GETPW_R_SIZE_MAX) returns an error. Therefore, the
	// constant is used as below when error was retured.
	if (m_buf_len <= 0)
		m_buf_len = 1024;

	m_buffer = static_cast<char*>(OOBase::HeapAllocate(m_buf_len));
	if (!m_buffer)
		OOBase_CallCriticalFailure(ENOMEM);

	if (::getpwnam_r(uname,&m_pwd2,m_buffer,m_buf_len,&m_pwd) != 0)
		m_pwd = NULL;
}

#endif
