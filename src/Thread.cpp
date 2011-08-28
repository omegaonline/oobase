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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/TLSSingleton.h"
#include "../include/OOBase/Thread.h"

#if defined(_WIN32)
#include <process.h>
#endif

size_t OOBase::Thread::s_sentinal = 0;

namespace
{

#if defined(_WIN32)

	class Win32Thread : public OOBase::Thread
	{
	public:
		Win32Thread();
		virtual ~Win32Thread();

		int run(Thread* pThread, bool bAutodelete, int (*thread_fn)(void*), void* param);
		bool join(const OOBase::timeval_t* wait = NULL);
		void abort();
		bool is_running();

	private:
		struct wrapper
		{
			OOBase::Win32::SmartHandle m_hEvent;
			int (*m_thread_fn)(void*);
			void*                      m_param;
			Thread*                    m_pThread;
			bool                       m_bAutodelete;
		};

		OOBase::SpinLock           m_lock;
		OOBase::Win32::SmartHandle m_hThread;

		static unsigned int __stdcall oobase_thread_fn(void* param);
	};

	Win32Thread::Win32Thread() :
			OOBase::Thread(false,false),
			m_hThread(NULL)
	{
	}

	Win32Thread::~Win32Thread()
	{
		abort();
	}

	bool Win32Thread::is_running()
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		if (!m_hThread.is_valid())
			return false;

		DWORD dwWait = WaitForSingleObject(m_hThread,0);
		if (dwWait == WAIT_TIMEOUT)
		{
			m_hThread.close();
			return true;
		}
		else if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());

		return false;
	}

	int Win32Thread::run(Thread* pThread, bool bAutodelete, int (*thread_fn)(void*), void* param)
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		// Close any open handles, this allows restarting
		m_hThread.close();

		wrapper wrap;
		wrap.m_thread_fn = thread_fn;
		wrap.m_param = param;
		wrap.m_hEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
		wrap.m_pThread = pThread;
		wrap.m_bAutodelete = bAutodelete;
		if (!wrap.m_hEvent)
			return GetLastError();

		// Start the thread
		m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL,0,&oobase_thread_fn,&wrap,0,NULL));
		if (!m_hThread)
			return GetLastError();

		// Wait for the started signal or the thread terminating early
		HANDLE handles[2] =
		{
			wrap.m_hEvent,
			m_hThread
		};
		DWORD dwWait = WaitForMultipleObjects(2,handles,FALSE,INFINITE);

		if (dwWait != WAIT_OBJECT_0 && dwWait != WAIT_OBJECT_0+1)
			return GetLastError();

		return 0;
	}

	bool Win32Thread::join(const OOBase::timeval_t* timeout)
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		if (!m_hThread.is_valid())
			return true;

		DWORD dwWait = WaitForSingleObject(m_hThread,(timeout ? timeout->msec() : INFINITE));
		if (dwWait == WAIT_OBJECT_0)
		{
			m_hThread.close();
			return true;
		}
		else if (dwWait != WAIT_TIMEOUT)
			OOBase_CallCriticalFailure(GetLastError());

		return false;
	}

	void Win32Thread::abort()
	{
		if (is_running())
			TerminateThread(m_hThread,1);
	}

	unsigned int Win32Thread::oobase_thread_fn(void* param)
	{
		wrapper* wrap = static_cast<wrapper*>(param);

		// Copy the values out before we signal
		Thread* pThread = wrap->m_pThread;
		int (*thread_fn)(void*) = wrap->m_thread_fn;
		void* p = wrap->m_param;
		bool bAutodelete = wrap->m_bAutodelete;

		// Set our self pointer in TLS
		int err = OOBase::TLS::Set(&s_sentinal,pThread);
		if (err != 0)
			OOBase_CallCriticalFailure(err);

		// Set the event, meaning we have started
		if (!SetEvent(wrap->m_hEvent))
			OOBase_CallCriticalFailure(GetLastError());

		unsigned int ret = static_cast<unsigned int>((*thread_fn)(p));

		if (bAutodelete)
			delete pThread;

		// Make sure we clean up any thread-local storage
		OOBase::TLS::ThreadExit();

		return ret;
	}

#endif // _WIN32

