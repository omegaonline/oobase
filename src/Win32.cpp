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

#include "Win32Impl.h"

#include "../include/OOBase/Destructor.h"

#if defined(_WIN32)

namespace
{
	class Win32Thunk
	{
	public:
		static Win32Thunk& instance();

		typedef BOOL (__stdcall *pfn_InitOnceExecuteOnce)(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context);
		pfn_InitOnceExecuteOnce m_InitOnceExecuteOnce;
		static BOOL __stdcall impl_InitOnceExecuteOnce(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context);
		
		typedef void (__stdcall *pfn_InitializeSRWLock)(SRWLOCK* SRWLock);
		pfn_InitializeSRWLock m_InitializeSRWLock;
		static void __stdcall impl_InitializeSRWLock(SRWLOCK* SRWLock);

		typedef void (__stdcall *pfn_AcquireSRWLockShared)(SRWLOCK* SRWLock);
		pfn_AcquireSRWLockShared m_AcquireSRWLockShared;
		static void __stdcall impl_AcquireSRWLockShared(SRWLOCK* SRWLock);

		typedef void (__stdcall *pfn_AcquireSRWLockExclusive)(SRWLOCK* SRWLock);
		pfn_AcquireSRWLockExclusive m_AcquireSRWLockExclusive;
		static void __stdcall impl_AcquireSRWLockExclusive(SRWLOCK* SRWLock);

		typedef BOOL (__stdcall *pfn_TryAcquireSRWLockShared)(SRWLOCK* SRWLock);
		pfn_TryAcquireSRWLockShared m_TryAcquireSRWLockShared;
		static BOOL __stdcall impl_TryAcquireSRWLockShared(SRWLOCK* SRWLock);

		typedef BOOL (__stdcall *pfn_TryAcquireSRWLockExclusive)(SRWLOCK* SRWLock);
		pfn_TryAcquireSRWLockExclusive m_TryAcquireSRWLockExclusive;
		static BOOL __stdcall impl_TryAcquireSRWLockExclusive(SRWLOCK* SRWLock);

		typedef void (__stdcall *pfn_ReleaseSRWLockShared)(SRWLOCK* SRWLock);
		pfn_ReleaseSRWLockShared m_ReleaseSRWLockShared;
		static void __stdcall impl_ReleaseSRWLockShared(SRWLOCK* SRWLock);

		typedef void (__stdcall *pfn_ReleaseSRWLockExclusive)(SRWLOCK* SRWLock);
		pfn_ReleaseSRWLockExclusive m_ReleaseSRWLockExclusive;
		static void __stdcall impl_ReleaseSRWLockExclusive(SRWLOCK* SRWLock);

		typedef void (__stdcall *pfn_InitializeConditionVariable)(CONDITION_VARIABLE* ConditionVariable);
		pfn_InitializeConditionVariable m_InitializeConditionVariable;
		static void __stdcall impl_InitializeConditionVariable(CONDITION_VARIABLE* ConditionVariable);

		typedef BOOL (__stdcall *pfn_SleepConditionVariableCS)(CONDITION_VARIABLE* ConditionVariable, CRITICAL_SECTION* CriticalSection, DWORD dwMilliseconds);
		pfn_SleepConditionVariableCS m_SleepConditionVariableCS;

		typedef void (__stdcall *pfn_WakeConditionVariable)(CONDITION_VARIABLE* ConditionVariable);
		pfn_WakeConditionVariable m_WakeConditionVariable;
		static void __stdcall impl_WakeConditionVariable(CONDITION_VARIABLE* ConditionVariable);

		typedef void (__stdcall *pfn_WakeAllConditionVariable)(CONDITION_VARIABLE* ConditionVariable);
		pfn_WakeAllConditionVariable m_WakeAllConditionVariable;
		static void __stdcall impl_WakeAllConditionVariable(CONDITION_VARIABLE* ConditionVariable);

		typedef BOOL (__stdcall *pfn_BindIoCompletionCallback)(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG Flags);
		pfn_BindIoCompletionCallback m_BindIoCompletionCallback;

		typedef BOOLEAN (APIENTRY *pfn_RtlGenRandom)(PVOID, ULONG);
		pfn_RtlGenRandom m_RtlGenRandom;

		HANDLE  m_hHeap;
		
	private:
		Win32Thunk() : m_hHeap(NULL)
		{}

		static BOOL __stdcall init(INIT_ONCE*, void* i, void**);
		static void init_low_frag_heap(HMODULE hKernel32, HANDLE hHeap);
	};

