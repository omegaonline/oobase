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

#include "Memory.h"

namespace OOBase
{
	template <size_t SIZE, typename Allocator = ThreadLocalAllocator>
	class StackAllocator : public AllocatorInstance
	{
	public:
		StackAllocator() : m_free(align_up(m_start,alignof<index_t>::value))
		{
			static_assert(sizeof(m_start) < s_null_ptr,"SIZE too big");

			reinterpret_cast<free_block_t*>(m_free)->m_size = sizeof(m_start) / sizeof(index_t);
			reinterpret_cast<free_block_t*>(m_free)->m_next = s_null_ptr;
			reinterpret_cast<free_block_t*>(m_free)->m_prev = s_null_ptr;
		}

		void* allocate(size_t bytes, size_t align)
		{
			if (!bytes)
				return NULL;

			// Find the extent of the adjacent allocation
			char* free_start = m_free;
			char* free_end = NULL;
			char* alloc_start = NULL;
			char* alloc_end = NULL;

			while (free_start)
			{
				alloc_start = align_up(free_start + sizeof(index_t),correct_align(align)) - sizeof(index_t);

				// Ensure we have space for a pre block if needed
				if (alloc_start != free_start && static_cast<size_t>(alloc_start - free_start) < sizeof(free_block_t))
					alloc_start = align_up(free_start + sizeof(free_block_t) + sizeof(index_t),correct_align(align)) - sizeof(index_t);

				alloc_end = align_up(alloc_start + min_alloc(bytes),alignof<index_t>::value);

				free_end = free_start + get_size(free_start);
				if (alloc_end <= free_end)
					break;

				free_start = get_next(free_start);
			}

			// If we have no space, use the Allocator
			if (!free_start)
				return Allocator::allocate(bytes,align);

			if (alloc_end <= free_end - sizeof(free_block_t))
				split_block(free_start,alloc_end);

			if (alloc_start != free_start)
				split_block(free_start,alloc_start);

			alloc_tag(alloc_start);
			return alloc_start + sizeof(index_t);
		}

		void* reallocate(void* ptr, size_t bytes, size_t align)
		{
			if (!ptr)
				return allocate(bytes,align);

			if (ptr < m_start + sizeof(index_t) || ptr >= m_start + sizeof(m_start))
				return Allocator::reallocate(ptr,bytes);

			char* tag = get_tag(ptr);
			if (get_size(tag) >= bytes)
			{
				// We don't shrink blocks
				return ptr;
			}

			// Check the adjacent block
			char* adjacent = get_adjacent(tag);
			if (adjacent && is_free(adjacent))
			{
				char* alloc_end = align_up(tag + min_alloc(bytes),alignof<index_t>::value);

				if (static_cast<size_t>(alloc_end - adjacent) < sizeof(free_block_t))
					alloc_end = align_up(alloc_end + (sizeof(free_block_t) - static_cast<size_t>(alloc_end - adjacent)),alignof<index_t>::value);

				char* free_end = adjacent + get_size(adjacent);

				if (alloc_end <= free_end)
				{
					// Merge with adjacent
					if (alloc_end <= free_end - sizeof(free_block_t))
						split_block(adjacent,alloc_end);

					reinterpret_cast<free_block_t*>(tag)->m_size += reinterpret_cast<free_block_t*>(adjacent)->m_size;
					alloc_tag(adjacent);

					return ptr;
				}
			}

			// Just do an allocate, copy and free
			void* ptr_new = allocate(bytes,align);
			if (ptr_new)
			{
				memcpy(ptr_new,ptr,get_size(tag));
				free_tag(tag);
			}
			return ptr_new;
		}

		void free(void* ptr)
		{
			if (!ptr)
				return;

			if (ptr < m_start + sizeof(index_t) || ptr >= m_start + sizeof(m_start))
				Allocator::free(ptr);
			else
				free_tag(get_tag(ptr));
		}

	private:
		typedef unsigned short index_t;

		struct free_block_t
		{
			index_t m_size;
			index_t m_next;
			index_t m_prev;
		};

		static char* align_up(char* p, size_t align)
		{
			size_t o = (reinterpret_cast<size_t>(p) % align);
			if (o)
				p += align - o;
			return p;
		}

		static size_t correct_align(size_t align)
		{
			return (align < alignof<index_t>::value ? alignof<index_t>::value : align);
		}

		static size_t min_alloc(size_t bytes)
		{
			return (bytes + sizeof(index_t) < sizeof(free_block_t) ? sizeof(free_block_t) : bytes + sizeof(index_t));
		}

		static index_t get_size(char* p)
		{
			return (*reinterpret_cast<index_t*>(p) & ~s_in_use_mask) * sizeof(index_t);
		}

		char* get_next(char* p)
		{
			return (reinterpret_cast<free_block_t*>(p)->m_next == s_null_ptr ? NULL : m_start + (reinterpret_cast<free_block_t*>(p)->m_next) * sizeof(index_t));
		}

		char* get_adjacent(char* p) const
		{
			char* n = p + get_size(p);
			return (n < m_start + sizeof(m_start) ? n : NULL);
		}

		index_t index_of(char* p) const
		{
			return static_cast<index_t>((p - m_start) / sizeof(index_t));
		}

