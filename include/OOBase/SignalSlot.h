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
	class Signal0 : public NonCopyable
	{
	private:
		class Slot
		{
		public:
			template<typename T>
			Slot(WeakPtr<T> ptr, void (T::*fn)()) :
					m_ptr(ptr), m_fn(fn), m_adaptor(&Slot::adaptor<T>)
			{
			}

			bool invoke() const
			{
				return (*m_adaptor)(this);
			}

		private:
			WeakPtr<void> m_ptr;
			void* m_fn;
			bool (*m_adaptor)(const Slot*);

			template<typename T>
			static bool adaptor(const Slot* pThis)
			{
				SharedPtr<T> ptr(pThis->m_ptr);
				if (!ptr)
					return false;

				ptr->*(pThis->m_fn)();
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
		int connect(WeakPtr<T>& ptr, void (T::*slot)())
		{
			return m_slots.push_back(Slot(ptr,slot));
		}

		void fire() const
		{
			for (typename Vector<Slot,Allocator>::iterator i=m_slots.begin();i!=m_slots.end();)
			{
				if (!i->invoke())
					i = m_slots.remove_at(i);
				else
					++i;
			}
		}
	};
}

#endif // OOBASE_SIGNALSLOT_H_INCLUDED_