	BOOL Win32Thunk::init(INIT_ONCE*, void* i, void**)
	{
		Win32Thunk* instance = static_cast<Win32Thunk*>(i);
		
		HMODULE hKernel32 = GetModuleHandleW(L"Kernel32.dll");
		if (hKernel32 != NULL)
		{
			instance->m_InitOnceExecuteOnce = (pfn_InitOnceExecuteOnce)(GetProcAddress(hKernel32,"InitOnceExecuteOnce"));
			
			instance->m_InitializeSRWLock = (pfn_InitializeSRWLock)(GetProcAddress(hKernel32,"InitializeSRWLock"));
			instance->m_AcquireSRWLockShared = (pfn_AcquireSRWLockShared)(GetProcAddress(hKernel32,"AcquireSRWLockShared"));
			instance->m_AcquireSRWLockExclusive = (pfn_AcquireSRWLockExclusive)(GetProcAddress(hKernel32,"AcquireSRWLockExclusive"));
			instance->m_TryAcquireSRWLockShared = (pfn_TryAcquireSRWLockShared)(GetProcAddress(hKernel32,"TryAcquireSRWLockShared"));
			instance->m_TryAcquireSRWLockExclusive = (pfn_TryAcquireSRWLockExclusive)(GetProcAddress(hKernel32,"TryAcquireSRWLockExclusive"));
			instance->m_ReleaseSRWLockShared = (pfn_ReleaseSRWLockShared)(GetProcAddress(hKernel32,"ReleaseSRWLockShared"));
			instance->m_ReleaseSRWLockExclusive = (pfn_ReleaseSRWLockExclusive)(GetProcAddress(hKernel32,"ReleaseSRWLockExclusive"));

			instance->m_InitializeConditionVariable = (pfn_InitializeConditionVariable)(GetProcAddress(hKernel32,"InitializeConditionVariable"));
			instance->m_SleepConditionVariableCS = (pfn_SleepConditionVariableCS)(GetProcAddress(hKernel32,"SleepConditionVariableCS"));
			instance->m_WakeConditionVariable = (pfn_WakeConditionVariable)(GetProcAddress(hKernel32,"WakeConditionVariable"));
			instance->m_WakeAllConditionVariable = (pfn_WakeAllConditionVariable)(GetProcAddress(hKernel32,"WakeAllConditionVariable"));

			instance->m_BindIoCompletionCallback = (pfn_BindIoCompletionCallback)(GetProcAddress(hKernel32,"BindIoCompletionCallback"));
		}
			
		if (!instance->m_InitOnceExecuteOnce)
			instance->m_InitOnceExecuteOnce = impl_InitOnceExecuteOnce;

		if (!instance->m_TryAcquireSRWLockExclusive)
		{
			instance->m_InitializeSRWLock = impl_InitializeSRWLock;
			instance->m_AcquireSRWLockShared = impl_AcquireSRWLockShared;
			instance->m_AcquireSRWLockExclusive = impl_AcquireSRWLockExclusive;
			instance->m_TryAcquireSRWLockShared = impl_TryAcquireSRWLockShared;
			instance->m_TryAcquireSRWLockExclusive = impl_TryAcquireSRWLockExclusive;
			instance->m_ReleaseSRWLockShared = impl_ReleaseSRWLockShared;
			instance->m_ReleaseSRWLockExclusive = impl_ReleaseSRWLockExclusive;
		}		

		if (!instance->m_InitializeConditionVariable ||
				!instance->m_SleepConditionVariableCS ||
				!instance->m_WakeConditionVariable ||
				!instance->m_WakeAllConditionVariable)
		{
			instance->m_InitializeConditionVariable = impl_InitializeConditionVariable;
			instance->m_SleepConditionVariableCS = NULL;
			instance->m_WakeConditionVariable = impl_WakeConditionVariable;
			instance->m_WakeAllConditionVariable = impl_WakeAllConditionVariable;
		}

		if (!instance->m_RtlGenRandom)
		{
			HMODULE hADVAPI32 = GetModuleHandleW(L"ADVAPI32.DLL");
			if (hADVAPI32)
				instance->m_RtlGenRandom = (pfn_RtlGenRandom)(GetProcAddress(hADVAPI32,"SystemFunction036"));
		}

		instance->m_hHeap = HeapCreate(0,0,0);
		if (!instance->m_hHeap)
			OOBase_CallCriticalFailure(GetLastError());

		init_low_frag_heap(hKernel32,instance->m_hHeap);

#if defined(_MSC_VER)
		init_low_frag_heap(hKernel32,(HANDLE)_get_heap_handle());
#endif

		return TRUE;
	}

