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
					m_slots.erase(*i);
			}
		}
	};

	template <typename P1, typename Allocator = CrtAllocator>
	class Signal1
	{
	private:
		class Slot
		{
		public:
			template<typename T>
			Slot(const WeakPtr<T>& ptr, void (T::*fn)(const P1& p1)) :	m_ptr(ptr)
			{
				assert(!!ptr);
				d.m_adaptor = &Slot::adaptor<T,fn>;
			}

			Slot(void (*fn)(const P1& p1)) : m_ptr()
			{
				d.m_static = fn;
			}

			bool operator == (const Slot& rhs) const
			{
				if (this == &rhs)
					return true;
				return m_ptr == rhs.m_ptr && d.m_adaptor == rhs.d.m_adaptor;
			}

			bool invoke(const P1& p1) const
			{
				if (m_ptr)
					return (*d.m_adaptor)(this,p1);

				(*d.m_static)(p1);
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
				bool (*m_adaptor)(const Slot*, const P1& p1);
				void (*m_static)(const P1& p1);
			} d;

			template<typename T, void (T::*fn)(const P1& p1)>
			static bool adaptor(const Slot* pThis, const P1& p1)
			{
				SharedPtr<T> ptr(pThis->m_ptr);
				if (!ptr)
					return false;

				ptr->*(pThis->*fn)(p1);
				return true;
			}
		};
		mutable Vector<Slot,Allocator> m_slots;

	public:
		Signal1()
		{}

		Signal1(AllocatorInstance& a) : m_slots(a)
		{}

		template <typename T>
		int connect(const WeakPtr<T>& ptr, void (T::*slot)(const P1& p1))
		{
			return m_slots.push_back(Slot(ptr,slot));
		}

		int connect(void (*slot)(const P1& p1))
		{
			return m_slots.push_back(Slot(slot));
		}

		template <typename T>
		bool disconnect(WeakPtr<T>& ptr, void (T::*slot)(const P1& p1))
		{
			return m_slots.remove(Slot(ptr,slot));
		}

		void fire(const P1& p1) const
		{
			Vector<Slot,Allocator> slots(m_slots);
			for (typename Vector<Slot,Allocator>::iterator i=slots.begin();i!=slots.end();++i)
			{
				if (!i->invoke(p1))
					m_slots.erase(*i);
			}
		}
	};

	template <typename P1, typename P2, typename Allocator = CrtAllocator>
	class Signal2
	{
	private:
		class Slot
		{
		public:
			template<typename T>
			Slot(const WeakPtr<T>& ptr, void (T::*fn)(const P1& p1, const P2& p2)) :	m_ptr(ptr)
			{
				assert(!!ptr);
				d.m_adaptor = &Slot::adaptor<T,fn>;
			}

			Slot(void (*fn)(const P1& p1, const P2& p2)) : m_ptr()
			{
				d.m_static = fn;
			}

			bool operator == (const Slot& rhs) const
			{
				if (this == &rhs)
					return true;
				return m_ptr == rhs.m_ptr && d.m_adaptor == rhs.d.m_adaptor;
			}

			bool invoke(const P1& p1, const P2& p2) const
			{
				if (m_ptr)
					return (*d.m_adaptor)(this,p1,p2);

				(*d.m_static)(p1,p2);
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
				bool (*m_adaptor)(const Slot*, const P1& p1, const P2& p2);
				void (*m_static)(const P1& p1, const P2& p2);
			} d;

			template<typename T, void (T::*fn)(const P1& p1, const P2& p2)>
			static bool adaptor(const Slot* pThis, const P1& p1, const P2& p2)
			{
					SharedPtr<T> ptr(pThis->m_ptr);
					if (!ptr)
						return false;

					ptr->*(pThis->*fn)(p1,p2);
					return true;
				}
			};
			mutable Vector<Slot,Allocator> m_slots;

		public:
			Signal2()
			{}

			Signal2(AllocatorInstance& a) : m_slots(a)
			{}

			template <typename T>
			int connect(const WeakPtr<T>& ptr, void (T::*slot)(const P1& p1,const P2& p2))
			{
				return m_slots.push_back(Slot(ptr,slot));
			}

			int connect(void (*slot)(const P1& p1, const P2& p2))
			{
				return m_slots.push_back(Slot(slot));
			}

			template <typename T>
			bool disconnect(WeakPtr<T>& ptr, void (T::*slot)(const P1& p1,const P2& p2))
			{
				return m_slots.remove(Slot(ptr,slot));
			}

			void fire(const P1& p1, const P2& p2) const
			{
				Vector<Slot,Allocator> slots(m_slots);
				for (typename Vector<Slot,Allocator>::iterator i=slots.begin();i!=slots.end();++i)
				{
					if (!i->invoke(p1,p2))
						m_slots.erase(*i);
				}
			}
		};
}

#endif // OOBASE_SIGNALSLOT_H_INCLUDED_
