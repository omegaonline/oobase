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
#include <new>

namespace OOBase
{
	void* HeapAllocate(size_t bytes);
	void* HeapReallocate(void* p, size_t bytes);
	void HeapFree(void* p);

	void* LocalAllocate(size_t bytes);
	void* LocalReallocate(void* p, size_t bytes);
	void LocalFree(void* p);

	struct critical_t { int unused; };
	extern const critical_t critical;

	// Allocator types
	class CrtAllocator
	{
	public:
		static void* allocate(size_t len)
		{
			return ::malloc(len);
		}

		static void* reallocate(void* ptr, size_t len)
		{
			return ::realloc(ptr,len);
		}

		static void free(void* ptr)
		{
			::free(ptr);
		}	
	};

	class HeapAllocator
	{
	public:
		static void* allocate(size_t size) 
		{
			return OOBase::HeapAllocate(size);
		}

		static void* reallocate(void* p, size_t size)
		{
			return OOBase::HeapReallocate(p,size);
		}

		static void free(void* p) 
		{
			OOBase::HeapFree(p);
		}
	};

	class LocalAllocator
	{
	public:
		static void* allocate(size_t size) 
		{
			return OOBase::LocalAllocate(size);
		}

		static void* reallocate(void* p, size_t size)
		{
			return OOBase::LocalReallocate(p,size);
		}

		static void free(void* p) 
		{
			OOBase::LocalFree(p);
		}
	};
}

// Global operator new overloads
void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, const std::nothrow_t&);
void* operator new[](size_t size, const std::nothrow_t&);
void* operator new(size_t size, const OOBase::critical_t&);
void* operator new[](size_t size, const OOBase::critical_t&);
void operator delete(void* p);
void operator delete(void* p, const std::nothrow_t&);
void operator delete(void* p, const OOBase::critical_t&);
void operator delete[](void* p);
void operator delete[](void* p, const std::nothrow_t&);
void operator delete[](void* p, const OOBase::critical_t&);

#endif // OOBASE_MEMORY_H_INCLUDED_