	Win32Thunk& Win32Thunk::instance()
	{
		static Win32Thunk s_instance;
		static INIT_ONCE key = {0};
		if (!impl_InitOnceExecuteOnce(&key,&init,&s_instance,NULL))
			OOBase_CallCriticalFailure(GetLastError());
		
		return s_instance;
	}

	void Win32Thunk::init_low_frag_heap(HMODULE hKernel32, HANDLE hHeap)
	{
#if (_WIN32_WINNT >= 0x0501)
		typedef BOOL (__stdcall *pfn_HeapSetInformation)(HANDLE HeapHandle, HEAP_INFORMATION_CLASS HeapInformationClass, void* HeapInformation, SIZE_T HeapInformationLength);

		pfn_HeapSetInformation pfn = (pfn_HeapSetInformation)(GetProcAddress(hKernel32,"HeapSetInformation"));
		if (pfn)
		{
			ULONG ulEnableLFH = 2;
			(*pfn)(hHeap,HeapCompatibilityInformation,&ulEnableLFH, sizeof(ulEnableLFH));
		}
#endif
	}

	HANDLE get_mutex(void* v)
	{
		wchar_t buf[256] = {0};
		wsprintfW(buf,L"Local\\OOBase__Win32Thunk__Once__Mutex__%lu__%p",GetCurrentProcessId(),v);
		HANDLE h = CreateMutexW(NULL,FALSE,buf);
		if (!h)
			OOBase_CallCriticalFailure(GetLastError());

		return h;
	}

	BOOL Win32Thunk::impl_InitOnceExecuteOnce(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context)
	{
		static_assert(sizeof(LONG) <= sizeof(INIT_ONCE),"Refer to maintainters");

		// *check == 0  <- Start state
		// *check == 1  <- Init being called...
		// *check == 2  <- Init failed...
		// *check == 3  <- Init succeeded...

		LONG* check = reinterpret_cast<LONG*>(&InitOnce->Ptr);
		switch (InterlockedCompareExchange(check,1,0))
		{
		case 2:
			return FALSE;

		case 3:
			return TRUE;

		default:
			break;
		}

		BOOL bReturn = TRUE;
		
		// Acquire mutex
		OOBase::Win32::SmartHandle mutex(get_mutex(check));
		if (WaitForSingleObject(mutex,INFINITE) != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());

		if (*check == 1)
		{
			// Assume success (this avoids recursion)
			InterlockedExchange(check,3);

			// First in...
			if (!(*InitFn)(InitOnce,Parameter,Context))
				InterlockedExchange(check,2);
		}
		else
		{
			// If we get here then we are not the first thread in, and the first thread failed!
			bReturn = (*check == 3 ? TRUE : FALSE);
		}
									
		// And release
		if (!ReleaseMutex(mutex))
			OOBase_CallCriticalFailure(GetLastError());
		
		return bReturn;
	}

