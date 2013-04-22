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

#include "../include/OOBase/Mutex.h"
#include "Win32Impl.h"

#if defined(_WIN32)

OOBase::SpinLock::SpinLock()
{
	if (!InitializeCriticalSection(&m_cs))
		OOBase_CallCriticalFailure(GetLastError());
}

OOBase::SpinLock::~SpinLock()
{
	DeleteCriticalSection(&m_cs);
}

void OOBase::SpinLock::acquire()
{
	EnterCriticalSection(&m_cs);

	assert(m_cs.RecursionCount == 1);
}

bool OOBase::SpinLock::try_acquire()
{
	if (!TryEnterCriticalSection(&m_cs))
		return false;

	assert(m_cs.RecursionCount == 1);

	return true;
}

void OOBase::SpinLock::release()
{
	LeaveCriticalSection(&m_cs);
}

OOBase::Mutex::Mutex() :
		m_mutex(NULL)
{
	m_mutex = CreateMutexW(NULL,FALSE,NULL);
	if (!m_mutex)
		OOBase_CallCriticalFailure(GetLastError());
}

OOBase::Mutex::~Mutex()
{
}

bool OOBase::Mutex::try_acquire()
{
	DWORD dwWait = WaitForSingleObject(m_mutex,0);
	if (dwWait == WAIT_OBJECT_0)
		return true;
	else if (dwWait != WAIT_TIMEOUT)
		OOBase_CallCriticalFailure(GetLastError());

	return false;
}

void OOBase::Mutex::acquire()
{
	DWORD dwWait = WaitForSingleObject(m_mutex,INFINITE);
	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());
}

bool OOBase::Mutex::acquire(const Timeout& timeout)
{
	DWORD dwWait = WaitForSingleObject(m_mutex,timeout.millisecs());
	if (dwWait == WAIT_TIMEOUT)
		return false;

	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	return true;
}

void OOBase::Mutex::release()
{
	if (!ReleaseMutex(m_mutex))
		OOBase_CallCriticalFailure(GetLastError());
}

OOBase::RWMutex::RWMutex()
{
	Win32::InitializeSRWLock(&m_lock);
}

OOBase::RWMutex::~RWMutex()
{
	Win32::DeleteSRWLock(&m_lock);
}

bool OOBase::RWMutex::try_acquire()
{
	return (Win32::TryAcquireSRWLockExclusive(&m_lock) == TRUE);
}

void OOBase::RWMutex::acquire()
{
	Win32::AcquireSRWLockExclusive(&m_lock);
}

void OOBase::RWMutex::release()
{
	Win32::ReleaseSRWLockExclusive(&m_lock);
}

bool OOBase::RWMutex::try_acquire_read()
{
	return (Win32::TryAcquireSRWLockShared(&m_lock) == TRUE);
}

void OOBase::RWMutex::acquire_read()
{
	Win32::AcquireSRWLockShared(&m_lock);
}

void OOBase::RWMutex::release_read()
{
	Win32::ReleaseSRWLockShared(&m_lock);
}

#elif defined(HAVE_PTHREAD)

#if defined(_POSIX_SPIN_LOCKS)

OOBase::SpinLock::SpinLock()
{
	int err = pthread_spin_init(&m_sl,PTHREAD_PROCESS_PRIVATE);
	if (err)
		OOBase_CallCriticalFailure(err);
}

OOBase::SpinLock::~SpinLock()
{
	int err = pthread_spin_destroy(&m_sl);
	if (err)
		OOBase_CallCriticalFailure(err);
}

bool OOBase::SpinLock::try_acquire()
{
	int err = pthread_spin_trylock(&m_sl);
	if (!err)
		return true;

	if (err != EBUSY)
		OOBase_CallCriticalFailure(err);

	return false;
}

void OOBase::SpinLock::acquire()
{
	int err = pthread_spin_lock(&m_sl);
	if (err)
		OOBase_CallCriticalFailure(err);
}

void OOBase::SpinLock::release()
{
	int err = pthread_spin_unlock(&m_sl);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

#endif // _POSIX_SPIN_LOCKS

OOBase::Mutex::Mutex()
{
	init(true);
}

OOBase::Mutex::Mutex(bool as_spin_lock)
{
	init(false);
}

void OOBase::Mutex::init(bool bRecursive)
{
	pthread_mutexattr_t attr;
	int err = pthread_mutexattr_init(&attr);
	if (!err)
	{
#if !defined(NDEBUG) && defined(PTHREAD_MUTEX_ERRORCHECK)
		err = pthread_mutexattr_settype(&attr,bRecursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_ERRORCHECK);
#else
		err = pthread_mutexattr_settype(&attr,bRecursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
#endif
		if (!err)
		{
			err = pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_PRIVATE);
			if (!err)
				err = pthread_mutex_init(&m_mutex,&attr);
		}

		pthread_mutexattr_destroy(&attr);
	}

	if (err)
		OOBase_CallCriticalFailure(err);
}

OOBase::Mutex::~Mutex()
{
	int err = pthread_mutex_destroy(&m_mutex);
	if (err)
		OOBase_CallCriticalFailure(err);
}

bool OOBase::Mutex::try_acquire()
{
	int err = pthread_mutex_trylock(&m_mutex);
	if (!err)
		return true;

	if (err != EBUSY)
		OOBase_CallCriticalFailure(err);

	return false;
}

void OOBase::Mutex::acquire()
{
	int err = pthread_mutex_lock(&m_mutex);
	if (err)
		OOBase_CallCriticalFailure(err);
}

void OOBase::Mutex::release()
{
	int err = pthread_mutex_unlock(&m_mutex);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

OOBase::RWMutex::RWMutex()
{
	pthread_rwlockattr_t attr;
	int err = pthread_rwlockattr_init(&attr);
	if (!err)
	{
		err = pthread_rwlockattr_setpshared(&attr,PTHREAD_PROCESS_PRIVATE);
		if (!err)
			err = pthread_rwlock_init(&m_mutex,&attr);

		pthread_rwlockattr_destroy(&attr);
	}

	if (err)
		OOBase_CallCriticalFailure(err);
}

OOBase::RWMutex::~RWMutex()
{
	pthread_rwlock_destroy(&m_mutex);
}

bool OOBase::RWMutex::try_acquire()
{
	int err = pthread_rwlock_trywrlock(&m_mutex);
	if (!err)
		return true;

	if (err != EBUSY)
		OOBase_CallCriticalFailure(err);

	return false;
}

void OOBase::RWMutex::acquire()
{
	int err = pthread_rwlock_wrlock(&m_mutex);
	if (err)
		OOBase_CallCriticalFailure(err);
}

void OOBase::RWMutex::release()
{
	int err = pthread_rwlock_unlock(&m_mutex);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

bool OOBase::RWMutex::try_acquire_read()
{
	int err = pthread_rwlock_tryrdlock(&m_mutex);
	if (!err)
		return true;

	if (err != EBUSY)
		OOBase_CallCriticalFailure(err);

	return false;
}

void OOBase::RWMutex::acquire_read()
{
	int err = pthread_rwlock_rdlock(&m_mutex);
	if (err)
		OOBase_CallCriticalFailure(err);
}

void OOBase::RWMutex::release_read()
{
	release();
}

#endif
