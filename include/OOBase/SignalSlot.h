///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2014 Rick Taylor
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

#ifndef OOBASE_SIGNALSLOT_H_INCLUDED_
#define OOBASE_SIGNALSLOT_H_INCLUDED_

#include "Delegate.h"
#include "Vector.h"

namespace OOBase
{
	template <typename Allocator = CrtAllocator>
	class Signal0
	{
	private:
		typedef Delegate0<void,Allocator> delegate_t;
		mutable Vector<delegate_t,Allocator> m_slots;

	public:
		Signal0()
		{}

		Signal0(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		bool connect(T* p, void (T::*slot)())
		{
			return m_slots.push_back(delegate_t(p,slot));
		}

		bool connect(void (*slot)())
		{
			return m_slots.push_back(delegate_t(slot));
		}

		template <typename T>
		bool disconnect(T* p, void (T::*slot)())
		{
			return m_slots.erase(delegate_t(p,slot));
		}

		void fire() const
		{
			Vector<delegate_t,Allocator> slots(m_slots);
			for (typename Vector<delegate_t,Allocator>::iterator i=slots.begin();i;++i)
			{
				if (!*i)
					m_slots.erase(*i);
				else
					i->invoke();
			}
		}
	};

	template <typename P1, typename Allocator = CrtAllocator>
	class Signal1
	{
	private:
		typedef Delegate1<void,P1,Allocator> delegate_t;
		mutable Vector<delegate_t,Allocator> m_slots;

	public:
		Signal1()
		{}

		Signal1(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		bool connect(T* p, void (T::*slot)(P1))
		{
			return m_slots.push_back(delegate_t(p,slot));
		}

		bool connect(void (*slot)(P1))
		{
			return m_slots.push_back(delegate_t(slot));
		}

		template <typename T>
		bool disconnect(T* p, void (T::*slot)(P1))
		{
			return m_slots.remove(delegate_t(p,slot));
		}

		void fire(typename call_traits<P1>::param_type p1) const
		{
			Vector<delegate_t,Allocator> slots(m_slots);
			for (typename Vector<delegate_t,Allocator>::iterator i=slots.begin();i;++i)
			{
				if (!*i)
					m_slots.erase(*i);
				else
					i->invoke(p1);
			}
		}
	};

	template <typename P1, typename P2, typename Allocator = CrtAllocator>
	class Signal2
	{
	private:
		typedef Delegate2<void,P1,P2,Allocator> delegate_t;
		mutable Vector<delegate_t,Allocator> m_slots;

	public:
		Signal2()
		{}

		Signal2(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		bool connect(T* p, void (T::*slot)(P1,P2))
		{
			return m_slots.push_back(delegate_t(p,slot));
		}

		bool connect(void (*slot)(P1,P2))
		{
			return m_slots.push_back(delegate_t(slot));
		}

		template <typename T>
		bool disconnect(T* p, void (T::*slot)(P1,P2))
		{
			return m_slots.remove(delegate_t(p,slot));
		}

		void fire(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2) const
		{
			Vector<delegate_t,Allocator> slots(m_slots);
			for (typename Vector<delegate_t,Allocator>::iterator i=slots.begin();i;++i)
			{
				if (!*i)
					m_slots.erase(*i);
				else
					i->invoke(p1,p2);
			}
		}
	};

	template <typename P1, typename P2, typename P3, typename Allocator = CrtAllocator>
	class Signal3
	{
	private:
		typedef Delegate3<void,P1,P2,P3,Allocator> delegate_t;
		mutable Vector<delegate_t,Allocator> m_slots;

	public:
		Signal3()
		{}

		Signal3(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		bool connect(T* p, void (T::*slot)(P1,P2,P3))
		{
			return m_slots.push_back(delegate_t(p,slot));
		}

		bool connect(void (*slot)(P1,P2,P3))
		{
			return m_slots.push_back(delegate_t(slot));
		}

		template <typename T>
		bool disconnect(T* p, void (T::*slot)(P1,P2,P3))
		{
			return m_slots.remove(delegate_t(p,slot));
		}

		void fire(typename call_traits<P1>::param_type p1, typename call_traits<P2>::param_type p2, typename call_traits<P3>::param_type p3) const
		{
			Vector<delegate_t,Allocator> slots(m_slots);
			for (typename Vector<delegate_t,Allocator>::iterator i=slots.begin();i;++i)
			{
				if (!*i)
					m_slots.erase(*i);
				else
					i->invoke(p1,p2,p3);
			}
		}
	};
}

#endif // OOBASE_SIGNALSLOT_H_INCLUDED_