	void Win32Thunk::impl_InitializeSRWLock(SRWLOCK* SRWLock)
	{
		OOBase::Win32::rwmutex_t*& m = *reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock);
		m = OOBase::CrtAllocator::allocate_new<OOBase::Win32::rwmutex_t>();
		if (!m)
			OOBase_CallCriticalFailure(OOBase::system_error());
	}

	void Win32Thunk::impl_AcquireSRWLockShared(SRWLOCK* SRWLock)
	{
		(*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->acquire_read();
	}

	void Win32Thunk::impl_AcquireSRWLockExclusive(SRWLOCK* SRWLock)
	{
		(*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->acquire();
	}

	BOOL Win32Thunk::impl_TryAcquireSRWLockShared(SRWLOCK* SRWLock)
	{
		return (*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->try_acquire_read();
	}

	BOOL Win32Thunk::impl_TryAcquireSRWLockExclusive(SRWLOCK* SRWLock)
	{
		return (*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->try_acquire();
	}

	void Win32Thunk::impl_ReleaseSRWLockShared(SRWLOCK* SRWLock)
	{
		return (*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->release_read();
	}

	void Win32Thunk::impl_ReleaseSRWLockExclusive(SRWLOCK* SRWLock)
	{
		(*reinterpret_cast<OOBase::Win32::rwmutex_t**>(SRWLock))->release();
	}

	void Win32Thunk::impl_InitializeConditionVariable(CONDITION_VARIABLE* ConditionVariable)
	{
		OOBase::Win32::condition_variable_t*& c = *reinterpret_cast<OOBase::Win32::condition_variable_t**>(ConditionVariable);
		c = OOBase::CrtAllocator::allocate_new<OOBase::Win32::condition_variable_t>();
		if (!c)
			OOBase_CallCriticalFailure(OOBase::system_error());
	}

	void Win32Thunk::impl_WakeConditionVariable(CONDITION_VARIABLE* ConditionVariable)
	{
		(*reinterpret_cast<OOBase::Win32::condition_variable_t**>(ConditionVariable))->signal();
	}

	void Win32Thunk::impl_WakeAllConditionVariable(CONDITION_VARIABLE* ConditionVariable)
	{
		(*reinterpret_cast<OOBase::Win32::condition_variable_t**>(ConditionVariable))->broadcast();
	}
}

BOOL OOBase::Win32::InitOnceExecuteOnce(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context)
{
	return (*Win32Thunk::instance().m_InitOnceExecuteOnce)(InitOnce,InitFn,Parameter,Context);
}

void OOBase::Win32::InitializeSRWLock(SRWLOCK* SRWLock)
{
#if (_WIN32_WINNT >= 0x0600)
	static_assert(sizeof(rwmutex_t*) >= sizeof(SRWLOCK),"Refer to maintainters");
#endif

	(*Win32Thunk::instance().m_InitializeSRWLock)(SRWLock);
}

void OOBase::Win32::AcquireSRWLockShared(SRWLOCK* SRWLock)
{
	(*Win32Thunk::instance().m_AcquireSRWLockShared)(SRWLock);
}

void OOBase::Win32::AcquireSRWLockExclusive(SRWLOCK* SRWLock)
{
	(*Win32Thunk::instance().m_AcquireSRWLockExclusive)(SRWLock);
}

BOOL OOBase::Win32::TryAcquireSRWLockShared(SRWLOCK* SRWLock)
{
	return (*Win32Thunk::instance().m_TryAcquireSRWLockShared)(SRWLock);
}

BOOL OOBase::Win32::TryAcquireSRWLockExclusive(SRWLOCK* SRWLock)
{
	return (*Win32Thunk::instance().m_TryAcquireSRWLockExclusive)(SRWLock);
}

void OOBase::Win32::ReleaseSRWLockShared(SRWLOCK* SRWLock)
{
	(*Win32Thunk::instance().m_ReleaseSRWLockShared)(SRWLock);
}

void OOBase::Win32::ReleaseSRWLockExclusive(SRWLOCK* SRWLock)
{
	(*Win32Thunk::instance().m_ReleaseSRWLockExclusive)(SRWLock);
}

void OOBase::Win32::DeleteSRWLock(SRWLOCK* SRWLock)
{
	if (Win32Thunk::instance().m_InitializeSRWLock == Win32Thunk::impl_InitializeSRWLock)
		OOBase::CrtAllocator::delete_free(*reinterpret_cast<rwmutex_t**>(SRWLock));
}

OOBase::Win32::rwmutex_t::rwmutex_t() :
		m_nReaders(-1),
		m_hReaderEvent(NULL),
		m_hEvent(NULL),
		m_hWriterMutex(NULL)
{
	m_hReaderEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
	if (!m_hReaderEvent)
		OOBase_CallCriticalFailure(GetLastError());

	m_hEvent = CreateEventW(NULL,FALSE,TRUE,NULL);
	if (!m_hEvent)
		OOBase_CallCriticalFailure(GetLastError());

	m_hWriterMutex = CreateMutexW(NULL,FALSE,NULL);
	if (!m_hWriterMutex)
		OOBase_CallCriticalFailure(GetLastError());
}

OOBase::Win32::rwmutex_t::~rwmutex_t()
{
}

BOOL OOBase::Win32::rwmutex_t::try_acquire()
{
	HANDLE handles[2];
	handles[0] = m_hWriterMutex;
	handles[1] = m_hEvent;

	DWORD dwWait = WaitForMultipleObjects(2,handles,TRUE,0);
	if (dwWait == WAIT_TIMEOUT)
		return false;

	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	/*DWORD dwWait = WaitForSingleObject(m_hWriterMutex, 0);
	if (dwWait == WAIT_TIMEOUT)
		return false;

	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	if (WaitForSingleObject(m_hEvent, INFINITE) != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());*/

	return true;
}

void OOBase::Win32::rwmutex_t::acquire()
{
	HANDLE handles[2];
	handles[0] = m_hWriterMutex;
	handles[1] = m_hEvent;

	if (WaitForMultipleObjects(2,handles,TRUE,INFINITE) != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	/*if (WaitForSingleObject(m_hWriterMutex, INFINITE) != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	if (WaitForSingleObject(m_hEvent, INFINITE) != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());*/
}

void OOBase::Win32::rwmutex_t::release()
{
	if (!SetEvent(m_hEvent))
		OOBase_CallCriticalFailure(GetLastError());

	if (!ReleaseMutex(m_hWriterMutex))
		OOBase_CallCriticalFailure(GetLastError());
}

BOOL OOBase::Win32::rwmutex_t::try_acquire_read()
{
	if (InterlockedIncrement(&m_nReaders) == 0)
	{
		DWORD dwWait = WaitForSingleObject(m_hEvent, 0);
		if (dwWait == WAIT_TIMEOUT)
		{
			InterlockedDecrement(&m_nReaders);
			return false;
		}

		if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());

		if (!SetEvent(m_hReaderEvent))
			OOBase_CallCriticalFailure(GetLastError());
	}
	else
	{
		DWORD dwWait = WaitForSingleObject(m_hReaderEvent, 0);
		if (dwWait == WAIT_TIMEOUT)
		{
			InterlockedDecrement(&m_nReaders);
			return false;
		}

		if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());
	}

	return true;
}

void OOBase::Win32::rwmutex_t::acquire_read()
{
	if (InterlockedIncrement(&m_nReaders) == 0)
	{
		if (WaitForSingleObject(m_hEvent, INFINITE) != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());

		if (!SetEvent(m_hReaderEvent))
			OOBase_CallCriticalFailure(GetLastError());
	}
	else if (WaitForSingleObject(m_hReaderEvent, INFINITE) != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());
}

void OOBase::Win32::rwmutex_t::release_read()
{
	if (InterlockedDecrement(&m_nReaders) < 0)
	{
		if (!ResetEvent(m_hReaderEvent))
			OOBase_CallCriticalFailure(GetLastError());

		if (!SetEvent(m_hEvent))
			OOBase_CallCriticalFailure(GetLastError());
	}
}

void OOBase::Win32::InitializeConditionVariable(CONDITION_VARIABLE* ConditionVariable)
{
#if (_WIN32_WINNT >= 0x0600)
	static_assert(sizeof(condition_variable_t*) >= sizeof(CONDITION_VARIABLE),"Refer to maintainters");
#endif

	(*Win32Thunk::instance().m_InitializeConditionVariable)(ConditionVariable);
}

BOOL OOBase::Win32::SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds)
{
	if (!Win32Thunk::instance().m_SleepConditionVariableCS)
		return (*reinterpret_cast<condition_variable_t**>(ConditionVariable))->wait(Mutex->u.m_mutex,dwMilliseconds) ? TRUE : FALSE;
	else
		return (*Win32Thunk::instance().m_SleepConditionVariableCS)(ConditionVariable,&Mutex->u.m_cs,dwMilliseconds);
}

void OOBase::Win32::WakeConditionVariable(CONDITION_VARIABLE* ConditionVariable)
{
	(*Win32Thunk::instance().m_WakeConditionVariable)(ConditionVariable);
}

void OOBase::Win32::WakeAllConditionVariable(CONDITION_VARIABLE* ConditionVariable)
{
	(*Win32Thunk::instance().m_WakeAllConditionVariable)(ConditionVariable);
}

void OOBase::Win32::DeleteConditionVariable(CONDITION_VARIABLE* ConditionVariable)
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
		OOBase::CrtAllocator::delete_free(*reinterpret_cast<condition_variable_t**>(ConditionVariable));
}

BOOL OOBase::Win32::BindIoCompletionCallback(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG Flags)
{
	return (*Win32Thunk::instance().m_BindIoCompletionCallback)(FileHandle,Function,Flags);
}

OOBase::Win32::condition_mutex_t::condition_mutex_t()
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
	{
		u.m_mutex = CreateMutexW(NULL,FALSE,NULL);
		if (!u.m_mutex)
			OOBase_CallCriticalFailure(GetLastError());
	}
	else
	{
		InitializeCriticalSection(&u.m_cs);
	}
}

