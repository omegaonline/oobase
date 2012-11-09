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

	template <size_t SIZE>
	class TempAllocator : public AllocatorInstance
	{
		// MAKE THIS GO BACKWARDS - STACK GROWS DOWN...

	public:
		TempAllocator() : m_free(m_data)
		{
		}

		void* allocate(size_t bytes, size_t align)
		{
			if (!bytes)
				return NULL;

			// Round all allocations to 'align' bytes
			if (align > 1)
			{
				size_t overrun = (reinterpret_cast<size_t>(m_free) & (align-1));
				if (overrun)
					m_free += align - overrun;
			}

			if (m_free + bytes > m_data + SIZE)
				return CrtAllocator::allocate(bytes);

			void* ptr = m_free;
			m_free += bytes;
			return ptr;
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			if (ptr < m_data || ptr >= m_data + SIZE)
				return CrtAllocator::reallocate(ptr,bytes);

			void* ptr_new = allocate(bytes,align);
			if (ptr_new && ptr)
				memcpy(ptr_new,ptr,bytes);

			free(ptr);

			return ptr_new;
		}

		void free(void* ptr)
		{
			if (ptr && (ptr < m_data || ptr >= m_data + SIZE))
				CrtAllocator::free(ptr);
		}

	private:
		char  m_data[SIZE];
		char* m_free;
	};

	template <size_t SIZE>
	class TempAllocator2 : public AllocatorInstance
	{
	public:
		TempAllocator2() : m_alloc(m_data), m_end(m_data + m_size)
		{
			static_assert(m_size <= (1 << (sizeof(index_t) - 1)),"Invalid SIZE");

			m_alloc->m_in_use = 0;
			m_alloc->m_size = m_size;
		}

		void* allocate(size_t bytes, size_t align)
		{
			if (!bytes)
				return NULL;

			// Round all allocations to 'align' bytes
			if (align < sizeof(index_t))
				align = sizeof(index_t);

			size_t alloc_blocks = (bytes / sizeof(index_t)) + 1;
			if (bytes & (align-1))
				++alloc_blocks;

			index_t* ptr = m_alloc;
			while (ptr)
			{
				if (!ptr->m_in_use && ptr->m_size >= alloc_blocks)
					break;

				ptr += ptr->m_size;
				if (ptr >= m_end)
					ptr = m_data;

				if (ptr == m_alloc)
					ptr = NULL;
			}

			if (!ptr)
				return CrtAllocator::allocate(bytes);

			ptr->m_in_use = 1;

			index_t* next = ptr + alloc_blocks;
			if (next < m_end)
			{
				next->m_size = ptr->m_size - alloc_blocks;
				next->m_in_use = 0;
			}
			else
			{
				next = m_data;
				while (next < m_end && next->m_in_use)
					next += next->m_size;

				if (next >= m_end)
					next = NULL;
			}

			ptr->m_size = alloc_blocks;

			m_alloc = next;

			return ptr + 1;
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			if (ptr < m_data + 1 || ptr >= m_end)
				return CrtAllocator::reallocate(ptr,bytes);

			void* ptr_new = allocate(bytes,align);
			if (ptr_new && ptr)
				memcpy(ptr_new,ptr,bytes);

			free(ptr);

			return ptr_new;
		}

		void free(void* ptr)
		{
			if (ptr)
			{
				if (ptr < m_data + 1 || ptr >= m_end)
					return CrtAllocator::free(ptr);

				if (reinterpret_cast<size_t>(ptr) & (sizeof(index_t)-1))
					OOBase_CallCriticalFailure("Invalid pointer to reallocate");

				// Check the tag block
				index_t* tag = static_cast<index_t*>(ptr) - 1;
				if (!tag->m_in_use)
					OOBase_CallCriticalFailure("Double free");

				// Coalesce adjacent free blocks
				index_t* next = tag + tag->m_size;
				while (next < m_end && next < m_alloc && !next->m_in_use)
				{
					tag->m_size += next->m_size;
					next += next->m_size;
				}

				// Reset the pointers if we free up to m_alloc
				if (next == m_alloc)
				{
					tag->m_size += next->m_size;
					m_alloc = tag;
				}

				// Clear the in-use flag
				tag->m_in_use = 0;

				if (!m_alloc)
					m_alloc = tag;
			}
		}

	private:
		struct index_t
		{
			unsigned m_in_use : 1;
			unsigned m_size : 15;
		};

		static const size_t m_size = ((SIZE / sizeof(index_t)) + (SIZE % sizeof(index_t) > 0 ? 1 : 0));

		index_t  m_data[m_size];
		index_t* m_alloc;
		const index_t* m_end;
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
