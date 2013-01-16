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

#ifndef OOBASE_CONDITION_H_INCLUDED_
#define OOBASE_CONDITION_H_INCLUDED_

#include "Mutex.h"

namespace OOBase
{
	class Condition
	{
	public:
		/** \typedef Mutex
		 *  The Condition mutex type.
		 *  Individual platforms require a different type of mutex for condition variables
		 *  e.g. \link Win32::condition_mutex_t \endlink or \link OOBase::Mutex \endlink
		 */
#if defined(_WIN32)
		typedef Win32::condition_mutex_t Mutex;
#elif defined(HAVE_PTHREAD)
		typedef OOBase::SpinLock Mutex;
#else
#error Implement platform native condition variables
#endif

		Condition();
		~Condition();

		bool wait(Condition::Mutex& mutex, const Timeout& timeout = OOBase::Timeout());
		void signal();
		void broadcast();

	private:
		Condition(const Condition&);
		Condition& operator = (const Condition&);

		/** \var m_var
		 *  The platform specific condition variable.
		 */
#if defined(_WIN32)
	#if (_WIN32_WINNT < 0x0600)
		typedef OOBase::Win32::condition_variable_t* CONDITION_VARIABLE;
	#endif

		CONDITION_VARIABLE m_var;
#elif defined(HAVE_PTHREAD)
		pthread_cond_t     m_var;
#endif
	};

	class Event
	{
	public:
		Event(bool bSet = false, bool bAutoReset = true);
		~Event();

		bool is_set();
		void set();
		bool wait(const Timeout& timeout = OOBase::Timeout());
		void reset();

	private:
		Event(const Event&);
		Event& operator = (const Event&);

		/** \var m_var
		 *  The platform specific event variable.
		 *  We use an OOBase::Condition for non Win32 platforms
		 */
#if defined(_WIN32)
		Win32::SmartHandle m_handle;
#else
		Condition          m_condition;
		Condition::Mutex   m_lock;
		bool               m_bAuto;
		bool               m_bSet;
#endif
	};

	template <typename T>
	class Future
	{
	public:
		Future() : m_complete(false)
		{}

		Future(const T& v) : m_complete(false), m_value(v)
		{}

		void signal(const T& v)
		{
			Guard<Condition::Mutex> guard(m_lock);
			m_complete = true;
			m_value = v;
			m_condition.signal();
		}

		void broadcast(const T& v)
		{
			Guard<Condition::Mutex> guard(m_lock);
			m_complete = true;
			m_value = v;
			m_condition.broadcast();
		}

		bool wait(T& v, bool bReset, const Timeout& timeout)
		{
			OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
			while (!m_complete)
			{
				if (!m_condition.wait(m_lock,timeout))
					return false;
			}

			if (bReset)
				m_complete = false;

			v = m_value;
			return true;
		}

		const T& wait(bool bReset)
		{
			OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
			while (!m_complete)
				m_condition.wait(m_lock);

			if (bReset)
				m_complete = false;

			return m_value;
		}

	private:
		Future(const Future&);
		Future& operator = (const Future&);

		Condition        m_condition;
		Condition::Mutex m_lock;
		bool             m_complete;
		T                m_value;
	};
}

#endif // OOBASE_CONDITION_H_INCLUDED_
