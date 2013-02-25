///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#ifndef OOBASE_STACK_ALLOCATOR_H_INCLUDED_
#define OOBASE_STACK_ALLOCATOR_H_INCLUDED_

#include "SmartPtr.h"

//#define OOBASE_STACK_ALLOC_CHECK 1

namespace OOBase
{
	class ScratchAllocator : public AllocatorInstance
	{
	public:
		ScratchAllocator(char* start, size_t len);
		virtual ~ScratchAllocator();

		void* allocate(size_t bytes, size_t align);
		void* reallocate(void* ptr, size_t bytes, size_t align);
		void free(void* ptr);

	protected:
		typedef unsigned short index_t;

		bool is_our_ptr(void* p) const;

	private:
		struct free_block_t
		{
			index_t m_size;
			index_t m_next;
			index_t m_prev;
		};

		static char* align_up(char* p, size_t align);
		static size_t correct_align(size_t align);
		static size_t min_alloc(size_t bytes);
		static index_t get_size(char* p);
		char* get_next(char* p);
		char* get_adjacent(char* p) const;
		index_t index_of(char* p) const;
		free_block_t* from_index(index_t idx);
		void split_block(char* p, char* split);
		static bool is_free(char* p);
		static char* get_tag(void* ptr);
		void alloc_tag(char* p);
		void free_tag(char* p);

		static const index_t s_in_use_mask = 1 << (sizeof(index_t)*8 - 1);
		static const index_t s_null_ptr = index_t(-1);

		char* m_start;
		char* m_end;
		char* m_free;

#if defined(OOBASE_STACK_ALLOC_CHECK) && OOBASE_STACK_ALLOC_CHECK
		char* get_prev(char* p);
		void check();
#endif
	};

	template <size_t SIZE, typename Allocator = ThreadLocalAllocator>
	class StackAllocator : public ScratchAllocator, public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;

	public:
		StackAllocator() : ScratchAllocator(m_buffer,sizeof(m_buffer)), baseClass()
		{
			static_assert(sizeof(m_buffer) < index_t(-1),"SIZE too big");
		}

		StackAllocator(AllocatorInstance& allocator) : ScratchAllocator(m_buffer,sizeof(m_buffer)), baseClass(allocator)
		{
			static_assert(sizeof(m_buffer) < index_t(-1),"SIZE too big");
		}

		void* allocate(size_t bytes, size_t align)
		{
			void* p = ScratchAllocator::allocate(bytes,align);
			if (p)
				return p;

			return baseClass::allocate(bytes,align);
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			if (!ptr)
				return allocate(bytes,align);

			if (!is_our_ptr(ptr))
				return baseClass::reallocate(ptr,bytes,align);

			return ScratchAllocator::reallocate(ptr,bytes,align);
		}

		void free(void* ptr)
		{
			if (!ptr)
				return;

			if (!is_our_ptr(ptr))
				baseClass::free(ptr);
			else
				ScratchAllocator::free(ptr);
		}

	private:
		char m_buffer[((SIZE / sizeof(index_t))+(SIZE % sizeof(index_t) ? 1 : 0)) * sizeof(index_t)];
	};

	template <typename T, size_t COUNT = 256/sizeof(T), typename Allocator = ThreadLocalAllocator>
	class StackArrayPtr : public NonCopyable
	{
	public:
		StackArrayPtr() : m_data(m_static), m_count(COUNT)
		{}

		StackArrayPtr(size_t count) : m_data(m_static), m_count(COUNT)
		{
			reallocate(count);
		}

		~StackArrayPtr()
		{}

		bool reallocate(size_t count)
		{
			if (count > COUNT)
			{
				m_data = m_dynamic = static_cast<T*>(Allocator::allocate(count * sizeof(T),alignment_of<T>::value));
				if (!m_data)
				{
					m_count = 0;
					return false;
				}

				m_count = count;
			}
			return true;
		}

		operator T* ()
		{
			return m_data;
		}

		operator const T* () const
		{
			return m_data;
		}

		size_t size() const
		{
			return m_count * sizeof(T);
		}

		size_t count() const
		{
			return m_count;
		}

	private:
		T           m_static[COUNT];
		T*          m_data;
		size_t      m_count;

		LocalPtr<T,FreeDestructor<Allocator> > m_dynamic;
	};
}

#endif // OOBASE_STACK_ALLOCATOR_H_INCLUDED_
