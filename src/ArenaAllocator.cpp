/***********************************************************************************
 *
 * Copyright (C) 2012 Rick Taylor
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

#include "../include/OOBase/Base.h"

#define DLMALLOC_EXPORT static
#define MSPACES 1
#define ONLY_MSPACES 1
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1
#define FOOTERS 1

#define CORRUPTION_ERROR_ACTION(m)  OOBase_CallCriticalFailure("ArenaAllocator corrupted")
#define USAGE_ERROR_ACTION(m, p)  OOBase_CallCriticalFailure("Invalid pointer passed to ArenaAllocator")

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127 4702)

#if (_MSC_VER >= 1600) && defined(CODE_ANALYSIS)
#include <codeanalysis\warnings.h>
#pragma warning ( disable : ALL_CODE_ANALYSIS_WARNINGS )
#endif

#endif

#include "./dl_malloc.c"

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "../include/OOBase/ArenaAllocator.h"

OOBase::ArenaAllocator::ArenaAllocator(bool locked) : m_mspace(NULL)
{
	m_mspace = create_mspace(0,locked ? 1 : 0);
	if (!m_mspace)
		OOBase_CallCriticalFailure("Failed to create dl_malloc mspace");
}

OOBase::ArenaAllocator::~ArenaAllocator()
{
	if (m_mspace)
		destroy_mspace(m_mspace);
}

void* OOBase::ArenaAllocator::allocate(size_t bytes, size_t align)
{
	return mspace_memalign(m_mspace,align,bytes);
}

void* OOBase::ArenaAllocator::reallocate(void* ptr, size_t bytes, size_t align)
{
	if (align <= 8)
		return mspace_realloc(m_mspace,ptr,bytes);

	if (mspace_realloc_in_place(m_mspace,ptr,bytes))
		return ptr;

	void* new_ptr = mspace_memalign(m_mspace,align,bytes);
	if (new_ptr)
	{
		memcpy(new_ptr,ptr,mspace_usable_size(ptr));
		mspace_free(m_mspace,ptr);
	}
	return new_ptr;
}

void OOBase::ArenaAllocator::free(void* ptr)
{
	mspace_free(m_mspace,ptr);
}
