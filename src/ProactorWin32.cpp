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

#include "ProactorWin32.h"

#if defined(_WIN32)

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Thread.h"

OOBase::Proactor* OOBase::Proactor::create(int& err)
{
	detail::ProactorWin32* proactor = new (std::nothrow) detail::ProactorWin32();
	if (!proactor)
		err = ERROR_OUTOFMEMORY;
	return proactor;
}

void OOBase::Proactor::destroy(Proactor* proactor)
{
	if (proactor)
	{
		proactor->stop();
		delete proactor;
	}
}

OOBase::detail::ProactorWin32::ProactorWin32() : Proactor(),
		m_hPort(NULL),
		m_bound(0)
{
	Win32::WSAStartup();
}

OOBase::detail::ProactorWin32::~ProactorWin32()
{
}

int OOBase::detail::ProactorWin32::new_overlapped(Overlapped*& pOv, pfnCompletion_t callback)
{
	pOv = static_cast<Overlapped*>(internal_allocate(sizeof(Overlapped),alignof<Overlapped>::value));
	if (!pOv)
		return ERROR_OUTOFMEMORY;
	
	ZeroMemory(pOv,sizeof(Overlapped));
	pOv->m_callback = callback;
	pOv->m_pProactor = this;
	pOv->m_refcount = 1;

	return 0;
}

void OOBase::detail::ProactorWin32::delete_overlapped(Overlapped* pOv)
{
	if (InterlockedDecrement(&pOv->m_refcount) == 0)
		internal_free(pOv);
}

int OOBase::detail::ProactorWin32::bind(HANDLE hFile)
{
	Guard<SpinLock> guard(m_lock);

	HANDLE hPort = CreateIoCompletionPort(hFile,m_hPort,(ULONG_PTR)hFile,0);
	if (!hPort)
		return GetLastError();
	if (!m_hPort)
		m_hPort = hPort;

	++m_bound;

	return 0;
}

void OOBase::detail::ProactorWin32::unbind()
{
	Guard<SpinLock> guard(m_lock);

	if (--m_bound == 0)
		CloseHandle(m_hPort);
}

int OOBase::detail::ProactorWin32::run(int& err, const Timeout& timeout)
{
	Guard<SpinLock> guard(m_lock);
	
	if (!m_hPort)
	{
		m_hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
		if (!m_hPort)
		{
			err = GetLastError();
			return -1;
		}

		++m_bound;
	}
	
	while (m_bound > 0)
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

	err = 0;
	return 1;
}

void OOBase::detail::ProactorWin32::stop()
{
	unbind();
}

int OOBase::detail::ProactorWin32::restart()
{
	int err = 0;

	Guard<SpinLock> guard(m_lock);

	if (!m_hPort)
	{
		m_hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,0);
		if (!m_hPort)
			err = GetLastError();
		else
			++m_bound;
	}

	return err;
}

void* OOBase::detail::ProactorWin32::internal_allocate(size_t bytes, size_t align)
{
	return OOBase::CrtAllocator::allocate(bytes,align);
}

void* OOBase::detail::ProactorWin32::internal_reallocate(void* ptr, size_t bytes, size_t align)
{
	return OOBase::CrtAllocator::reallocate(ptr,bytes,align);
}

void OOBase::detail::ProactorWin32::internal_free(void* ptr)
{
	OOBase::CrtAllocator::free(ptr);
}

#endif // _WIN32