OOBase::Win32::condition_mutex_t::~condition_mutex_t()
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
		CloseHandle(u.m_mutex);
	else
		DeleteCriticalSection(&u.m_cs);
}

bool OOBase::Win32::condition_mutex_t::try_acquire()
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
	{
		DWORD dwWait = WaitForSingleObject(u.m_mutex,0);
		if (dwWait == WAIT_OBJECT_0)
			return true;
		else if (dwWait != WAIT_TIMEOUT)
			OOBase_CallCriticalFailure(GetLastError());

		return false;
	}
	else
		return (TryEnterCriticalSection(&u.m_cs) ? true : false);
}

void OOBase::Win32::condition_mutex_t::acquire()
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
	{
		DWORD dwWait = WaitForSingleObject(u.m_mutex,INFINITE);
		if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());
	}
	else
		EnterCriticalSection(&u.m_cs);
}

bool OOBase::Win32::condition_mutex_t::acquire(const Timeout& timeout)
{
	if (timeout.is_infinite())
	{
		acquire();
		return true;
	}

	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
	{
		DWORD dwWait = WaitForSingleObject(u.m_mutex,timeout.millisecs());
		if (dwWait == WAIT_TIMEOUT)
			return false;
		else if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());
	}
	else 
	{
		while (!timeout.has_expired())
		{
			if (TryEnterCriticalSection(&u.m_cs))
				return true;

			::Sleep(0);
		}
	}
	return false;
}

