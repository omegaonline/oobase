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

#include "SharedPtr.h"
#include "Vector.h"

namespace OOBase
{
	template <typename Allocator = CrtAllocator>
	class Signal0
	{
	private:
		class Slot
		{
		public:
			template<typename T>
			Slot(const WeakPtr<T>& ptr, void (T::*fn)()) :	m_ptr(ptr)
			{
				assert(!!ptr);
				d.m_adaptor = &Slot::adaptor<T,fn>;
			}

			Slot(void (*fn)()) : m_ptr()
			{
				d.m_static = fn;
			}

			bool operator == (const Slot& rhs) const
			{
				if (this == &rhs)
					return true;
				return m_ptr == rhs.m_ptr && d.m_adaptor == rhs.d.m_adaptor;
			}

			bool invoke() const
			{
				if (m_ptr)
					return (*d.m_adaptor)(this);

				(*d.m_static)();
				return true;
			}

			void swap(Slot& rhs)
			{
				m_ptr.swap(rhs.m_ptr);
				OOBase::swap(d,rhs.d);
			}

		private:
			WeakPtr<void> m_ptr;
			union disc_t
			{
				bool (*m_adaptor)(const Slot*);
				void (*m_static)();
			} d;

			template<typename T, void (T::*fn)()>
			static bool adaptor(const Slot* pThis)
			{
				SharedPtr<T> ptr(pThis->m_ptr);
				if (!ptr)
					return false;

				ptr->*(pThis->*fn)();
				return true;
			}
		};
		mutable Vector<Slot,Allocator> m_slots;

	public:
		Signal0()
		{}

		Signal0(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		int connect(const WeakPtr<T>& ptr, void (T::*slot)())
		{
			return m_slots.push_back(Slot(ptr,slot));
		}

		int connect(void (*slot)())
		{
			return m_slots.push_back(Slot(slot));
		}

		template <typename T>
		bool disconnect(WeakPtr<T>& ptr, void (T::*slot)())
		{
			return m_slots.remove(Slot(ptr,slot));
		}

		void fire() const
		{
			Vector<Slot,Allocator> slots(m_slots);
			for (typename Vector<Slot,Allocator>::iterator i=slots.begin();i!=slots.end();++i)
			{
				if (!i->invoke())
					m_slots.remove(*i);
			}
		}
	};
}

#endif // OOBASE_SIGNALSLOT_H_INCLUDED_
