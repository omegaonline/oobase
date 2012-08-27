///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#ifndef OOBASE_WIN32_IMPL_H_INCLUDED_
#define OOBASE_WIN32_IMPL_H_INCLUDED_

#if defined(_WIN32)

#include "../include/OOBase/Once.h"

#if (_WIN32_WINNT < 0x0600)
typedef BOOL (__stdcall *PINIT_ONCE_FN) (INIT_ONCE* InitOnce, void* Parameter, void** Context);
#endif

namespace OOBase
{
	namespace Win32
	{
		BOOL InitOnceExecuteOnce(INIT_ONCE* InitOnce, PINIT_ONCE_FN InitFn, void* Parameter, void** Context);
		
		void InitializeSRWLock(SRWLOCK* SRWLock);
		void AcquireSRWLockShared(SRWLOCK* SRWLock);
		void AcquireSRWLockExclusive(SRWLOCK* SRWLock);
		BOOL TryAcquireSRWLockShared(SRWLOCK* SRWLock);
		BOOL TryAcquireSRWLockExclusive(SRWLOCK* SRWLock);
		void ReleaseSRWLockShared(SRWLOCK* SRWLock);
		void ReleaseSRWLockExclusive(SRWLOCK* SRWLock);
		void DeleteSRWLock(SRWLOCK* SRWLock);

		void InitializeConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		BOOL SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds);
		void WakeConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		void WakeAllConditionVariable(CONDITION_VARIABLE* ConditionVariable);
		void DeleteConditionVariable(CONDITION_VARIABLE* ConditionVariable);

		BOOL BindIoCompletionCallback(HANDLE FileHandle, LPOVERLAPPED_COMPLETION_ROUTINE Function, ULONG Flags);
	}
}

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_IMPL_H_INCLUDED_
