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

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/TLSSingleton.h"
#include "../include/OOBase/Thread.h"
#include "../include/OOBase/Condition.h"

#if defined(_WIN32)
#include <process.h>
#endif

namespace
{

#if defined(_WIN32)

	class Win32Thread : public OOBase::Thread
	{
	public:
		Win32Thread(bool bAutodelete);
		virtual ~Win32Thread();

		int run(int (*thread_fn)(void*), void* param);
		bool join(const OOBase::Timeout& timeout);
		void abort();
		bool is_running();

	private:
		struct wrapper
		{
			OOBase::Win32::SmartHandle m_hEvent;
			int (*m_thread_fn)(void*);
			void*                      m_param;
			Win32Thread*               m_pThis;
		};

		OOBase::Win32::SmartHandle m_hThread;

		static unsigned int __stdcall oobase_thread_fn(void* param);
	};

	Win32Thread::Win32Thread(bool bAutodelete) :
			OOBase::Thread(bAutodelete,false)
	{
	}

	Win32Thread::~Win32Thread()
	{
		abort();
	}

	bool Win32Thread::is_running()
	{
		if (!m_hThread.is_valid())
			return false;

		DWORD dwWait = WaitForSingleObject(m_hThread,0);
		if (dwWait == WAIT_TIMEOUT)
			return true;

		if (dwWait != WAIT_OBJECT_0)
			OOBase_CallCriticalFailure(GetLastError());

		return false;
	}

	int Win32Thread::run(int (*thread_fn)(void*), void* param)
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
		{
			TerminateThread(hThread,1);
			return GetLastError();
		}

		m_hThread = hThread.detach();
		return 0;
	}

	bool Win32Thread::join(const OOBase::Timeout& timeout)
	{
		if (!m_hThread.is_valid())
			return true;

		DWORD dwWait = WaitForSingleObject(m_hThread,timeout.millisecs());
		if (dwWait == WAIT_OBJECT_0)
			return true;

		if (dwWait != WAIT_TIMEOUT)
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
		Win32Thread* pThis = wrap->m_pThis;
		int (*thread_fn)(void*) = wrap->m_thread_fn;
		void* p = wrap->m_param;

		// Set the event, meaning we have started
		if (!SetEvent(wrap->m_hEvent))
			OOBase_CallCriticalFailure(GetLastError());

		unsigned int ret = static_cast<unsigned int>((*thread_fn)(p));

		if (pThis->m_bAutodelete)
			delete pThis;

		// Make sure we clean up any thread-local storage
		OOBase::TLS::ThreadExit();

		return ret;
	}

#endif // _WIN32

#if defined(HAVE_PTHREAD)

	class PthreadThread : public OOBase::Thread
	{
	public:
		PthreadThread(bool bAutodelete);
		virtual ~PthreadThread();

		int run(int (*thread_fn)(void*), void* param);
		bool join(const OOBase::Timeout& timeout);
		void abort();
		bool is_running();

	private:
		struct wrapper
		{
			PthreadThread*        m_pThis;
			int (*m_thread_fn)(void*);
			void*                 m_param;
			OOBase::Event*        m_started;
		};

		pthread_t        m_thread;
		OOBase::Event    m_finished;

		static const pthread_t pthread_t_def()
		{
			static const pthread_t t = {0};
			return t;
		}

		static void* oobase_thread_fn(void* param);
	};

	PthreadThread::PthreadThread(bool bAutodelete) :
			OOBase::Thread(bAutodelete,false),
			m_thread(pthread_t_def()),
			m_finished(false,false)
	{}

	PthreadThread::~PthreadThread()
	{
		abort();
	}

	bool PthreadThread::is_running()
	{
		return m_finished.is_set();
	}

	int PthreadThread::run(int (*thread_fn)(void*), void* param)
	{
		if (m_finished.is_set())
			return EINVAL;

		OOBase::Event started(false,false);

		wrapper wrap;
		wrap.m_pThis = this;
		wrap.m_thread_fn = thread_fn;
		wrap.m_param = param;
		wrap.m_started = &started;

		// Start the thread
		int err = pthread_create(&m_thread,NULL,&oobase_thread_fn,&wrap);
		if (err)
			return err;

		// Wait for the start event
		started.wait();

		return 0;
	}

	bool PthreadThread::join(const OOBase::Timeout& timeout)
	{
		if (!m_finished.wait(timeout))
			return false;

		// Join the thread
		void* ret = NULL;
		pthread_join(m_thread,&ret);

		return true;
	}

	void PthreadThread::abort()
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

	void* PthreadThread::oobase_thread_fn(void* param)
	{
		wrapper* wrap = static_cast<wrapper*>(param);

		// Copy the values out before we signal
		PthreadThread* pThis = wrap->m_pThis;
		int (*thread_fn)(void*) = wrap->m_thread_fn;
		void* p = wrap->m_param;

		// Set the event, meaning we have started
		wrap->m_started->set();

		// Mark that we are running
		pThis->m_finished.reset();

		// Run the actual thread function
		int ret = (*thread_fn)(p);

		// Signal we have finished
		pThis->m_finished.set();

		if (pThis->m_bAutodelete)
			delete pThis;

		return reinterpret_cast<void*>(ret);
	}

#endif // HAVE_PTHREAD

}

OOBase::Thread::Thread(bool bAutodelete) :
		m_bAutodelete(bAutodelete),
		m_impl(NULL)
{
#if defined(_WIN32)
		m_impl = new (critical) Win32Thread(bAutodelete);
#elif defined(HAVE_PTHREAD)
		m_impl = new (critical) PthreadThread(bAutodelete);
#else
#error Implement platform native thread wrapper
#endif
}

OOBase::Thread::Thread(bool bAutodelete, bool) :
		m_bAutodelete(bAutodelete),
		m_impl(NULL)
{
}

OOBase::Thread::~Thread()
{
	delete m_impl;
}

int OOBase::Thread::run(int (*thread_fn)(void*), void* param)
{
	assert(!is_running());

	return m_impl->run(thread_fn,param);
}

bool OOBase::Thread::join(const Timeout& timeout)
{
	return m_impl->join(timeout);
}

void OOBase::Thread::abort()
{
	return m_impl->abort();
}

bool OOBase::Thread::is_running()
{
	return m_impl->is_running();
}

void OOBase::Thread::yield()
{
	// Just perform a tiny sleep
	Thread::sleep(0);
}

void OOBase::Thread::sleep(unsigned int millisecs)
{
#if defined(_WIN32)
	::Sleep(static_cast<DWORD>(millisecs));
#else

#if defined(HAVE_PTHREAD)
	if (!millisecs)
	{
		pthread_yield();
		return;
	}
#endif

	::timespec wt = {0};
	Timeout(millisecs / 1000,(millisecs % 1000) * 1000).get_timespec(wt);

	int rc = -1;
	do
	{
		rc = ::nanosleep(&wt,&wt);
	}
	while (rc == -1 && errno == EINTR);

	if (rc == -1)
		OOBase_CallCriticalFailure(errno);
#endif
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
		Thread* pThread = new (std::nothrow) Thread(false);
		if (!pThread)
			return ERROR_OUTOFMEMORY;

		Guard<SpinLock> guard(m_lock);

		bool bAdd = true;
		for (size_t j = 0;j<m_threads.size();++j)
		{
			Thread** ppThread = m_threads.at(j);
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
			int err = m_threads.add(pThread);
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
