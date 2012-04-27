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

#include "ProactorWin32.h"

#if defined(_WIN32)

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Thread.h"

OOSvrBase::detail::ProactorWin32::ProactorWin32() :
		Proactor(false),
		m_hPort(NULL),
		m_outstanding(1)
{
	OOBase::Win32::WSAStartup();
}

OOSvrBase::detail::ProactorWin32::~ProactorWin32()
{
}

int OOSvrBase::detail::ProactorWin32::new_overlapped(Overlapped*& pOv, pfnCompletion_t callback)
{
	pOv = static_cast<Overlapped*>(OOBase::HeapAllocator::allocate(sizeof(Overlapped)));
	if (!pOv)
		return ERROR_OUTOFMEMORY;
	
	ZeroMemory(pOv,sizeof(Overlapped));
	pOv->m_callback = callback;
	pOv->m_pProactor = this;
	pOv->m_refcount = 2;

	return 0;
}

void OOSvrBase::detail::ProactorWin32::delete_overlapped(Overlapped* pOv)
{
	if (InterlockedDecrement(&pOv->m_refcount) == 0)
		OOBase::HeapAllocator::free(pOv);
}

int OOSvrBase::detail::ProactorWin32::bind(HANDLE hFile)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	HANDLE hPort = CreateIoCompletionPort(hFile,m_hPort,(ULONG_PTR)hFile,0);
	if (!hPort)
		return GetLastError();
	if (!m_hPort)
		m_hPort = hPort;

	++m_outstanding;

	return 0;
}

void OOSvrBase::detail::ProactorWin32::unbind(HANDLE /*hFile*/)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	if (--m_outstanding == 0)
	{
		CloseHandle(m_hPort);
		m_hPort = NULL;
	}
}

int OOSvrBase::detail::ProactorWin32::run(int& err, const OOBase::Timeout& timeout)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (!guard.acquire(timeout))
		return 0;

	if (!m_hPort)
	{
		m_hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
		if (!m_hPort)
		{
			err = GetLastError();
			return -1;
		}
	}
	
	do
	{
		guard.release();

		DWORD dwErr = 0;
		DWORD dwBytes = 0;
		ULONG_PTR key = 0;
		LPOVERLAPPED lpOv = NULL;
		if (!GetQueuedCompletionStatus(m_hPort,&dwBytes,&key,&lpOv,timeout.millisecs()))
		{
			dwErr = GetLastError();
			if (lpOv)
			{
				// The operation lpOv failed
				Overlapped* pOv = static_cast<Overlapped*>(lpOv);
				(*pOv->m_callback)((HANDLE)key,dwBytes,dwErr,pOv);
			}
			else if (dwErr == WAIT_TIMEOUT)
				return 0;
			else
			{
				err = dwErr;
				return -1;
			}
		}
		else
		{
			// lpOV succeeded
			Overlapped* pOv = static_cast<Overlapped*>(lpOv);
			(*pOv->m_callback)((HANDLE)key,dwBytes,dwErr,pOv);
		}

		guard.acquire();
	}
	while (m_outstanding > 0);

	err = 0;
	return 1;
}

void OOSvrBase::detail::ProactorWin32::stop()
{
	unbind(NULL);
}

#endif // _WIN32
