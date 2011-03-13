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

#ifndef OOBASE_ALLOCATOR_H_INCLUDED_
#define OOBASE_ALLOCATOR_H_INCLUDED_

#include "../config-base.h"

#if defined(max)
#undef max
#endif

namespace OOBase
{
	void* HeapAllocate(size_t bytes);
	void* HeapReallocate(void* p, size_t bytes);
	void HeapFree(void* p);

	void* LocalAllocate(size_t bytes);
	void* LocalReallocate(void* p, size_t bytes);
	void LocalFree(void* p);

	void* ChunkAllocate(size_t size, size_t count);
	void ChunkFree(void* p, size_t size, size_t count);
	
	class NullFailure
	{
	public:
		static void fail()
		{ }
	};

	class StdFailure
	{
	public:
		static void fail()
		{ 
			throw std::bad_alloc();
		}
	};

	class CriticalFailure
	{
	public:
		static void fail()
		{
			OOBase::CallCriticalFailureMem("OOBase::CriticalAllocator::allocate()",0);
		}
	};

	// Allocator types
	template <typename FailureStrategy>
	class HeapAllocator
	{
	public:
		static void* allocate(size_t size, size_t count) 
		{
			void* p = OOBase::HeapAllocate(size*count);
			if (p == NULL)
				FailureStrategy::fail();

			return p;
		}

		static void deallocate(void* p, size_t /*size*/, size_t /*count*/) 
		{
			OOBase::HeapFree(p);
		}
	};

	template <typename FailureStrategy>
	class ChunkAllocator
	{
	public:
		static void* allocate(size_t size, size_t count) 
		{
			void* p = OOBase::ChunkAllocate(size,count);
			if (p == NULL)
				FailureStrategy::fail();

			return p;
		}

		static void deallocate(void* p, size_t size, size_t count) 
		{
			OOBase::ChunkFree(p,size,count);
		}
	};

	template <typename FailureStrategy>
	class LocalAllocator
	{
	public:
		static void* allocate(size_t size, size_t count) 
		{
			void* p = OOBase::LocalAllocate(size*count);
			if (p == NULL)
				FailureStrategy::fail();

			return p;
		}

		static void deallocate(void* p, size_t /*size*/, size_t /*count*/) 
		{
			OOBase::LocalFree(p);
		}
	};
}

#endif // OOBASE_ALLOCATOR_H_INCLUDED_