#if defined(HAVE_PTHREAD)

	class PthreadThread : public OOBase::Thread
	{
	public:
		PthreadThread();
		virtual ~PthreadThread();

		int run(Thread* pThread, bool bAutodelete, int (*thread_fn)(void*), void* param);
		bool join(const OOBase::timeval_t* wait = NULL);
		void abort();
		bool is_running();

	private:
		struct wrapper
		{
			PthreadThread*     m_pThis;
			int (*m_thread_fn)(void*);
			void*              m_param;
			bool               m_bAutodelete;
			Thread*            m_pThread;
		};

		OOBase::SpinLock m_lock;
		bool             m_running;
		pthread_t        m_thread;
		pthread_cond_t   m_condition;

		static void* oobase_thread_fn(void* param);
	};

	PthreadThread::PthreadThread() :
			OOBase::Thread(false,false),
			m_running(false)
	{
		int err = pthread_cond_init(&m_condition,NULL);
		if (err)
			OOBase_CallCriticalFailure(err);
	}

	PthreadThread::~PthreadThread()
	{
		abort();

		pthread_cond_destroy(&m_condition);
	}

	bool PthreadThread::is_running()
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		return m_running;
	}

	int PthreadThread::run(Thread* pThread, bool bAutodelete, int (*thread_fn)(void*), void* param)
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		assert(!m_running);

		wrapper wrap;
		wrap.m_pThis = this;
		wrap.m_pThread = pThread;
		wrap.m_thread_fn = thread_fn;
		wrap.m_param = param;
		wrap.m_bAutodelete = bAutodelete;

		pthread_attr_t attr;
		int err = pthread_attr_init(&attr);
		if (err)
			return err;

		// Start the thread
		err = pthread_create(&m_thread,NULL,&oobase_thread_fn,&wrap);
		if (err)
			return err;

		pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;
		err = pthread_mutex_lock(&start_mutex);
		if (err)
			OOBase_CallCriticalFailure(err);

		// Wait for the started signal
		while (!m_running)
		{
			err = pthread_cond_wait(&m_condition,&start_mutex);
			if (err)
				OOBase_CallCriticalFailure(err);
		}

		pthread_mutex_unlock(&start_mutex);

		return 0;
	}

	bool PthreadThread::join(const OOBase::timeval_t* timeout)
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		if (timeout)
		{
			pthread_mutex_t join_mutex = PTHREAD_MUTEX_INITIALIZER;
			int err = pthread_mutex_lock(&join_mutex);
			if (err)
				OOBase_CallCriticalFailure(err);

			// Wait for the started signal
			while (m_running)
			{
				timespec wt;
				OOBase::timeval_t now = OOBase::timeval_t::gettimeofday();
				now += *timeout;
				wt.tv_sec = now.tv_sec();
				wt.tv_nsec = now.tv_usec() * 1000;
				err = pthread_cond_timedwait(&m_condition,&join_mutex,&wt);

				if (err)
				{
					if (err != ETIMEDOUT)
						OOBase_CallCriticalFailure(err);
					break;
				}
			}

			pthread_mutex_unlock(&join_mutex);

			if (err == ETIMEDOUT)
				return false;
		}

		if (m_running)
		{
			void* ret = NULL;
			pthread_join(m_thread,&ret);
		}

		return true;
	}

	void PthreadThread::abort()
	{
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		if (m_running)
		{
			int err = pthread_cancel(m_thread);
			if (err)
				OOBase_CallCriticalFailure(err);

			void* ret = NULL;
			pthread_join(m_thread,&ret);
		}
	}

	void* PthreadThread::oobase_thread_fn(void* param)
	{
		wrapper* wrap = static_cast<wrapper*>(param);

		// Copy the values out before we signal
		Thread* pThread = wrap->m_pThread;
		PthreadThread* pThis = wrap->m_pThis;
		int (*thread_fn)(void*) = wrap->m_thread_fn;
		void* p = wrap->m_param;
		bool bAutodelete = wrap->m_bAutodelete;

		// Set our self pointer in TLS
		int err = OOBase::TLS::Set(&s_sentinal,pThread);
		if (err != 0)
			OOBase_CallCriticalFailure(err);

		pThis->m_running = true;

		// Set the event, meaning we have started
		err = pthread_cond_signal(&pThis->m_condition);
		if (err)
			OOBase_CallCriticalFailure(err);

		int ret = (*thread_fn)(p);

		pThis->m_running = false;

		err = pthread_cond_signal(&pThis->m_condition);
		if (err)
			OOBase_CallCriticalFailure(err);

		if (bAutodelete)
			delete pThread;

		return (void*)ret;
	}

