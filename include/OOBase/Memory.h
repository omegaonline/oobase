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

#if defined(_MSC_VER)
#define HAVE__IS_POD 1
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof(X)
#if (_MSC_VER == 1400)
#include <type_traits>
#define IS_POD(X) (std::tr1::is_pod<X>::value)
#elif (_MSC_VER > 1400)
#include <type_traits>
#define IS_POD(X) (std::is_pod<X>::value)
#else
#include <limits>
#define HAVE__IS_POD 1
#define IS_POD(X) (std::numeric_limits<X>::is_specialized || __is_enum(X) || (__has_trivial_constructor(X) && __is_pod(X)))
#endif
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof__(X)
#endif

#if defined(__clang__)
#if __has_extension(is_pod)
#define HAVE__IS_POD 1
#define IS_POD(X) __is_pod(X)
#endif
#elif defined(__GNUG__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HAVE__IS_POD 1
#define IS_POD(X) __is_pod(X)
#endif

#if !defined(HAVE__IS_POD)
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
#include <limits>
#endif

namespace OOBase
{
	template <typename T>
	struct alignment_of
	{
#if defined(HAVE__ALIGNOF)
		static const size_t value = ALIGNOF(T);
#else
		static const size_t value = sizeof(T) > 16 ? 16 : sizeof(T);
#endif
	};

	namespace detail
	{
		template <typename T>
		struct is_pod
		{
#if defined(HAVE__IS_POD)
			static const bool value = IS_POD(T);
#else
			static const bool value = std::numeric_limits<T>::is_specialized;
#endif
		};

		template <>
		struct is_pod<void>
		{
			static const bool value = true;
		};

		template <typename T>
		struct is_pod<T&>
		{
			static const bool value = false;
		};

		template <typename T>
		struct is_pod<T*>
		{
			static const bool value = true;
		};

		template <typename T>
		struct is_pod<T const>
		{
			static const bool value = is_pod<T>::value;
		};

		template <typename T>
		struct is_pod<T volatile>
		{
			static const bool value = is_pod<T>::value;
		};
	}

	struct NonCopyable
	{
	protected:
		NonCopyable() {}

	private:
		NonCopyable(const NonCopyable&);
		NonCopyable& operator = (const NonCopyable&);
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
		static bool allocate_new(T*& t, P1& p1)
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
		static bool allocate_new(T*& t, P1& p1, P2& p2)
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
		static bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3)
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
		static bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4)
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
		static bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4, P5& p5)
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
		static bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6)
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
		bool allocate_new(T*& t, P1& p1)
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
		bool allocate_new(T*& t, P1& p1, P2& p2)
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
		bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3)
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
		bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4)
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
		bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4, P5& p5)
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
		bool allocate_new(T*& t, P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6)
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
	public:
		Allocating()
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
	public:
		Allocating(AllocatorInstance& allocator) : m_allocator(allocator)
		{}

		Allocating(const Allocating& rhs) : m_allocator(rhs.m_allocator)
		{}

		Allocating& operator = (const Allocating& rhs)
		{
			if (&rhs != this)
				 m_allocator = rhs.m_allocator;
			return *this;
		}

		AllocatorInstance& get_allocator() const
		{
			return m_allocator;
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

	protected:
		AllocatorInstance& m_allocator;
	};
}

#endif // OOBASE_MEMORY_H_INCLUDED_