		free_block_t* from_index(index_t idx)
		{
			return reinterpret_cast<free_block_t*>(m_start + (idx * sizeof(index_t)));
		}

		void split_block(char* p, char* split)
		{
			free_block_t* orig_block = reinterpret_cast<free_block_t*>(p);
			free_block_t* new_block = reinterpret_cast<free_block_t*>(split);

			new_block->m_size = orig_block->m_size - (static_cast<index_t>(split - p) / sizeof(index_t));
			new_block->m_next = index_of(p);
			new_block->m_prev = orig_block->m_prev;

			orig_block->m_size = (static_cast<index_t>(split - p) / sizeof(index_t));

			if (orig_block->m_prev != s_null_ptr)
				from_index(orig_block->m_prev)->m_next = index_of(split);

			orig_block->m_prev = index_of(split);

			if (m_free == p)
				m_free = split;
		}

		static bool is_free(char* p)
		{
			return *reinterpret_cast<index_t*>(p) & s_in_use_mask ? false : true;
		}

		static char* get_tag(void* ptr)
		{
			// Check alignment...
			if (reinterpret_cast<ptrdiff_t>(ptr) & (alignof<index_t>::value - 1))
				OOBase_CallCriticalFailure("Invalid pointer to reallocate");

			// Check the tag block
			char* tag = static_cast<char*>(ptr) - sizeof(index_t);
			if (is_free(tag))
				OOBase_CallCriticalFailure("Double free");

			return tag;
		}

		void alloc_tag(char* p)
		{
			free_block_t* alloc = reinterpret_cast<free_block_t*>(p);

			if (alloc->m_prev != s_null_ptr)
				from_index(alloc->m_prev)->m_next = alloc->m_next;

			if (alloc->m_next != s_null_ptr)
				from_index(alloc->m_next)->m_prev = alloc->m_prev;

			if (m_free == p)
			{
				if (alloc->m_prev != s_null_ptr)
					m_free = m_start + (alloc->m_prev * sizeof(index_t));
				else
					m_free = get_next(p);
			}

			alloc->m_size |= s_in_use_mask;
		}

		void free_tag(char* p)
		{
			free_block_t* block = reinterpret_cast<free_block_t*>(p);
			block->m_size &= ~s_in_use_mask;

			char* adjacent = get_adjacent(p);
			if (adjacent && is_free(adjacent))
			{
				// Merge with adjacent
				free_block_t* adjacent_block = reinterpret_cast<free_block_t*>(adjacent);
				block->m_size += adjacent_block->m_size;
				block->m_next = adjacent_block->m_next;
				block->m_prev = adjacent_block->m_prev;

				if (block->m_prev != s_null_ptr)
					from_index(block->m_prev)->m_next = index_of(p);

				if (block->m_next != s_null_ptr)
					from_index(block->m_next)->m_prev = index_of(p);

				if (m_free == adjacent)
					m_free = p;
			}
			else
			{
				// Insert into free list maintaining order
				char* insert = NULL;
				for (char* f = m_free;f && f > p;f = get_next(f))
					insert = f;

				if (insert)
				{
					block->m_prev = index_of(insert);
					block->m_next = reinterpret_cast<free_block_t*>(insert)->m_next;
					reinterpret_cast<free_block_t*>(insert)->m_next = index_of(p);

					if (block->m_next != s_null_ptr)
						from_index(block->m_next)->m_prev = index_of(p);
				}
				else
				{
					if (m_free)
					{
						block->m_next = index_of(m_free);
						reinterpret_cast<free_block_t*>(m_free)->m_prev = index_of(p);
					}
					else
						block->m_next = s_null_ptr;

					block->m_prev = s_null_ptr;
					m_free = p;
				}
			}

			// Merge with next if adjacent to p
			char* next = get_next(p);
			if (next && get_adjacent(next) == p)
			{
				free_block_t* next_block = reinterpret_cast<free_block_t*>(next);
				if (next_block->m_prev == index_of(p))
				{
					next_block->m_size += block->m_size;
					next_block->m_prev = block->m_prev;

					if (next_block->m_prev != s_null_ptr)
						from_index(next_block->m_prev)->m_next = index_of(next);

					if (m_free == p)
						m_free = next;

				}
			}
		}

		static const index_t s_in_use_mask = 1 << (sizeof(index_t)*8 - 1);
		static const index_t s_null_ptr = index_t(-1);

		char  m_start[((SIZE / sizeof(index_t))+(SIZE % sizeof(index_t) ? 1 : 0)) * sizeof(index_t)];
		char* m_free;
	};

	template <typename T, size_t COUNT = 256/sizeof(T), typename Allocator = ThreadLocalAllocator>
	class StackArrayPtr
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
				m_data = m_dynamic = static_cast<T*>(Allocator::allocate(count * sizeof(T),alignof<T>::value));
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
		// No copying or assignment - these are stack allocated
		StackArrayPtr(const StackArrayPtr&);
		StackArrayPtr& operator = (const StackArrayPtr&);

		T           m_static[COUNT];
		T*          m_data;
		size_t      m_count;

		LocalPtr<T,FreeDestructor<Allocator> > m_dynamic;
	};
}

#endif // OOBASE_STACK_ALLOCATOR_H_INCLUDED_
