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
		Delegate0(void (*fn)() = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate0(T* p, void (T::*fn)()) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
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
				m_ptr->thunk();
			else if (!m_static)
				return false;
			else
				(*m_static)();
			return true;
		}

		void swap(Delegate0& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
			OOBase::swap(m_static,rhs.m_static);
		}

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return (m_ptr.read(stream) && stream.read(m_static));
		}

		bool write(CDRStream& stream) const
		{
			return (m_ptr.write(stream) && stream.write(m_static));
		}
#endif

	private:
		struct ThunkBase
		{
			virtual void thunk() = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, void (T::*fn)()) : m_p(p), m_fn(fn)
			{}

			void thunk()
			{
				(m_p->*m_fn)();
			}

			T* m_p;
			void (T::*m_fn)();
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)();
	};

	template <typename P1, typename Allocator = CrtAllocator>
	class Delegate1
	{
	public:
		typedef Allocator allocator_type;
		typedef P1 param1_type;

		Delegate1(void (*fn)(P1) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate1(T* p, void (T::*fn)(P1)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
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
				m_ptr->thunk(p1);
			else if (!m_static)
				return false;
			else
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
			virtual void thunk(P1) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, void (T::*fn)(P1)) : m_p(p), m_fn(fn)
			{}

			void thunk(P1 p1)
			{
				(m_p->*m_fn)(p1);
			}

			T* m_p;
			void (T::*m_fn)(P1);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1);
	};

	template <typename P1, typename P2, typename Allocator = CrtAllocator>
	class Delegate2
	{
	public:
		typedef Allocator allocator_type;
		typedef P1 param1_type;
		typedef P2 param2_type;

		Delegate2(void (*fn)(P1,P2) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate2(T* p, void (T::*fn)(P1,P2)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
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
				m_ptr->thunk(p1,p2);
			else if (!m_static)
				return false;
			else
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
			virtual void thunk(P1, P2) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, void (T::*fn)(P1,P2)) : m_p(p), m_fn(fn)
			{}

			void thunk(P1 p1, P2 p2)
			{
				(m_p->*m_fn)(p1,p2);
			}

			T* m_p;
			void (T::*m_fn)(P1,P2);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1,P2);
	};

	template <typename P1, typename P2, typename P3, typename Allocator = CrtAllocator>
	class Delegate3
	{
	public:
		typedef Allocator allocator_type;
		typedef P1 param1_type;
		typedef P2 param2_type;
		typedef P3 param3_type;

		Delegate3(void (*fn)(P1,P2,P3) = NULL) : m_static(fn)
		{
		}

		template<typename T>
		Delegate3(T* p, void (T::*fn)(P1,P2,P3)) : m_static(NULL)
		{
			m_ptr = OOBase::allocate_shared<Thunk<T>,Allocator>(p,fn);
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
				m_ptr->thunk(p1,p2,p3);
			else if (!m_static)
				return false;
			else
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
			virtual void thunk(P1, P2, P3) = 0;
		};

		template<typename T>
		struct Thunk : public ThunkBase
		{
			Thunk(T* p, void (T::*fn)(P1,P2,P3)) : m_p(p), m_fn(fn)
			{}

			void thunk(P1 p1, P2 p2, P3 p3)
			{
				(m_p->*m_fn)(p1,p2,p3);
			}

			T* m_p;
			void (T::*m_fn)(P1,P2,P3);
		};

		SharedPtr<ThunkBase> m_ptr;
		void (*m_static)(P1,P2,P3);
	};
}

#endif // OOBASE_DELEGATE_H_INCLUDED_