void OOBase::Win32::condition_mutex_t::release()
{
	if (Win32Thunk::instance().m_InitializeConditionVariable == Win32Thunk::impl_InitializeConditionVariable)
	{
		if (!ReleaseMutex(u.m_mutex))
			OOBase_CallCriticalFailure(GetLastError());
	}
	else
		LeaveCriticalSection(&u.m_cs);
}

OOBase::Win32::condition_variable_t::condition_variable_t() :
		m_waiters(0),
		m_broadcast(false),
		m_sema(NULL),
		m_waiters_done(NULL)
{
	if (!InitializeCriticalSectionAndSpinCount(&m_waiters_lock,4001))
		OOBase_CallCriticalFailure(GetLastError());

	m_sema = CreateSemaphoreW(NULL,       // no security
							  0,          // initially 0
							  0x7fffffff, // max count
							  NULL);      // unnamed
	if (!m_sema)
	{
		DWORD dwErr = GetLastError();
		DeleteCriticalSection(&m_waiters_lock);
		OOBase_CallCriticalFailure(dwErr);
	}

	m_waiters_done = CreateEventW(NULL,  // no security
								  FALSE, // auto-reset
								  FALSE, // non-signaled initially
								  NULL); // unnamed
	if (!m_waiters_done)
	{
		DWORD dwErr = GetLastError();
		DeleteCriticalSection(&m_waiters_lock);
		OOBase_CallCriticalFailure(dwErr);
	}
}

OOBase::Win32::condition_variable_t::~condition_variable_t()
{
	DeleteCriticalSection(&m_waiters_lock);
}

bool OOBase::Win32::condition_variable_t::wait(HANDLE hMutex, DWORD dwMilliseconds)
{
	// Use a timeout as we do multiple waits
	Timeout timeout(dwMilliseconds / 1000,(dwMilliseconds % 1000) * 1000);

	// Avoid race conditions.
	EnterCriticalSection(&m_waiters_lock);
	if (m_broadcast)
	{
		// If there is a broadcast in progress, spin here...
		LeaveCriticalSection(&m_waiters_lock);
		for (;;)
		{
			EnterCriticalSection(&m_waiters_lock);
			if (!m_broadcast)
				break;

			LeaveCriticalSection(&m_waiters_lock);

			if (timeout.has_expired())
				return false;
		}
	}
		
	bool ret = false;
	DWORD dwWait;
	if (!timeout.has_expired())
	{
		++m_waiters;

		LeaveCriticalSection(&m_waiters_lock);

		// This call atomically releases the mutex and waits on the
		// semaphore until <signal> or <broadcast>
		// are called by another thread.
		dwWait = SignalObjectAndWait(hMutex,m_sema,timeout.millisecs(),FALSE);
		if (dwWait != WAIT_OBJECT_0 && dwWait != WAIT_TIMEOUT)
			OOBase_CallCriticalFailure(GetLastError());

		ret = (dwWait == WAIT_OBJECT_0);

		// Reacquire lock to avoid race conditions.
		EnterCriticalSection(&m_waiters_lock);

		// We're no longer waiting...
		--m_waiters;
	}

	// Check to see if we're the last waiter after <broadcast>.
	bool last_waiter = m_broadcast && (m_waiters == 0);

	LeaveCriticalSection(&m_waiters_lock);

	// If we're the last waiter thread during this particular broadcast
	// then let all the other threads proceed.
	if (last_waiter)
	{
		// This call atomically signals the <m_waiters_done> event and waits until
		// it can acquire the <hMutex>.  This is required to ensure fairness.
		dwWait = SignalObjectAndWait(m_waiters_done,hMutex,INFINITE,FALSE);
	}
	else
	{
		// Always regain the external mutex since that's the guarantee we
		// give to our callers.
		dwWait = WaitForSingleObject(hMutex,INFINITE);
	}

	// Return the correct code
	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	if (last_waiter)
	{
		if (!SetEvent(m_waiters_done))
			OOBase_CallCriticalFailure(GetLastError());
	}

	return ret;
}

