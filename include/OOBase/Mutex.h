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

#ifndef OOBASE_MUTEX_H_INCLUDED_
#define OOBASE_MUTEX_H_INCLUDED_

#include "Timeout.h"
#include "StackAllocator.h"
#include "Win32.h"

namespace OOBase
{
	/// A recursive mutex that can be acquired with a timeout
	class Mutex : public NonCopyable
	{
	public:
		Mutex();
		~Mutex();

		bool try_acquire();
		void acquire();
		void release();

#if defined(_WIN32)
		bool acquire(const Timeout& timeout);
#endif

	private:
		/** \var m_mutex
		 *  The platform specific mutex variable.
		 */
#if defined(_WIN32)
		Win32::SmartHandle m_mutex;
#elif defined(HAVE_PTHREAD)
		friend class Condition;

	protected:
		pthread_mutex_t    m_mutex;

		Mutex(bool as_spin_lock);

		void init(bool as_spin_lock);
#else
#error Implement platform native mutex
#endif
	};

#if defined(_WIN32) || defined(DOXYGEN)
	/// A non-recursive mutex that spins in user-mode before acquiring the kernel mutex
	class SpinLock : public NonCopyable
	{
	public:
		SpinLock();
		~SpinLock();

		bool try_acquire();
		void acquire();
		void release();

	private:
		CRITICAL_SECTION m_cs;
	};
#elif defined(HAVE_PTHREAD) && defined(_POSIX_SPIN_LOCKS)
	class SpinLock : public NonCopyable
	{
	public:
		SpinLock();
		~SpinLock();

		bool try_acquire();
		void acquire();
		void release();

	private:
		pthread_spinlock_t m_sl;
	};
#else
	class SpinLock : public Mutex
	{
	public:
		SpinLock() : Mutex(true)
		{}
	};
#endif

	class RWMutex : public NonCopyable
	{
	public:
		RWMutex();
		~RWMutex();

		// Write lock
		bool try_acquire();
		void acquire();
		void release();

		// Read lock
		bool try_acquire_read();
		void acquire_read();
		void release_read();

	private:
#if defined(_WIN32)
		SRWLOCK          m_lock;
#elif defined(HAVE_PTHREAD)
		pthread_rwlock_t m_mutex;
#else
#error Implement platform native reader/writer locks
#endif
	};

	template <typename MUTEX>
	class Guard : public NonCopyable
	{
	public:
		Guard(MUTEX& mutex, bool acq = true) :
				m_acquired(false),
				m_mutex(mutex)
		{
			if (acq)
				acquire();
		}

		~Guard()
		{
			if (m_acquired)
				release();
		}

		bool try_acquire()
		{
			if (!m_mutex.try_acquire())
				return false;

			m_acquired = true;
			return true;
		}

		void acquire()
		{
			m_mutex.acquire();

			m_acquired = true;
		}

		bool acquire(const Timeout& timeout)
		{
			return (m_acquired = m_mutex.acquire(timeout));
		}

		void release()
		{
			m_acquired = false;

			m_mutex.release();
		}

	private:
		bool   m_acquired;
		MUTEX& m_mutex;
	};

	template <typename MUTEX>
	class ReadGuard : public NonCopyable
	{
	public:
		ReadGuard(MUTEX& mutex, bool acq = true) :
				m_acquired(false),
				m_mutex(mutex)
		{
			if (acq)
				acquire();
		}

		~ReadGuard()
		{
			if (m_acquired)
				release();
		}

		bool try_acquire()
		{
			if (!m_mutex.try_acquire_read())
				return false;

			m_acquired = true;
			return true;
		}

		void acquire()
		{
			m_mutex.acquire_read();

			m_acquired = true;
		}

		void release()
		{
			m_acquired = false;

			m_mutex.release_read();
		}

	private:
		bool   m_acquired;
		MUTEX& m_mutex;
	};

	template <size_t SIZE, typename Allocator = CrtAllocator>
	class LockedAllocator : public ScratchAllocator, public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;

	public:
		LockedAllocator() : ScratchAllocator(m_buffer,sizeof(m_buffer)), baseClass()
		{
			static_assert(sizeof(m_buffer) < LockedAllocator::index_t(-1),"SIZE too big");
		}

		LockedAllocator(AllocatorInstance& allocator) : ScratchAllocator(m_buffer,sizeof(m_buffer)), baseClass(allocator)
		{
			static_assert(sizeof(m_buffer) < LockedAllocator::index_t(-1),"SIZE too big");
		}

		void* allocate(size_t bytes, size_t align)
		{
			Guard<SpinLock> guard(m_lock);

			void* p = ScratchAllocator::allocate(bytes,align);
			if (p)
				return p;

			guard.release();

			return baseClass::allocate(bytes,align);
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			if (!ptr)
				return allocate(bytes,align);

			Guard<SpinLock> guard(m_lock);

			if (is_our_ptr(ptr))
				return ScratchAllocator::reallocate(ptr,bytes,align);

			guard.release();

			return baseClass::reallocate(ptr,bytes,align);
		}

		void free(void* ptr)
		{
			if (!ptr)
				return;

			Guard<SpinLock> guard(m_lock);

			if (is_our_ptr(ptr))
				return ScratchAllocator::free(ptr);

			guard.release();

			baseClass::free(ptr);
		}

	private:
		SpinLock m_lock;
		char     m_buffer[((SIZE / sizeof(index_t))+(SIZE % sizeof(index_t) ? 1 : 0)) * sizeof(index_t)];
	};
}

#endif // OOBASE_MUTEX_H_INCLUDED_
