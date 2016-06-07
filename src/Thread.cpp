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

#include "../include/OOBase/TLSSingleton.h"
#include "../include/OOBase/Thread.h"

#if defined(_WIN32)

#include <process.h>

OOBase::Thread::Thread()
{
}

OOBase::Thread::~Thread()
{
	abort();
}

int OOBase::Thread::run(int (*thread_fn)(void*), void* param)
{
	if (is_running())
		return ERROR_INVALID_PARAMETER;

	wrapper wrap;
	wrap.m_thread_fn = thread_fn;
	wrap.m_param = param;
	wrap.m_hEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
	wrap.m_pThis = this;
	if (!wrap.m_hEvent)
		return GetLastError();

	// Start the thread
	OOBase::Win32::SmartHandle hThread(reinterpret_cast<HANDLE>(_beginthreadex(NULL,0,&oobase_thread_fn,&wrap,0,NULL)));
	if (!hThread)
		return GetLastError();

	// Wait for the started signal or the thread terminating early
	HANDLE handles[2] =
	{
		wrap.m_hEvent,
		hThread
	};
	DWORD dwWait = WaitForMultipleObjects(2,handles,FALSE,INFINITE);
	if (dwWait != WAIT_OBJECT_0 && dwWait != WAIT_OBJECT_0+1)
		return GetLastError();

	m_hThread = hThread.detach();
	return 0;
}

bool OOBase::Thread::join(const Timeout& timeout)
{
	if (!m_hThread.valid())
		return true;

	DWORD dwWait = WaitForSingleObject(m_hThread,timeout.millisecs());
	if (dwWait == WAIT_OBJECT_0)
		return true;

	if (dwWait != WAIT_TIMEOUT)
		OOBase_CallCriticalFailure(GetLastError());

	return false;
}

void OOBase::Thread::abort()
{
	if (is_running())
		TerminateThread(m_hThread,1);
}

bool OOBase::Thread::is_running() const
{
	if (!m_hThread.valid())
		return false;

	DWORD dwWait = WaitForSingleObject(m_hThread,0);
	if (dwWait == WAIT_TIMEOUT)
		return true;

	if (dwWait != WAIT_OBJECT_0)
		OOBase_CallCriticalFailure(GetLastError());

	return false;
}

void OOBase::Thread::sleep(unsigned int millisecs)
{
	::Sleep(static_cast<DWORD>(millisecs));
}

unsigned int OOBase::Thread::oobase_thread_fn(void* param)
{
	wrapper* wrap = static_cast<wrapper*>(param);

	// Copy the values out before we signal
	Thread* pThis = wrap->m_pThis;
	int (*thread_fn)(void*) = wrap->m_thread_fn;
	void* p = wrap->m_param;

	// Hold a reference to ourselves for the lifetime of the thread function
	SharedPtr<Thread> ptrThis = pThis->shared_from_this();

	// Set the TLS for self
	if (!TLS::Set(&s_tls_key,pThis))
		OOBase_CallCriticalFailure(system_error());

	// Set the event, meaning we have started
	if (!SetEvent(wrap->m_hEvent))
		OOBase_CallCriticalFailure(GetLastError());

	return static_cast<unsigned int>((*thread_fn)(p));
}

const OOBase::Thread* OOBase::Thread::self()
{
	void* p = NULL;
	if (TLS::Get(&s_tls_key,&p))
		return const_cast<const Thread*>(static_cast<Thread*>(p));

	Thread* pThread = CrtAllocator::allocate_new<Thread>();
	if (!pThread)
		OOBase_CallCriticalFailure(system_error());

	// Do some silly windows nonsense to get the thread handle
	if (!DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&pThread->m_hThread,0,FALSE,DUPLICATE_SAME_ACCESS))
		OOBase_CallCriticalFailure(GetLastError());

	if (!TLS::Set(&s_tls_key,pThread,&destroy_thread_self))
		OOBase_CallCriticalFailure(system_error());

	return pThread;
}

void OOBase::Thread::destroy_thread_self(void* p)
{
	CrtAllocator::delete_free(static_cast<Thread*>(p));
}

#elif defined(HAVE_PTHREAD)

bool OOBase::Thread::join(const Timeout& timeout)
{
	if (!m_finished.wait(timeout))
		return false;

	// Join the thread
	void* ret = NULL;
	pthread_join(m_thread,&ret);

	return true;
}

void OOBase::Thread::abort()
{
	if (!m_finished.is_set())
	{
		int err = pthread_cancel(m_thread);
		if (err)
			OOBase_CallCriticalFailure(err);

		void* ret = NULL;
		pthread_join(m_thread,&ret);
	}
}

bool OOBase::Thread::is_running() const
{
	return !m_finished.is_set();
}

void OOBase::Thread::sleep(unsigned int millisecs)
{
#if defined(HAVE_PTHREAD)
	if (!millisecs)
	{
		pthread_yield();
		return;
	}
#endif

	::timespec wt = {0,0};
	Timeout(millisecs / 1000,(millisecs % 1000) * 1000).get_timespec(wt);

	int rc = -1;
	do
	{
		rc = ::nanosleep(&wt,&wt);
	}
	while (rc == -1 && errno == EINTR);

	if (rc == -1)
		OOBase_CallCriticalFailure(errno);
}

