///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#ifndef OOBASE_WIN32_INTERNAL_H_INCLUDED_
#define OOBASE_WIN32_INTERNAL_H_INCLUDED_

#include "../Win32.h"
#include "../Allocator.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		OOBase::string FormatMessage(DWORD dwErr = GetLastError());

		class rwmutex_t
		{
		public:
			rwmutex_t();
			~rwmutex_t();

			void acquire();
			void release();
			void acquire_read();
			void release_read();

		private:
			LONG                       m_nReaders;
			OOBase::Win32::SmartHandle m_hReaderEvent;
			OOBase::Win32::SmartHandle m_hEvent;
			OOBase::Win32::SmartHandle m_hWriterMutex;
		};

		class condition_variable_t
		{
		public:
			condition_variable_t();
			~condition_variable_t();

			bool wait(HANDLE hMutex, DWORD dwMilliseconds);
			void signal();
			void broadcast();

		private:
			CRITICAL_SECTION m_waiters_lock;
			unsigned long    m_waiters;
			bool             m_broadcast;
			SmartHandle      m_sema;
			SmartHandle      m_waiters_done;
		};

		class condition_mutex_t;
	}
}

// Declare types and function not declared prior to Vista
#if (WINVER < 0x0600)

typedef LONG INIT_ONCE;
typedef BOOL (__stdcall *PINIT_ONCE_FN) (INIT_ONCE* InitOnce, void* Parameter, void** Context);

typedef OOBase::Win32::rwmutex_t* SRWLOCK;
typedef OOBase::Win32::condition_variable_t* CONDITION_VARIABLE;

#endif

namespace OOBase
{
	namespace Win32
	{
		BOOL InitOnceExecuteOnce(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context);
		
		void InitializeSRWLock(SRWLOCK* SRWLock);
		void AcquireSRWLockShared(SRWLOCK* SRWLock);
		void AcquireSRWLockExclusive(SRWLOCK* SRWLock);
		void ReleaseSRWLockShared(SRWLOCK* SRWLock);
		void ReleaseSRWLockExclusive(SRWLOCK* SRWLock);
		void DeleteSRWLock(SRWLOCK* SRWLock);

		void InitializeConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		BOOL SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds);
		void WakeConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		void WakeAllConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		void DeleteConditionVariable(CONDITION_VARIABLE* ConditionVariable);

		BOOL BindIoCompletionCallback(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG Flags);

		class condition_mutex_t
		{
			// We need private access in this function
			friend BOOL SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds);

		public:
			condition_mutex_t();
			~condition_mutex_t();

			bool tryacquire();
			void acquire();
			void release();

		private:
			union U
			{
				CRITICAL_SECTION m_cs;
				HANDLE           m_mutex;
			} u;
		};
	}
}

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_INTERNAL_H_INCLUDED_
