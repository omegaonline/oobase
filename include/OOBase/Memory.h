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

#include "../config-base.h"

#include <malloc.h>
#include <string.h>
#include <new>

#if defined(_MSC_VER)
#define HAVE__IS_POD 1
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof(X)
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof__(X)
#endif

#if defined(__clang__)
#if __has_extension(is_pod)
#define HAVE__IS_POD 1
#endif
#elif defined(__GNUG__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HAVE__IS_POD 1
#endif

#if !defined(HAVE__IS_POD)
#include <limits>
#endif

namespace OOBase
{
	struct critical_t { int unused; };
	extern const critical_t critical;

	template <typename T>
	struct alignof
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
			static const bool value = __is_pod(T);
#else
			static const bool value = std::numeric_limits<T>::is_integer || std::numeric_limits<T>::is_signed;
#endif
		};

#if !defined(HAVE__IS_POD)
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
			static const bool value = is_pod<T>::value;
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
#endif
	}

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
		static T* allocate_new()
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					return ::new (p) T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1>
		static T* allocate_new(P1 p1)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					return ::new (p) T(p1);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2>
		static T* allocate_new(P1 p1, P2 p2)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					return ::new (p) T(p1,p2);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3>
		static T* allocate_new(P1& p1, P2& p2, P3& p3)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4>
		static T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
		static T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4,p5);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
		static T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6)
		{
			void* p = Derived::allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4,p5,p6);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { Derived::free(p); throw; }
#endif
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

	class ThreadLocalAllocator : public AllocateNewStatic<ThreadLocalAllocator>
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
		T* allocate_new()
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					return ::new (p) T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1>
		T* allocate_new(P1& p1)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try {
#endif
					return ::new (p) T(p1);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2>
		T* allocate_new(P1& p1, P2& p2)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					return ::new (p) T(p1,p2);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3>
		T* allocate_new(P1& p1, P2& p2, P3& p3)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4>
		T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5>
		T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4,p5);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
		}

		template <typename T, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
		T* allocate_new(P1& p1, P2& p2, P3& p3, P4& p4, P5& p5, P6& p6)
		{
			void* p = static_cast<Derived*>(this)->allocate(sizeof(T),alignof<T>::value);
			if (!p)
				return NULL;
			else
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try	{
#endif
					return ::new (p) T(p1,p2,p3,p4,p5,p6);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				} catch (...) { static_cast<Derived*>(this)->free(p); throw; }
#endif
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

	template <typename Allocator = CrtAllocator>
	class Allocating : protected AllocateNewStatic<Allocating<Allocator> >
	{
	public:
		Allocating()
		{}

	protected:
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

		AllocatorInstance& get_allocator() const
		{
			return m_allocator;
		}

	protected:
		AllocatorInstance& m_allocator;

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
	};
}

// Call the OOBase::OnCriticalError handler on failure
inline void* operator new(size_t bytes, const OOBase::critical_t&)
{
	void* ptr = ::operator new(bytes,std::nothrow);
	if (!ptr)
		OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
	return ptr;
}

inline void* operator new[](size_t bytes, const OOBase::critical_t&)
{
	void* ptr = ::operator new [] (bytes,std::nothrow);
	if (!ptr)
		OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
	return ptr;
}

inline void operator delete(void* ptr, const OOBase::critical_t&)
{
	::operator delete(ptr);
}

inline void operator delete[](void* ptr, const OOBase::critical_t&)
{
	::operator delete[](ptr);
}

#endif // OOBASE_MEMORY_H_INCLUDED_
