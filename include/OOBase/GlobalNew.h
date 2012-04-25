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

#ifndef OOBASE_CUSTOM_NEW_H_INCLUDED_
#define OOBASE_CUSTOM_NEW_H_INCLUDED_

#include "Memory.h"

// Global operator new overloads
#if defined(_MSC_VER)
#define THROW_BAD_ALLOC
#else
#define THROW_BAD_ALLOC throw(std::bad_alloc)
#endif

// Throws std::bad_alloc
inline void* operator new(size_t size) THROW_BAD_ALLOC
{
	assert(false);

	void* p = OOBase::HeapAllocator::allocate(size);
	if (!p)
		throw std::bad_alloc();

	return p;
}

inline void* operator new[](size_t size) THROW_BAD_ALLOC
{
	assert(false);

	void* p = OOBase::HeapAllocator::allocate(size);
	if (!p)
		throw std::bad_alloc();

	return p;
}

inline void operator delete(void* p) throw()
{
	return OOBase::HeapAllocator::free(p);
}

inline void operator delete[](void* p) throw()
{
	return OOBase::HeapAllocator::free(p);
}

// Returns NULL, doesn't throw
inline void* operator new(size_t size, const std::nothrow_t&) throw()
{
	return OOBase::HeapAllocator::allocate(size);
}

inline void* operator new[](size_t size, const std::nothrow_t&) throw()
{
	return OOBase::HeapAllocator::allocate(size);
}

inline void operator delete(void* p, const std::nothrow_t&) throw()
{
	return OOBase::HeapAllocator::free(p);
}

inline void operator delete[](void* p, const std::nothrow_t&) throw()
{
	return OOBase::HeapAllocator::free(p);
}

#endif // OOBASE_CUSTOM_NEW_H_INCLUDED_
