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

#include "../include/OOBase/Condition.h"
#include "Win32Impl.h"

#if defined(_WIN32)

OOBase::Condition::Condition()
{
	Win32::InitializeConditionVariable(&m_var);
}

OOBase::Condition::~Condition()
{
	Win32::DeleteConditionVariable(&m_var);
}

bool OOBase::Condition::wait(Condition::Mutex& mutex, const Timeout& timeout)
{
	if (!Win32::SleepConditionVariable(&m_var,&mutex,timeout.millisecs()))
	{
		DWORD dwErr = GetLastError();
		if (dwErr == ERROR_TIMEOUT)
			return false;

		OOBase_CallCriticalFailure(dwErr);
	}

	return true;
}

void OOBase::Condition::signal()
{
	Win32::WakeConditionVariable(&m_var);
}

void OOBase::Condition::broadcast()
{
	Win32::WakeAllConditionVariable(&m_var);
}

#elif defined(HAVE_PTHREAD)

OOBase::Condition::Condition()
{
	pthread_condattr_t attr;
	int err = pthread_condattr_init(&attr);
	if (!err)
	{
		err = pthread_condattr_setclock(&attr,CLOCK_MONOTONIC);
		if (!err)
			err = pthread_condattr_setpshared(&attr,PTHREAD_PROCESS_PRIVATE);
		if (!err)
			err = pthread_cond_init(&m_var,&attr);

		pthread_condattr_destroy(&attr);
	}

	if (err)
		OOBase_CallCriticalFailure(err);
}

OOBase::Condition::~Condition()
{
	pthread_cond_destroy(&m_var);
}

bool OOBase::Condition::wait(Condition::Mutex& mutex, const Timeout& timeout)
{
	int err = 0;
	if (timeout.is_infinite())
		err = pthread_cond_wait(&m_var,&mutex.m_mutex);
	else
	{
		do
		{
			::timespec wt = {0};
			timeout.get_abs_timespec(wt);
			err = pthread_cond_timedwait(&m_var,&mutex.m_mutex,&wt);
		}
		while (err == ETIMEDOUT && !timeout.has_expired());
	}
	
	if (err == 0)
		return true;

	if (err != ETIMEDOUT)
		OOBase_CallCriticalFailure(err);

	return false;
}

void OOBase::Condition::signal()
{
	pthread_cond_signal(&m_var);
}

void OOBase::Condition::broadcast()
{
	pthread_cond_broadcast(&m_var);
}

#else

#error Fix me!

#endif

#if defined(_WIN32)

OOBase::Event::Event(bool bSet, bool bAutoReset) :
		m_handle(::CreateEvent(NULL,bAutoReset ? FALSE : TRUE,bSet ? TRUE : FALSE,NULL))
{
	if (!m_handle.is_valid())
		OOBase_CallCriticalFailure(GetLastError());
}

OOBase::Event::~Event()
{
}

bool OOBase::Event::is_set()
{
	return (WaitForSingleObject(m_handle,0) == WAIT_OBJECT_0);
}

void OOBase::Event::set()
{
	if (!::SetEvent(m_handle))
		OOBase_CallCriticalFailure(GetLastError());
}

bool OOBase::Event::wait(const Timeout& timeout)
{
	DWORD dwWait = WaitForSingleObject(m_handle,timeout.millisecs());
	if (dwWait == WAIT_OBJECT_0)
		return true;
	else if (dwWait != WAIT_TIMEOUT)
		OOBase_CallCriticalFailure(GetLastError());

	return false;
}

void OOBase::Event::reset()
{
	if (!::ResetEvent(m_handle))
		OOBase_CallCriticalFailure(GetLastError());
}

#else

OOBase::Event::Event(bool bSet, bool bAutoReset) :
		m_bAuto(bAutoReset), m_bSet(bSet)
{
}

OOBase::Event::~Event()
{
}

bool OOBase::Event::is_set()
{
	Guard<Condition::Mutex> guard(m_lock);

	bool bSet = m_bSet;

	// Reset if we are an auto event
	if (bSet && m_bAuto)
		m_bSet = false;

	return bSet;
}

void OOBase::Event::set()
{
	Guard<Condition::Mutex> guard(m_lock);

	// Only set if we are not set
	if (!m_bSet)
	{
		m_bSet = true;

		// If we are an auto event, signal 1 thread only
		if (m_bAuto)
			m_condition.signal();
		else
			m_condition.broadcast();
	}
}

bool OOBase::Event::wait(const Timeout& timeout)
{
	Guard<Condition::Mutex> guard(m_lock);

	while (!m_bSet)
	{
		if (!m_condition.wait(m_lock,timeout))
			return false;
	}

	// Reset if we are an auto event
	if (m_bAuto)
		m_bSet = false;

	return true;
}

void OOBase::Event::reset()
{
	Guard<Condition::Mutex> guard(m_lock);

	m_bSet = false;
}

#endif
