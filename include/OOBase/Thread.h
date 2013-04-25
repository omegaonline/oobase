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

namespace OOBase
{
	class Thread : public NonCopyable
	{
	public:
		Thread(bool bAutodelete);
		virtual ~Thread();

		virtual int run(int (*thread_fn)(void*), void* param);

		virtual bool join(const Timeout& timeout = Timeout());
		virtual void abort();
		virtual bool is_running() const;

		static void sleep(unsigned int millisecs);
		static void yield();

	protected:
		Thread(bool bAutodelete,bool);

		const bool m_bAutodelete;

	private:
		Thread*    m_impl;
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
		List<SmartPtr<Thread>,CrtAllocator> m_threads;
	};
}

#endif // OOBASE_THREAD_H_INCLUDED_
