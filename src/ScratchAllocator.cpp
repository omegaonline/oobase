/***********************************************************************************
 *
 * Copyright (C) 2013 Rick Taylor
 *
 * This file is part of OOBase, the Omega Online Base library.
 *
 * OOBase is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OOBase is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
 *
 **********************************************************************************/

#include "../include/OOBase/StackAllocator.h"

#if !defined(OOBASE_STACK_ALLOC_CHECK) || !OOBASE_STACK_ALLOC_CHECK
#define check()
#endif

OOBase::ScratchAllocator::ScratchAllocator(char* start, size_t len) :
		m_start(align_up(start,alignof<index_t>::value)),
		m_end(start + (len >= index_t(-1) ? index_t(-1) : len)),
		m_free(m_start)
{
	reinterpret_cast<free_block_t*>(m_free)->m_size = (m_end - m_start) / sizeof(index_t);
	reinterpret_cast<free_block_t*>(m_free)->m_next = s_null_ptr;
	reinterpret_cast<free_block_t*>(m_free)->m_prev = s_null_ptr;
}

OOBase::ScratchAllocator::~ScratchAllocator()
{
	check();

	assert(m_free && m_free == m_start && is_free(m_start) && get_size(m_start) == (m_end - m_start));
}

void* OOBase::ScratchAllocator::allocate(size_t bytes, size_t align)
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

	if (!free_start)
	{
#if defined(_WIN32)
		SetLastError(ERROR_OUTOFMEMORY);
#else
		errno = ERROR_OUTOFMEMORY;
#endif
		return NULL;
	}

	if (alloc_end <= free_end - sizeof(free_block_t))
		split_block(free_start,alloc_end);

	if (alloc_start != free_start)
		split_block(free_start,alloc_start);

	alloc_tag(alloc_start);
	return alloc_start + sizeof(index_t);
}

void* OOBase::ScratchAllocator::reallocate(void* ptr, size_t bytes, size_t align)
{
	if (!ptr)
		return allocate(bytes,align);

	char* tag = get_tag(ptr);
	if (get_size(tag) >= bytes + sizeof(index_t))
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
	void* ptr_new = ScratchAllocator::allocate(bytes,align);
	if (ptr_new)
	{
		memcpy(ptr_new,ptr,get_size(tag) - sizeof(index_t));
		free_tag(tag);
	}
	return ptr_new;
}

void OOBase::ScratchAllocator::free(void* ptr)
{
	if (ptr)
		free_tag(get_tag(ptr));
}

bool OOBase::ScratchAllocator::is_our_ptr(void* ptr) const
{
	return (ptr >= m_start + sizeof(index_t) && ptr < m_end);
}

char* OOBase::ScratchAllocator::align_up(char* p, size_t align)
{
	size_t o = (reinterpret_cast<size_t>(p) % align);
	if (o)
		p += align - o;
	return p;
}

size_t OOBase::ScratchAllocator::correct_align(size_t align)
{
	return (align < alignof<index_t>::value ? alignof<index_t>::value : align);
}

size_t OOBase::ScratchAllocator::min_alloc(size_t bytes)
{
	return (bytes + sizeof(index_t) < sizeof(free_block_t) ? sizeof(free_block_t) : bytes + sizeof(index_t));
}

OOBase::ScratchAllocator::index_t OOBase::ScratchAllocator::get_size(char* p)
{
	return (*reinterpret_cast<index_t*>(p) & ~s_in_use_mask) * sizeof(index_t);
}

char* OOBase::ScratchAllocator::get_next(char* p)
{
	return (reinterpret_cast<free_block_t*>(p)->m_next == s_null_ptr ? NULL : m_start + (reinterpret_cast<free_block_t*>(p)->m_next) * sizeof(index_t));
}

char* OOBase::ScratchAllocator::get_adjacent(char* p) const
{
	char* n = p + get_size(p);
	return (n < m_end ? n : NULL);
}

OOBase::ScratchAllocator::index_t OOBase::ScratchAllocator::index_of(char* p) const
{
	return static_cast<index_t>((p - m_start) / sizeof(index_t));
}

OOBase::ScratchAllocator::free_block_t* OOBase::ScratchAllocator::from_index(index_t idx)
{
	return reinterpret_cast<free_block_t*>(m_start + (idx * sizeof(index_t)));
}

void OOBase::ScratchAllocator::split_block(char* p, char* split)
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

bool OOBase::ScratchAllocator::is_free(char* p)
{
	return *reinterpret_cast<index_t*>(p) & s_in_use_mask ? false : true;
}

char* OOBase::ScratchAllocator::get_tag(void* ptr)
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

void OOBase::ScratchAllocator::alloc_tag(char* p)
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

	check();
}

void OOBase::ScratchAllocator::free_tag(char* p)
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

	check();
}

#if defined(OOBASE_STACK_ALLOC_CHECK) && OOBASE_STACK_ALLOC_CHECK
char* OOBase::ScratchAllocator::get_prev(char* p)
{
	return (reinterpret_cast<free_block_t*>(p)->m_prev == s_null_ptr ? NULL : m_start + (reinterpret_cast<free_block_t*>(p)->m_prev) * sizeof(index_t));
}

void OOBase::ScratchAllocator::check()
{
	printf("%p ",this);
	for (char* p = m_start; p < m_end && get_size(p); p += get_size(p))
	{
		printf("|%u:",index_of(p));
		if (is_free(p))
		{
			printf("F<");
			if (get_next(p))
			{
				if (get_prev(get_next(p)) == p)
					printf("%u,",index_of(get_next(p)));
				else
					printf("X,");
			}
			else
				printf("!,");

			if (get_prev(p))
			{
				if (get_next(get_prev(p)) == p)
					printf("%u>",index_of(get_prev(p)));
				else
					printf("X>");
			}
			else
				printf("!>");

			printf("[%u]",get_size(p));
		}
		else
			printf("U[%u]",get_size(p));
	}
	printf("| m_free = ");
	if (m_free)
		printf("%u\n",index_of(m_free));
	else
		printf("!\n");
}
#endif
