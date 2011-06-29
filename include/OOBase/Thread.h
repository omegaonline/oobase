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
#include "Stack.h"

namespace OOBase
{
	class Thread
	{
	public:
		Thread(bool bAutodelete);
		virtual ~Thread();

		int run(int (*thread_fn)(void*), void* param);

		virtual bool join(const timeval_t* wait = NULL);
		virtual void abort();
		virtual bool is_running();

		static void sleep(const timeval_t& wait);
		static void yield();

		static Thread* self();

	protected:
		explicit Thread(bool,bool);

		virtual int run(Thread* /*pThread*/, bool /*bAutodelete*/, int (* /*thread_fn*/)(void*), void* /*param*/) { assert(false); return false; };

		static size_t s_sentinal;

	private:
		Thread(const Thread&);
		Thread& operator = (const Thread&);

		Thread*    m_impl;
		const bool m_bAutodelete;
	};

	class ThreadPool
	{
	public:
		ThreadPool();
		~ThreadPool();

		int run(int (*thread_fn)(void*), void* param, size_t threads);
		void join();
		void abort();
		size_t number_running();

	private:
		SpinLock       m_lock;
		Stack<Thread*> m_threads;
	};
}

#endif // OOBASE_THREAD_H_INCLUDED_