#endif // HAVE_PTHREAD

}

OOBase::Thread::Thread(bool bAutodelete) :
		m_impl(NULL),
		m_bAutodelete(bAutodelete)
{
#if defined(_WIN32)
	m_impl = new (critical) Win32Thread();
#elif defined(HAVE_PTHREAD)
	m_impl = new (critical) PthreadThread();
#else
#error Fix me!
#endif
}

OOBase::Thread::Thread(bool,bool) :
		m_impl(NULL),
		m_bAutodelete(false)
{
}

OOBase::Thread::~Thread()
{
	delete m_impl;
}

int OOBase::Thread::run(int (*thread_fn)(void*), void* param)
{
	assert(this != self());
	assert(!is_running());

	return m_impl->run(this,m_bAutodelete,thread_fn,param);
}

bool OOBase::Thread::join(const timeval_t* timeout)
{
	assert(this != self());

	return m_impl->join(timeout);
}

void OOBase::Thread::abort()
{
	assert(this != self());

	return m_impl->abort();
}

bool OOBase::Thread::is_running()
{
	if (this == self())
		return true;

	return m_impl->is_running();
}

void OOBase::Thread::yield()
{
#if defined(_WIN32)
	::Sleep(0);
#elif defined(HAVE_PTHREAD)
	pthread_yield();
#else
	// Just perform a tiny sleep
	Thread::sleep(timeval_t(0,1));
#endif
}

void OOBase::Thread::sleep(const timeval_t& timeout)
{
	if (timeout == timeval_t::Zero)
		return Thread::yield();

#if defined(_WIN32)
	::Sleep(timeout.msec());
#else
	timespec wt;
	wt.tv_sec = timeout.tv_sec();
	wt.tv_nsec = timeout.tv_usec() * 1000;

	for (;;)
	{
		if (!nanosleep(&wt,&wt))
			break;

		if (errno != EINTR)
			OOBase_CallCriticalFailure(errno);
	}
#endif
}

OOBase::Thread* OOBase::Thread::self()
{
	void* v = NULL;
	OOBase::TLS::Get(&s_sentinal,&v);
	return static_cast<OOBase::Thread*>(v);
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
	for (size_t j=0;j<threads;++j)
	{
		Thread* pThread = new (std::nothrow) Thread(false);
		if (!pThread)
			return ERROR_OUTOFMEMORY;

		Guard<SpinLock> guard(m_lock);

		bool bAdd = true;
		for (size_t i = 0;i<m_threads.size();++i)
		{
			Thread** ppThread = m_threads.at(i);
			if (!*ppThread || !(*ppThread)->is_running())
			{
				delete *ppThread;
				if (!bAdd)
				{
					*ppThread = pThread;
					bAdd = false;
				}
				else
					*ppThread = NULL;
			}
		}
		if (bAdd)
		{
			int err = m_threads.push(pThread);
			if (err != 0)
				return err;
		}

		guard.release();

		int err = pThread->run(thread_fn,param);
		if (err != 0)
			return err;
	}
		
	return 0;
}

void OOBase::ThreadPool::join()
{
	for (;;)
	{
		Guard<SpinLock> guard(m_lock);

		if (m_threads.empty())
			break;
		
		Thread* pThread = *m_threads.at(m_threads.size()-1);
		if (pThread)
		{
			guard.release();

			pThread->join();

			guard.acquire();
		
			if (!m_threads.empty() && pThread == *m_threads.at(m_threads.size()-1))
			{
				m_threads.pop();
				delete pThread;
			}
		}
		else
			m_threads.pop();
	}
}

void OOBase::ThreadPool::abort()
{
	for (;;)
	{
		Guard<SpinLock> guard(m_lock);

		Thread* pThread = NULL;
		if (!m_threads.pop(&pThread))
			break;

		if (pThread)
			pThread->abort();
		delete pThread;
	}
}

size_t OOBase::ThreadPool::number_running()
{
	Guard<SpinLock> guard(m_lock);

	size_t count = 0;
	for (size_t i=0;i<m_threads.size();++i)
	{
		Thread* pThread = *m_threads.at(i);
		if (pThread && pThread->is_running())
			++count;
	}

	return count;
}
