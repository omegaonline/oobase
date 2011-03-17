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

#include "../include/OOSvrBase/Proactor.h"

#if defined(_WIN32)

#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/Thread.h"

#include "Win32Socket.h"
#include "ProactorWin32.h"
#include "Win32Impl.h"

OOSvrBase::Win32::ProactorImpl::ProactorImpl() :
		OOSvrBase::Proactor(false),
		m_refcount(0)
{
	OOBase::Win32::WSAStartup();
}

OOSvrBase::Win32::ProactorImpl::~ProactorImpl()
{
	while (m_refcount > 0)
		OOBase::Thread::yield();
}

int OOSvrBase::Win32::ProactorImpl::new_overlapped(OOSvrBase::Win32::ProactorImpl::Overlapped*& pOv, pfnCompletion_t callback)
{
	void* TODO; // Use a free-list here
	
	pOv = new (std::nothrow) Overlapped;
	if (!pOv)
		return ERROR_OUTOFMEMORY;
	
	pOv->Internal = 0;
	pOv->InternalHigh = 0;
	pOv->Offset = 0;
	pOv->OffsetHigh = 0;
	pOv->Pointer = NULL;
	pOv->hEvent = NULL;
	pOv->m_callback = callback;
	pOv->m_extras[0] = 0;
	pOv->m_extras[1] = 0;
	pOv->m_extras[2] = 0;
	pOv->m_extras[3] = 0;
	pOv->m_extras[4] = 0;
	
	return 0;
}

void OOSvrBase::Win32::ProactorImpl::delete_overlapped(OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	void* TODO; // Use a free-list here
	
	delete pOv;
}

int OOSvrBase::Win32::ProactorImpl::bind(HANDLE hFile)
{
	
}

#endif // _WIN32
