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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Destructor.h"

const OOBase::critical_t OOBase::critical = {0};

#if !defined(_WIN32)

void* OOBase::CrtAllocator::allocate(size_t bytes, size_t /*align*/)
{
	return ::malloc(bytes);
}

void* OOBase::CrtAllocator::reallocate(void* ptr, size_t bytes, size_t /*align*/)
{
	return ::realloc(ptr,bytes);
}

void OOBase::CrtAllocator::free(void* ptr)
{
	::free(ptr);
}

#endif
