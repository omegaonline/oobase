///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015 Rick Taylor
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

#ifndef OOBASE_DELEGATE_H_INCLUDED_
#define OOBASE_DELEGATE_H_INCLUDED_

#include "SharedPtr.h"

namespace OOBase
{
	template <typename Allocator = CrtAllocator>
	class Delegate0
	{
	public:
		template<typename T>
		Delegate0(const WeakPtr<T>& ptr, void (T::*fn)()) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(ptr,fn);
		}

		Delegate0(void (*fn)()) : m_static(fn)
		{
		}

		bool operator == (const Delegate0& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		bool invoke() const
		{
			if (m_ptr)
				return m_ptr->thunk();

			(*m_static)();
			return true;
		}

		void swap(Delegate0& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

	private:
		struct ThunkBase
		{
			virtual bool thunk() = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(const WeakPtr<T>& ptr, void (T::*fn)()) : m_ptr(ptr), m_fn(fn)
			{}

			bool thunk()
			{
				if (!m_ptr)
					return false;

				(m_ptr.lock().get()->*m_fn)();
				return true;
			}

			WeakPtr<T> m_ptr;
			void (T::*m_fn)();
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)();
	};

	template <typename P1, typename Allocator = CrtAllocator>
	class Delegate1
	{
	public:
		template<typename T>
		Delegate1(const WeakPtr<T>& ptr, void (T::*fn)(P1)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(ptr,fn);
		}

		Delegate1(void (*fn)(P1)) : m_static(fn)
		{
		}

		bool operator == (const Delegate1& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		bool invoke(P1 p1) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1);

			(*m_static)(p1);
			return true;
		}

		void swap(Delegate1& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

	private:
		struct ThunkBase
		{
			virtual bool thunk(P1) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(const WeakPtr<T>& ptr, void (T::*fn)(P1)) : m_ptr(ptr), m_fn(fn)
			{}

			bool thunk(P1 p1)
			{
				if (!m_ptr)
					return false;

				(m_ptr.lock().get()->*m_fn)(p1);
				return true;
			}

			WeakPtr<T> m_ptr;
			void (T::*m_fn)(P1);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1);
	};

	template <typename P1, typename P2, typename Allocator = CrtAllocator>
	class Delegate2
	{
	public:
		template<typename T>
		Delegate2(const WeakPtr<T>& ptr, void (T::*fn)(P1,P2)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(ptr,fn);
		}

		Delegate2(void (*fn)(P1,P2)) : m_static(fn)
		{
		}

		bool operator == (const Delegate2& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		bool invoke(P1 p1, P2 p2) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2);

			(*m_static)(p1,p2);
			return true;
		}

		void swap(Delegate2& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

	private:
		struct ThunkBase
		{
			virtual bool thunk(P1, P2) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(const WeakPtr<T>& ptr, void (T::*fn)(P1,P2)) : m_ptr(ptr), m_fn(fn)
			{}

			bool thunk(P1 p1, P2 p2)
			{
				if (!m_ptr)
					return false;

				(m_ptr.lock().get()->*m_fn)(p1,p2);
				return true;
			}

			WeakPtr<T> m_ptr;
			void (T::*m_fn)(P1,P2);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1,P2);
	};

	template <typename P1, typename P2, typename P3, typename Allocator = CrtAllocator>
	class Delegate3
	{
	public:
		template<typename T>
		Delegate3(const WeakPtr<T>& ptr, void (T::*fn)(P1,P2,P3)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(ptr,fn);
		}

		Delegate3(void (*fn)(P1,P2,P3)) : m_static(fn)
		{
		}

		bool operator == (const Delegate3& rhs) const
		{
			if (this == &rhs)
				return true;
			return m_ptr == rhs.m_ptr && m_static == rhs.m_static;
		}

		bool invoke(P1 p1, P2 p2, P3 p3) const
		{
			if (m_ptr)
				return m_ptr->thunk(p1,p2,p3);

			(*m_static)(p1,p2,p3);
			return true;
		}

		void swap(Delegate3& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

	private:
		struct ThunkBase
		{
			virtual bool thunk(P1, P2, P3) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(const WeakPtr<T>& ptr, void (T::*fn)(P1,P2,P3)) : m_ptr(ptr), m_fn(fn)
			{}

			bool thunk(P1 p1, P2 p2, P3 p3)
			{
				if (!m_ptr)
					return false;

				(m_ptr.lock().get()->*m_fn)(p1,p2,p3);
				return true;
			}

			WeakPtr<T> m_ptr;
			void (T::*m_fn)(P1,P2,P3);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1,P2,P3);
	};
}

#endif // OOBASE_DELEGATE_H_INCLUDED_
