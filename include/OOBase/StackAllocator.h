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

#ifndef OOBASE_BASIC_ALLOCATOR_H_INCLUDED_
#define OOBASE_BASIC_ALLOCATOR_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	template <size_t SIZE, typename Allocator = ThreadLocalAllocator>
	class StackAllocator : public AllocatorInstance
	{
	public:
		StackAllocator() : m_free(align_up(m_start,detail::alignof<index_t>::value))
		{
			static_assert(sizeof(m_start) < s_hibit_mask,"SIZE too big");

			reinterpret_cast<free_block_t*>(m_free)->m_size = sizeof(m_start);
			reinterpret_cast<free_block_t*>(m_free)->m_next = s_hibit_mask;
			reinterpret_cast<free_block_t*>(m_free)->m_prev = s_hibit_mask;
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

				alloc_end = align_up(alloc_start + min_alloc(bytes),detail::alignof<index_t>::value);

				free_end = free_start + get_size(free_start);
				if (alloc_end <= free_end)
					break;

				free_start = get_next(free_start);
			}

			// If we have no space, use the Allocator
			if (!free_start)
			{
				printf("StackAllocator out of space\n");
				return Allocator::allocate(bytes,align);
			}

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
				char* alloc_end = align_up(tag + min_alloc(bytes),detail::alignof<index_t>::value);
				char* free_end = adjacent + get_size(adjacent);

				if (alloc_end <= free_end)
				{
					// Merge with adjacent
					if (alloc_end <= free_end - sizeof(free_block_t))
						split_block(adjacent,alloc_end);

					reinterpret_cast<free_block_t*>(tag)->m_size += get_size(adjacent);
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
			return (align < detail::alignof<index_t>::value ? detail::alignof<index_t>::value : align);
		}

		static size_t min_alloc(size_t bytes)
		{
			return (bytes + sizeof(index_t) < sizeof(free_block_t) ? sizeof(free_block_t) : bytes + sizeof(index_t));
		}

		static index_t get_size(char* p)
		{
			return *reinterpret_cast<index_t*>(p) & ~s_hibit_mask;
		}

		char* get_next(char* p)
		{
			return (reinterpret_cast<free_block_t*>(p)->m_next == s_hibit_mask ? NULL : m_start + reinterpret_cast<free_block_t*>(p)->m_next);
		}

		char* get_adjacent(char* p)
		{
			char* n = p + get_size(p);
			return (n < m_start + sizeof(m_start) ? n : NULL);
		}

		void split_block(char* p, char* split)
		{
			free_block_t* orig_block = reinterpret_cast<free_block_t*>(p);
			free_block_t* new_block = reinterpret_cast<free_block_t*>(split);

			new_block->m_size = orig_block->m_size - (split - p);
			new_block->m_next = p - m_start;
			new_block->m_prev = orig_block->m_prev;

			if (orig_block->m_prev != s_hibit_mask)
				reinterpret_cast<free_block_t*>(m_start + orig_block->m_prev)->m_next = split - m_start;

			orig_block->m_size = split - p;
			orig_block->m_prev = split - m_start;

			if (m_free == p)
				m_free = split;
		}

		static bool is_free(char* p)
		{
			return *reinterpret_cast<index_t*>(p) & s_hibit_mask ? false : true;
		}

		static char* get_tag(void* ptr)
		{
			// Check alignment...
			if (reinterpret_cast<ptrdiff_t>(ptr) & (detail::alignof<index_t>::value - 1))
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

			if (alloc->m_prev != s_hibit_mask)
				reinterpret_cast<free_block_t*>(m_start + alloc->m_prev)->m_next = alloc->m_next;

			if (alloc->m_next != s_hibit_mask)
				reinterpret_cast<free_block_t*>(m_start + alloc->m_next)->m_prev = alloc->m_prev;

			if (m_free == p)
				m_free = get_next(p);

			alloc->m_size |= s_hibit_mask;
		}

		void free_tag(char* p)
		{
			free_block_t* block = reinterpret_cast<free_block_t*>(p);
			block->m_size &= ~s_hibit_mask;

			char* adjacent = get_adjacent(p);
			if (adjacent && is_free(adjacent))
			{
				// Merge with adjacent
				free_block_t* adjacent_block = reinterpret_cast<free_block_t*>(adjacent);
				block->m_size += adjacent_block->m_size;
				block->m_next = adjacent_block->m_next;
				block->m_prev = adjacent_block->m_prev;

				if (block->m_prev != s_hibit_mask)
					reinterpret_cast<free_block_t*>(m_start + block->m_prev)->m_next = p - m_start;

				if (block->m_next != s_hibit_mask)
					reinterpret_cast<free_block_t*>(m_start + block->m_next)->m_prev = p - m_start;

				if (m_free == adjacent)
					m_free = p;
			}
			else
			{
				// Add to head of free list
				block->m_next = (m_free ? m_free - m_start : s_hibit_mask);
				block->m_prev = s_hibit_mask;
				m_free = p;
			}

			char* next = get_next(p);
			if (next && get_adjacent(next) == p)
			{
				free_block_t* next_block = reinterpret_cast<free_block_t*>(next);
				if (next_block->m_prev == p - m_start)
				{
					next_block->m_size += block->m_size;
					next_block->m_prev = block->m_prev;

					if (next_block->m_prev != s_hibit_mask)
						reinterpret_cast<free_block_t*>(m_start + next_block->m_prev)->m_next = next - m_start;

					if (m_free == p)
						m_free = next;

					p = next;  // DEBUG
				}
			}
		}

		static const index_t s_hibit_mask = 1 << (sizeof(index_t)*8 - 1);

		char  m_start[((SIZE / sizeof(index_t))+(SIZE % sizeof(index_t) ? 1 : 0)) * sizeof(index_t)];
		char* m_free;
	};
}

#endif // OOBASE_BASIC_ALLOCATOR_H_INCLUDED_