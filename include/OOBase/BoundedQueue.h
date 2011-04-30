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

#ifndef OOBASE_BOUNDED_QUEUE_H_INCLUDED_
#define OOBASE_BOUNDED_QUEUE_H_INCLUDED_

#include "Memory.h"
#include "Condition.h"
#include "Thread.h"
#include "Queue.h"

namespace OOBase
{
	template <typename T>
	class BoundedQueue
	{
	public:
		enum Result
		{
			success = 0,
			timedout,
			closed,
			pulsed
		};

		BoundedQueue(size_t bound = 10) :
				m_bound(bound),
				m_waiters(0),
				m_closed(false),
				m_pulsed(false)
		{}

		Result push(const T& val, const timeval_t* wait = 0)
		{
			timeval_t wait2;
			if (wait)
				wait2 = *wait;
			Countdown countdown(&wait2);

			Guard<Condition::Mutex> guard(m_lock);

			while (!m_closed && m_queue.size() >= m_bound)
			{
				if (!m_space.wait(m_lock,(wait ? &wait2 : 0)))
					return timedout;
			}

			if (m_closed)
				return closed;

			int err = m_queue.push(val);
			assert(err == 0);
			
			m_available.signal();

			return success;
		}

		Result pop(T& val, const timeval_t* wait = 0)
		{
			timeval_t wait2;
			if (wait)
				wait2 = *wait;
			Countdown countdown(&wait2);

			Guard<Condition::Mutex> guard(m_lock);

			while (!m_pulsed && !m_closed && m_queue.empty())
			{
				++m_waiters;

				if (!m_available.wait(m_lock,(wait ? &wait2 : 0)))
				{
					--m_waiters;
					return timedout;
				}

				--m_waiters;
			}

			if (m_pulsed)
				return pulsed;

			if (m_queue.pop(&val))
			{
				m_space.signal();				

				return success;
			}

			// Must be closed
			assert(m_closed);
			return closed;
		}

		void close()
		{
			Guard<Condition::Mutex> guard(m_lock);

			if (!m_closed)
			{
				m_available.broadcast();
				m_space.broadcast();
			}

			m_closed = true;
		}

		void pulse()
		{
			Guard<Condition::Mutex> guard(m_lock);

			if (!m_pulsed)
			{
				m_pulsed = true;

				m_available.broadcast();
				
				while (m_waiters)
				{
					// Give any waiting threads a chance to wake...
					guard.release();

					// Let them pick up...
					Thread::yield();

					// Now re-acquire in order to check again
					guard.acquire();
				}

				m_pulsed = false;
			}
		}

	private:
		size_t           m_bound;
		size_t           m_waiters;
		bool             m_closed;
		bool             m_pulsed;
		Condition::Mutex m_lock;
		Condition        m_available;
		Condition        m_space;

		Queue<T> m_queue;
	};
}

#endif // OOBASE_BOUNDED_QUEUE_H_INCLUDED_
