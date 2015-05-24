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

#ifndef OOBASE_THREAD_H_INCLUDED_
#define OOBASE_THREAD_H_INCLUDED_

#include "Mutex.h"
#include "List.h"
#include "SharedPtr.h"
#include "Condition.h"

namespace OOBase
{
	class Thread : public NonCopyable, public EnableSharedFromThis<Thread>
	{
		friend class AllocateNewStatic<CrtAllocator>;

	public:
		~Thread();

		static SharedPtr<Thread> run(int (*thread_fn)(void*), void* param, int& err);

		bool join(const Timeout& timeout = Timeout());
		void abort();
		bool is_running() const;

		static void sleep(unsigned int millisecs);
		static void yield();

		static const Thread* self();

	private:
		Thread();

		int run(int (*thread_fn)(void*), void* param);

		static const int s_tls_key = 0;
		
#if defined(_WIN32)
		struct wrapper
		{
			Win32::SmartHandle m_hEvent;
			int (*m_thread_fn)(void*);
			void*              m_param;
			Thread*            m_pThis;
		};

		Win32::SmartHandle m_hThread;

		static void destroy_thread_self(void* p);
		static unsigned int __stdcall oobase_thread_fn(void* param);
#elif defined(HAVE_PTHREAD)
		struct wrapper
		{
			Thread*       m_pThis;
			int (*m_thread_fn)(void*);
			void*         m_param;
			Event*        m_started;
		};

		pthread_t m_thread;
		Event     m_finished;

		static pthread_t pthread_t_def()
		{
			static const pthread_t t = {0};
			return t;
		}

		static void* oobase_thread_fn(void* param);
#endif
	};

	class ThreadPool
	{
	public:
		ThreadPool();
		~ThreadPool();

		int run(int (*thread_fn)(void*), void* param, size_t threads);
		void join();
		void abort();
		size_t number_running() const;

	private:
		mutable Mutex m_lock;
		List<SharedPtr<Thread> > m_threads;
	};
}

#endif // OOBASE_THREAD_H_INCLUDED_
