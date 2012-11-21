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

#ifndef OOBASE_ARENA_ALLOCATOR_H_INCLUDED_
#define OOBASE_ARENA_ALLOCATOR_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	class ArenaAllocator : public AllocatorInstance
	{
	public:
		ArenaAllocator(size_t capacity = 0);
		~ArenaAllocator();

		void* allocate(size_t bytes, size_t align);
		void* reallocate(void* ptr, size_t bytes, size_t align);
		void free(void* ptr);

	private:
		void* m_mspace;
	};
}

#endif // OOBASE_ARENA_ALLOCATOR_H_INCLUDED_
