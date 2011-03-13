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
#include "Win32Impl.h"

namespace OOBase
{
	// This is the critical failure hook
	void CriticalFailure(const char* msg);
}

OOBase::string OOBase::system_error_text(int err)
{
#if defined(_WIN32)
	return Win32::FormatMessage(err);
#else

	void* TODO: // Catch!!

	ostringstream out;
	out << "(" << err << ") ";

#if defined(HAVE_PTHREADS_H)
	char szBuf[256] = {0};
	strerror_r(szBuf,sizeof(szBuf),err);
	out << szBuf;
#elif defined(HAVE_TR_24731)
	char szBuf[256] = {0};
	strerror_s(szBuf,sizeof(szBuf),err);
	out << szBuf;
#else
	out << ::strerror(err);
#endif

	return out.str();

#endif
}

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, int err)
{
	CallCriticalFailure(pszFile,nLine,OOBase::system_error_text(err).c_str());
}

#if defined(_WIN32)

void OOBase::CallCriticalFailureMem(const char* pszFile, unsigned int nLine)
{
	CallCriticalFailure(pszFile,nLine,ERROR_OUTOFMEMORY);
}

#elif defined(HAVE_UNISTD_H)

void OOBase::CallCriticalFailureMem(const char* pszFile, unsigned int nLine)
{
	CallCriticalFailure(pszFile,nLine,ENOMEM);
}

#else
#error Fix me!
#endif

void OOBase::CallCriticalFailure(const char* pszFile, unsigned int nLine, const char* msg)
{
	ostringstream out;
	out << pszFile << "(" << nLine << "): " << msg;
	OOBase::CriticalFailure(out.str().c_str());
	abort();
}
