///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#ifndef OOBASE_MEMORY_H_INCLUDED_
#define OOBASE_MEMORY_H_INCLUDED_

#include "Base.h"

#include <malloc.h>
#include <string.h>
#include <new>

namespace OOBase
{
	template <typename T>
	class Ref
	{
	public:
		Ref(T& r) : m_r(r)
		{}

		Ref(const Ref& rhs) : m_r(rhs.m_r)
		{}

		Ref& operator = (const Ref& rhs)
		{
			if (this != &rhs)
				m_r = rhs.m_r;
			return *this;
		}

		operator T&() const
		{
			return m_r;
		}

	private:
		T& m_r;
	};


	template <typename Derived>
	class AllocateNewStatic
	{
	public:
		template <typename T>
		static void delete_free(T* p)
		{
			if (p)
			{
				p->~T();
				Derived::free(p);
			}
		}

		template <typename T>
		static bool allocate_new(T*& t)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4345)
#endif
					t = ::new (p) T();
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1>
		static bool allocate_new(T*& t, const P1& p1)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					t = ::new (p) T(p1);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2>
		static bool allocate_new(T*& t, const P1& p1, const P2& p2)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					t = ::new (p) T(p1,p2);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3>
		static bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4>
		static bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3,p4);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
		static bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3,p4,p5);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
		static bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
		{
			t = NULL;
			void* p = Derived::allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3,p4,p5,p6);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
			return (t != NULL);
		}

		static void swap(AllocateNewStatic&) {};
	};

	// Allocator types
	class CrtAllocator : public AllocateNewStatic<CrtAllocator>
	{
	public:
		static void* allocate(size_t bytes, size_t align = 16);
		static void* reallocate(void* ptr, size_t bytes, size_t align = 16);
		static void free(void* ptr);
	};

	template <typename Derived>
	class AllocateNew
	{
	public:
		template <typename T>
		void delete_free(T* p)
		{
			if (p)
			{
				p->~T();
				static_cast<Derived*>(this)->free(p);
			}
		}

		template <typename T>
		bool allocate_new(T*& t)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4345)
#endif
					t = ::new (p) T();
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1>
		bool allocate_new(T*& t, const P1& p1)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					t = ::new (p) T(p1);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2>
		bool allocate_new(T*& t, const P1& p1, const P2& p2)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					t = ::new (p) T(p1,p2);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3>
		bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4>
		bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t =::new (p) T(p1,p2,p3,p4);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
		bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t =::new (p) T(p1,p2,p3,p4,p5);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
		bool allocate_new(T*& t, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
		{
			t = NULL;
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignment_of<T>::value);
			if (p)
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					t = ::new (p) T(p1,p2,p3,p4,p5,p6);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
			return (t != NULL);
		}
	};

	class AllocatorInstance : public AllocateNew<AllocatorInstance>
	{
	public:
		virtual ~AllocatorInstance() {}

		virtual void* allocate(size_t bytes, size_t align) = 0;
		virtual void* reallocate(void* ptr, size_t bytes, size_t align) = 0;
		virtual void free(void* ptr) = 0;
	};

	class ThreadLocalAllocator : public AllocateNewStatic<ThreadLocalAllocator>
	{
	public:
		static void* allocate(size_t bytes, size_t align = 16);
		static void* reallocate(void* ptr, size_t bytes, size_t align = 16);
		static void free(void* ptr);

		static AllocatorInstance& instance();
	};

	template <typename Allocator = CrtAllocator>
	class Allocating : protected AllocateNewStatic<Allocating<Allocator> >
	{
		friend class AllocateNewStatic<Allocating<Allocator> >;

	protected:
		Allocating()
		{}

		Allocating(const Allocating& rhs)
		{}

		Allocating& operator = (const Allocating& rhs)
		{
			Allocating(rhs).swap(*this);
			return *this;
		}

		void swap(Allocating& rhs)
		{}

		static void* allocate(size_t bytes, size_t align)
		{
			return Allocator::allocate(bytes,align);
		}

		static void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			return Allocator::reallocate(ptr,bytes,align);
		}

		static void free(void* ptr)
		{
			Allocator::free(ptr);
		}

		template <typename T>
		static void delete_this(T* p)
		{
			p->~T();
			Allocator::free(p);
		}
	};

	template <>
	class Allocating<AllocatorInstance> : protected AllocateNew<Allocating<AllocatorInstance> >
	{
		friend class AllocateNew<Allocating<AllocatorInstance> >;

	public:
		AllocatorInstance& get_allocator() const
		{
			return m_allocator;
		}

	protected:
		Allocating(AllocatorInstance& allocator) : m_allocator(allocator)
		{}

		Allocating(const Allocating& rhs) : m_allocator(rhs.m_allocator)
		{}

		Allocating& operator = (const Allocating& rhs)
		{
			Allocating(rhs).swap(*this);
			return *this;
		}

		void swap(Allocating& rhs)
		{
			AllocatorInstance& t = m_allocator;
			m_allocator = rhs.m_allocator;
			rhs.m_allocator = t;
		}

		void* allocate(size_t bytes, size_t align)
		{
			return m_allocator.allocate(bytes,align);
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			return m_allocator.reallocate(ptr,bytes,align);
		}

		void free(void* ptr)
		{
			m_allocator.free(ptr);
		}

		template <typename T>
		static void delete_this(T* p)
		{
			AllocatorInstance& a = p->m_allocator;
			p->~T();
			a.free(p);
		}

		AllocatorInstance& m_allocator;
	};
}

#endif // OOBASE_MEMORY_H_INCLUDED_