void OOBase::Win32::condition_variable_t::signal()
{
	EnterCriticalSection(&m_waiters_lock);
	bool have_waiters = m_waiters > 0;
	LeaveCriticalSection(&m_waiters_lock);

	// If there aren't any waiters, then this is a no-op.
	if (have_waiters && !ReleaseSemaphore(m_sema,1,NULL))
		OOBase_CallCriticalFailure(GetLastError());
}

void OOBase::Win32::condition_variable_t::broadcast()
{
	// This is needed to ensure that <m_waiters> and <m_broadcast> are
	// consistent relative to each other.
	EnterCriticalSection(&m_waiters_lock);
	bool have_waiters = false;

	if (m_waiters > 0)
	{
		// We are broadcasting, even if there is just one waiter...
		// Record that we are broadcasting, which helps optimize
		// <wait> for the non-broadcast case.
		m_broadcast = true;
		have_waiters = true;
	}

	if (have_waiters)
	{
		// Wake up all the waiters atomically.
		LONG lPrev = 0;
		if (!ReleaseSemaphore(m_sema,m_waiters,&lPrev))
		{
			DWORD dwErr = GetLastError();
			LeaveCriticalSection(&m_waiters_lock);
			OOBase_CallCriticalFailure(dwErr);
		}

		LeaveCriticalSection(&m_waiters_lock);

		// Wait for all the awakened threads to acquire the counting semaphore.
		DWORD dwWait = WaitForSingleObject(m_waiters_done,INFINITE);

		// This assignment is okay, even without the <m_waiters_lock> held
		// because no other waiter threads can wake up to access it.
		m_broadcast = false;

		if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());
	}
	else
		LeaveCriticalSection(&m_waiters_lock);
}

int OOBase::Win32::detail::wchar_t_to_utf8(const wchar_t* wsz, char* sz, int& len)
{
	int len2 = WideCharToMultiByte(CP_UTF8,0,wsz,-1,sz,len,NULL,NULL);
	if (len2 <= 0)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			return GetLastError();
	}
	else if (len2 <= len)
		return 0;

	len = WideCharToMultiByte(CP_UTF8,0,wsz,-1,NULL,0,NULL,NULL);
	return GetLastError();
}

int OOBase::Win32::detail::utf8_to_wchar_t(const char* sz, wchar_t* wsz, int& len)
{
	int len2 = MultiByteToWideChar(CP_UTF8,0,sz,-1,wsz,len);
	if (len2 <= 0)
	{
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			return GetLastError();
	}
	else if (len2 <= len)
		return 0;
	
	len = MultiByteToWideChar(CP_UTF8,0,sz,-1,NULL,0);
	return GetLastError();
}

int OOBase::random_bytes(void* buffer, size_t len)
{
	if (!Win32Thunk::instance().m_RtlGenRandom)
		return ERROR_INVALID_FUNCTION;

	if (!Win32Thunk::instance().m_RtlGenRandom(buffer,(ULONG)len))
		return GetLastError();

	return 0;
}

#if 0

namespace
{
	union Marker
	{
		struct
		{
			size_t  len;
			Marker* next;
			Marker* prev;
		} value;
		char   padding[32];
	};

	struct DebugHeapInfo
	{
		Marker*          free_head;
		Marker*          free_tail;
		size_t           page_size;
		CRITICAL_SECTION lock;
	} s_heap_info;

	BOOL WINAPI init_debug_heap_i(INIT_ONCE*, void*, void**)
	{
		InitializeCriticalSection(&s_heap_info.lock);

		SYSTEM_INFO si = {0};
		GetSystemInfo(&si);
		s_heap_info.page_size = si.dwPageSize - 1;

		s_heap_info.free_head = NULL;
		s_heap_info.free_tail = NULL;

		return TRUE;
	}

	void init_debug_heap()
	{
		static INIT_ONCE key = {0};
		if (!Win32Thunk::impl_InitOnceExecuteOnce(&key,&init_debug_heap_i,NULL,NULL))
			OOBase_CallCriticalFailure(GetLastError());
	}
}