OOBase::Thread::Thread() :
		m_thread(pthread_t_def()),
		m_finished(true,false)
{
}

OOBase::Thread::~Thread()
{
	abort();
}

int OOBase::Thread::run(int (*thread_fn)(void*), void* param)
{
	if (!m_finished.is_set())
		return EINVAL;

	// Mark that we are running
	m_finished.reset();

	OOBase::Event started(false,false);

	wrapper wrap;
	wrap.m_pThis = this;
	wrap.m_thread_fn = thread_fn;
	wrap.m_param = param;
	wrap.m_started = &started;

	// Start the thread
	int err = pthread_create(&m_thread,NULL,&oobase_thread_fn,&wrap);
	if (err)
	{
		m_finished.set();
		return err;
	}

	// Wait for the start event
	started.wait();

	return 0;
}

void* OOBase::Thread::oobase_thread_fn(void* param)
{
	wrapper* wrap = static_cast<wrapper*>(param);

	// Copy the values out before we signal
	Thread* pThis = wrap->m_pThis;
	int (*thread_fn)(void*) = wrap->m_thread_fn;
	void* p = wrap->m_param;

	// Hold a reference to ourselves for the lifetime of the thread function
	SharedPtr<Thread> ptrThis = pThis->shared_from_this();

	// Set the TLS for self
	if (!TLS::Set(&s_tls_key,pThis))
		OOBase_CallCriticalFailure(system_error());

	// Set the event, meaning we have started
	wrap->m_started->set();

	// Run the actual thread function
	int ret = (*thread_fn)(p);

	// Signal we have finished
	pThis->m_finished.set();

	return reinterpret_cast<void*>(ret);
}

const OOBase::Thread* OOBase::Thread::self()
{
	void* p = NULL;
	if (TLS::Get(&s_tls_key,&p))
		return const_cast<const Thread*>(static_cast<Thread*>(p));

	Thread* pThread = CrtAllocator::allocate_new<Thread>();
	if (!pThread)
		OOBase_CallCriticalFailure(system_error());

	pThread->m_thread = pthread_self();

	if (!TLS::Set(&s_tls_key,pThread,&destroy_thread_self))
		OOBase_CallCriticalFailure(system_error());

	return pThread;
}

void OOBase::Thread::destroy_thread_self(void* p)
{
	CrtAllocator::delete_free(static_cast<Thread*>(p));
}

#endif // HAVE_PTHREAD

const int OOBase::Thread::s_tls_key = 0;

OOBase::SharedPtr<OOBase::Thread> OOBase::Thread::run(int (*thread_fn)(void*), void* param, int& err)
{
	SharedPtr<Thread> ptrThread = allocate_shared<Thread>();
	if (!ptrThread)
		err = system_error();
	else
	{
		err = ptrThread->run(thread_fn,param);
		if (err)
			ptrThread.reset();
	}
	return ptrThread;
}

void OOBase::Thread::yield()
{
	// Just perform a tiny sleep
	Thread::sleep(0);
}

OOBase::ThreadPool::ThreadPool()
{
}

OOBase::ThreadPool::~ThreadPool()
{
	abort();
}

int OOBase::ThreadPool::run(int (*thread_fn)(void*), void* param, size_t threads)
{
	for (size_t i=0;i<threads;++i)
	{
		int err = 0;
		SharedPtr<Thread> ptrThread = Thread::run(thread_fn,param,err);
		if (!ptrThread)
			return err;

		Guard<Mutex> guard(m_lock);

		bool bAdd = true;
		for (List<SharedPtr<Thread>,CrtAllocator>::iterator j = m_threads.begin();j;++j)
		{
			if (!(*j)->is_running())
			{
				*j = ptrThread;
				bAdd = false;
				break;
			}
		}
		if (bAdd)
		{
			if (!m_threads.push_back(ptrThread))
				return system_error();
		}

		guard.release();
	}

	return 0;
}

void OOBase::ThreadPool::join()
{
	Guard<Mutex> guard(m_lock);

	for (SharedPtr<Thread> ptrThread;!m_threads.pop_back(&ptrThread);)
	{
		guard.release();

		ptrThread->join();

		guard.acquire();
	}
}

void OOBase::ThreadPool::abort()
{
	Guard<Mutex> guard(m_lock);

	for (SharedPtr<Thread> ptrThread;!m_threads.pop_back(&ptrThread);)
	{
		guard.release();

		ptrThread->abort();

		guard.acquire();
	}
}

size_t OOBase::ThreadPool::number_running() const
{
	Guard<Mutex> guard(m_lock);

	size_t count = 0;
	for (List<SharedPtr<Thread>,CrtAllocator>::const_iterator i=m_threads.begin();i;++i)
	{
		if ((*i)->is_running())
			++count;
	}

	return count;
}
