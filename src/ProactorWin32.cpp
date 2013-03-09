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
	detail::ProactorWin32* proactor = NULL;
	if (!OOBase::CrtAllocator::allocate_new(proactor))
		err = ERROR_OUTOFMEMORY;
	return proactor;
}

void OOBase::Proactor::destroy(Proactor* proactor)
{
	if (proactor)
	{
		proactor->stop();
		OOBase::CrtAllocator::delete_free(static_cast<detail::ProactorWin32*>(proactor));
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
	pOv = static_cast<Overlapped*>(m_allocator.allocate(sizeof(Overlapped),alignment_of<Overlapped>::value));
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
		m_allocator.free(pOv);
}

int OOBase::detail::ProactorWin32::bind(HANDLE hFile)
{
	Guard<SpinLock> guard(m_lock);

	if (hFile != INVALID_HANDLE_VALUE || !m_hPort)
	{
		HANDLE hPort = CreateIoCompletionPort(hFile,m_hPort,(ULONG_PTR)hFile,0);
		if (!hPort)
			return GetLastError();
		m_hPort = hPort;

		++m_bound;
	}

	return 0;
}

void OOBase::detail::ProactorWin32::unbind()
{
	Guard<SpinLock> guard(m_lock);

	if (--m_bound == 0)
	{
		::CloseHandle(m_hPort);
		m_hPort = NULL;
	}
}

int OOBase::detail::ProactorWin32::run(int& err, const Timeout& timeout)
{
	Guard<SpinLock> guard(m_lock);
	
	err = bind(INVALID_HANDLE_VALUE);
	if (err)
		return -1;
	
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

int OOBase::detail::ProactorWin32::post_completion(DWORD dwBytes, ULONG_PTR dwKey, Overlapped* pOv)
{
	Guard<SpinLock> guard(m_lock);

	if (!m_hPort)
	{
		HANDLE hPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,m_hPort,0,0);
		if (!hPort)
			return GetLastError();
		m_hPort = hPort;

		++m_bound;
	}

	if (!PostQueuedCompletionStatus(m_hPort,dwBytes,dwKey,pOv))
		return GetLastError();

	return 0;
}

void OOBase::detail::ProactorWin32::stop()
{
	unbind();
}

int OOBase::detail::ProactorWin32::restart()
{
	return bind(INVALID_HANDLE_VALUE);
}

namespace
{
	class InternalWaitAcceptor : public OOBase::RefCounted
	{
	public:
		InternalWaitAcceptor(OOBase::detail::ProactorWin32* pProactor, HANDLE hObject, void* param, OOBase::Proactor::wait_object_callback_t callback, DWORD dwMilliseconds);

		int start();
		void stop();

	private:
		OOBase::detail::ProactorWin32*           m_pProactor;
		void*                                    m_param;
		OOBase::Proactor::wait_object_callback_t m_callback;
		HANDLE                                   m_hObject;
		DWORD                                    m_dwTimeout;
		HANDLE                                   m_hWait;

		static void CALLBACK onWait(void* param, BOOLEAN TimerOrWaitFired);
		static void onWait2(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);

		void destroy()
		{
			OOBase::CrtAllocator::delete_free(this);
		}
	};

	class WaitAcceptor : public OOBase::Acceptor
	{
	public:
		WaitAcceptor();
		virtual ~WaitAcceptor();

		int bind(OOBase::detail::ProactorWin32* pProactor, HANDLE hObject, void* param, OOBase::Proactor::wait_object_callback_t callback, DWORD dwMilliseconds);

	private:
		void destroy()
		{
			OOBase::CrtAllocator::delete_free(this);
		}

		InternalWaitAcceptor* m_pAcceptor;
	};
}

WaitAcceptor::WaitAcceptor() :
		m_pAcceptor(NULL)
{ }

WaitAcceptor::~WaitAcceptor()
{
	if (m_pAcceptor)
		m_pAcceptor->stop();
}

int WaitAcceptor::bind(OOBase::detail::ProactorWin32* pProactor, HANDLE hObject, void* param, OOBase::Proactor::wait_object_callback_t callback, DWORD dwMilliseconds)
{
	if (!OOBase::CrtAllocator::allocate_new<InternalWaitAcceptor>(m_pAcceptor,pProactor,hObject,param,callback,dwMilliseconds))
		return ERROR_OUTOFMEMORY;

	int err = m_pAcceptor->start();
	if (err != 0)
	{
		m_pAcceptor->stop();
		m_pAcceptor = NULL;
	}

	return err;
}

InternalWaitAcceptor::InternalWaitAcceptor(OOBase::detail::ProactorWin32* pProactor, HANDLE hObject, void* param, OOBase::Proactor::wait_object_callback_t callback, DWORD dwMilliseconds) :
		m_pProactor(pProactor),
		m_param(param),
		m_callback(callback),
		m_hObject(hObject),
		m_dwTimeout(dwMilliseconds),
		m_hWait(NULL)
{
}

int InternalWaitAcceptor::start()
{
	addref();

	int err = 0;
	if (!RegisterWaitForSingleObject(&m_hWait,m_hObject,&onWait,this,m_dwTimeout,WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE))
	{
		err = GetLastError();
		release();
	}

	return err;
}

void InternalWaitAcceptor::stop()
{
	if (m_hWait)
	{
		OOBase::Win32::SmartHandle event(::CreateEvent(NULL,TRUE,FALSE,NULL));

		int err = 0;
		if (!UnregisterWaitEx(m_hWait,event.is_valid() ? (HANDLE)event : INVALID_HANDLE_VALUE))
			err = GetLastError();

		if (!err || err == ERROR_IO_PENDING)
		{
			if (event.is_valid())
				WaitForSingleObject(event,INFINITE);
		}
	}

	release();
}

void InternalWaitAcceptor::onWait(void* param, BOOLEAN TimerOrWaitFired)
{
	InternalWaitAcceptor* pThis = static_cast<InternalWaitAcceptor*>(param);

	// Done with the wait
	int err = 0;
	if (!UnregisterWaitEx(pThis->m_hWait,NULL))
		err = GetLastError();
	else
	{
		pThis->m_hWait = NULL;
		
		OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
		err = pThis->m_pProactor->new_overlapped(pOv,&onWait2);
		if (!err)
		{
			pThis->addref();

			pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(pThis);
			pOv->m_extras[1] = TimerOrWaitFired;
			err = pThis->m_pProactor->post_completion(0,0,pOv);
			if (err)
			{
				pThis->m_pProactor->delete_overlapped(pOv);
				pThis->release();
			}
		}
	}

	if (err)
		pThis->m_callback(pThis->m_param,pThis->m_hObject,false,err);

	pThis->release();
}

void InternalWaitAcceptor::onWait2(HANDLE /*handle*/, DWORD /*dwBytes*/, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	InternalWaitAcceptor* pThis = reinterpret_cast<InternalWaitAcceptor*>(pOv->m_extras[0]);
	BOOLEAN TimerOrWaitFired = static_cast<BOOLEAN>(pOv->m_extras[1]);

	pThis->m_pProactor->delete_overlapped(pOv);

	(*pThis->m_callback)(pThis->m_param,pThis->m_hObject,TimerOrWaitFired == TRUE,dwErr);

	pThis->release();
}

OOBase::Acceptor* OOBase::detail::ProactorWin32::wait_for_object(void* param, wait_object_callback_t callback, HANDLE hObject, int& err, ULONG dwMilliseconds)
{
	WaitAcceptor* pAcceptor = NULL;
	if (!OOBase::CrtAllocator::allocate_new(pAcceptor))
		err = ERROR_OUTOFMEMORY;
	else
	{
		if (err == 0)
			err = pAcceptor->bind(this,hObject,param,callback,dwMilliseconds);

		if (err != 0)
		{
			pAcceptor->release();
			pAcceptor = NULL;
		}
	}

	return pAcceptor;
}

#endif // _WIN32