void* OOBase::CrtAllocator::allocate(size_t len, size_t /*align*/)
{
	if (!len)
		return NULL;

	init_debug_heap();

	len = (len + sizeof(Marker) + s_heap_info.page_size) & (~s_heap_info.page_size);

	Marker* m = (Marker*)::VirtualAlloc(NULL,len,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
	if (m)
		m->value.len = len;
	else
	{
		EnterCriticalSection(&s_heap_info.lock);

		for (m = s_heap_info.free_head; m != NULL; )
		{
			DWORD dwOld = 0;
			::VirtualProtect(m,sizeof(Marker),PAGE_READWRITE,&dwOld);

			if (m->value.len >= len)
			{
				::VirtualProtect(m,m->value.len,PAGE_READWRITE,&dwOld);
			
				if (m->value.prev)
				{
					DWORD dwPrev = 0;
					::VirtualProtect(m->value.prev,sizeof(Marker),PAGE_READWRITE,&dwPrev);
					m->value.prev->value.next = m->value.next;
					::VirtualProtect(m->value.prev,sizeof(Marker),dwPrev,&dwPrev);
				}
				else
					s_heap_info.free_head = m->value.next;

				if (m->value.next)
				{
					DWORD dwPrev = 0;
					::VirtualProtect(m->value.next,sizeof(Marker),PAGE_READWRITE,&dwPrev);
					m->value.next->value.prev = m->value.prev;
					::VirtualProtect(m->value.next,sizeof(Marker),dwPrev,&dwPrev);
				}
				else
					s_heap_info.free_tail = m->value.prev;

				break;
			}

			Marker* next = m->value.next;
			
			::VirtualProtect(m,sizeof(Marker),dwOld,&dwOld);
	
			m = next;
		}

		LeaveCriticalSection(&s_heap_info.lock);
	}

	if (!m)
		return NULL;

	m->value.next = reinterpret_cast<Marker*>(0x12345678);
	m->value.prev = reinterpret_cast<Marker*>(0x87654321);
	memset(m+1,0xFF,m->value.len-sizeof(Marker));

	return (m+1);
}

void* OOBase::CrtAllocator::reallocate(void* ptr, size_t len, size_t /*align*/)
{
	if (!len)
		return NULL;

	if (!ptr)
		return allocate(len);

	Marker* m = (static_cast<Marker*>(ptr) - 1);
	if (m->value.next != reinterpret_cast<void*>(0x12345678) || m->value.prev != reinterpret_cast<void*>(0x87654321))
		DebugBreak();

	if (m->value.len - sizeof(Marker) >= len)
		return ptr;
	
	void* n = allocate(len);
	if (n)
	{
		memcpy(n,ptr,m->value.len);
		free(ptr);
	}

	return n;
}

void OOBase::CrtAllocator::free(void* ptr)
{
	if (ptr)
	{
		init_debug_heap();

		Marker* m = (static_cast<Marker*>(ptr) - 1);
		if (m->value.next != reinterpret_cast<void*>(0x12345678) || m->value.prev != reinterpret_cast<void*>(0x87654321))
			DebugBreak();

		EnterCriticalSection(&s_heap_info.lock);

		if (s_heap_info.free_tail)
		{
			m->value.prev = s_heap_info.free_tail;
			m->value.next = NULL;

			DWORD dwPrev = 0;
			::VirtualProtect(s_heap_info.free_tail,sizeof(Marker),PAGE_READWRITE,&dwPrev);
			s_heap_info.free_tail->value.next = m;
			::VirtualProtect(s_heap_info.free_tail,sizeof(Marker),dwPrev,&dwPrev);
		}
		else
		{
			m->value.next = NULL;
			m->value.prev = NULL;
			s_heap_info.free_head = m;
		}

		s_heap_info.free_tail = m;

		DWORD dwOld = 0;
		::VirtualProtect(m,m->value.len,PAGE_NOACCESS,&dwOld);

		LeaveCriticalSection(&s_heap_info.lock);
	}
}

#else

void* OOBase::CrtAllocator::allocate(size_t len, size_t /*align*/)
{
	if (!len)
		return NULL;
	else
		return ::HeapAlloc(Win32Thunk::instance().m_hHeap,0,len);
}

void* OOBase::CrtAllocator::reallocate(void* ptr, size_t len, size_t /*align*/)
{
	if (!len)
		return NULL;
	else if (!ptr)
		return ::HeapAlloc(Win32Thunk::instance().m_hHeap,0,len);
	else
		return ::HeapReAlloc(Win32Thunk::instance().m_hHeap,0,ptr,len);
}

void OOBase::CrtAllocator::free(void* ptr)
{
	if (ptr)
		::HeapFree(Win32Thunk::instance().m_hHeap,0,ptr);
}

#endif

#endif // _WIN32
