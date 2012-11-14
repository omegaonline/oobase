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
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof(X)
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#define HAVE__ALIGNOF 1
#define ALIGNOF(X) __alignof__(X)
#endif

namespace OOBase
{
	struct critical_t { int unused; };
	extern const critical_t critical;

	namespace detail
	{
		template <typename T>
		struct alignof
		{
#if defined(HAVE__ALIGNOF)
			static const size_t value = ALIGNOF(T);
#else
			static const size_t value = sizeof(T) > 16 ? 16 : sizeof(T);
#endif
		};
	}

	// Allocator types
	class CrtAllocator
	{
	public:
		static void* allocate(size_t bytes, size_t align = 1);
		static void* reallocate(void* ptr, size_t bytes, size_t align = 1);
		static void free(void* ptr);
	};

	class LocalAllocator
	{
	public:
		static void* allocate(size_t bytes, size_t align = 1);
		static void* reallocate(void* ptr, size_t bytes, size_t align = 1);
		static void free(void* ptr);
	};

	class ThreadLocalAllocator
	{
	public:
		static void* allocate(size_t bytes, size_t align = 1);
		static void* reallocate(void* ptr, size_t bytes, size_t align = 1);
		static void free(void* ptr);
	};

	class AllocatorInstance
	{
	public:
		virtual ~AllocatorInstance() {}

		virtual void* allocate(size_t bytes, size_t align) = 0;
		virtual void* reallocate(void* ptr, size_t bytes, size_t align) = 0;
		virtual void free(void* ptr) = 0;

	};

	template <typename Allocator>
	class CustomNew
	{
	public:
		void* operator new(size_t bytes)
		{
			assert(false);
			return NULL;
		}

		void* operator new[](size_t bytes)
		{
			assert(false);
			return NULL;
		}

		void operator delete(void* ptr)
		{
			Allocator::free(ptr);
		}

		void operator delete[](void* ptr)
		{
			Allocator::free(ptr);
		}

		void* operator new(size_t bytes, const std::nothrow_t&)
		{
			return Allocator::allocate(bytes);
		}

		void* operator new[](size_t bytes, const std::nothrow_t&)
		{
			return Allocator::allocate(bytes);
		}

		void operator delete(void* ptr, const std::nothrow_t&)
		{
			Allocator::free(ptr);
		}

		void operator delete[](void* ptr, const std::nothrow_t&)
		{
			Allocator::free(ptr);
		}

		void* operator new(size_t bytes, const OOBase::critical_t&)
		{
			void* ptr = Allocator::allocate(bytes);
			if (!ptr)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
			return ptr;
		}

		void* operator new[](size_t bytes, const OOBase::critical_t&)
		{
			void* ptr = Allocator::allocate(bytes);
			if (!ptr)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
			return ptr;
		}

		void operator delete(void* ptr, const OOBase::critical_t&)
		{
			Allocator::free(ptr);
		}

		void operator delete[](void* ptr, const OOBase::critical_t&)
		{
			Allocator::free(ptr);
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
