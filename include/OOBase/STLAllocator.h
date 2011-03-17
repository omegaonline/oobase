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

#ifndef OOBASE_STL_ALLOCATOR_H_INCLUDED_
#define OOBASE_STL_ALLOCATOR_H_INCLUDED_

#include "Memory.h"

#include <string>

#if defined(max)
#undef max
#endif

#include <limits>

namespace OOBase
{
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
			CriticalOutOfMemory();
		}
	};

	// Allocator types
	template <typename FailureStrategy>
	class HeapAllocator
	{
	public:
		static void* allocate(size_t size, size_t count) 
		{
			void* p = NULL;
			if (count == 1)
				p = OOBase::ChunkAllocate(size);
			else
				p = OOBase::HeapAllocate(size*count);
			
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

	template <typename T, typename A>
	class STLAllocator
	{
	public:
		// type definitions
		typedef T              value_type;
		typedef T*             pointer;
		typedef const T*       const_pointer;
		typedef T&             reference;
		typedef const T&       const_reference;
		typedef std::size_t    size_type;
		typedef std::ptrdiff_t difference_type;

		// rebind allocator to type U
		template <typename U>
		struct rebind 
		{
			typedef STLAllocator<U,A> other;
		};

		// return address of values
		pointer address(reference value) const 
		{
			return &value;
		}

		const_pointer address(const_reference value) const
		{
			return &value;
		}

		// constructors and destructor
		// - nothing to do because the allocator has no state
		STLAllocator() throw() 
		{}

		STLAllocator(const STLAllocator&) throw() 
		{}

		template <typename U>
		STLAllocator (const STLAllocator<U,A>&) throw() 
		{}

		template<typename U>
		STLAllocator& operator = (const STLAllocator<U,A>&) throw()
		{
			return *this;
		}

		~STLAllocator() throw() 
		{}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() 
		{
		   return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const_reference value) 
		{
			// initialize memory with placement new
			new (static_cast<void*>(p)) T(value);
		}

		// destroy elements of initialized storage p
		void destroy(pointer p) 
		{
			// destroy objects by calling their destructor
			if (p) 
				p->~T();
		}

		// allocate but don't initialize num elements of type T
		pointer allocate(size_type num, const void* = NULL) 
		{
			return static_cast<pointer>(A::allocate(sizeof(T),num));
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type num) 
		{
			A::deallocate(p,sizeof(T),num);	
		}
	};

	// return that all specializations of this allocator are interchangeable
	template <typename T1, typename T2, typename A>
	bool operator == (const STLAllocator<T1,A>&, const STLAllocator<T2,A>&) throw()
	{
		return true;
	}

	template <typename T1, typename T2, typename A>
	bool operator != (const STLAllocator<T1,A>& a1, const STLAllocator<T2,A>& a2) throw()
	{
		return !(a1 == a2);
	}

	// Some useful typedefs
	typedef std::basic_string<char, std::char_traits<char>, STLAllocator<char,HeapAllocator<OOBase::StdFailure> > > string;
	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, STLAllocator<wchar_t,HeapAllocator<OOBase::StdFailure> > > wstring;
	typedef std::basic_ostringstream<char, std::char_traits<char>, STLAllocator<char,HeapAllocator<OOBase::StdFailure> > > ostringstream;

	typedef std::basic_string<char, std::char_traits<char>, STLAllocator<char,LocalAllocator<OOBase::StdFailure> > > local_string;
	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, STLAllocator<wchar_t,LocalAllocator<OOBase::StdFailure> > > local_wstring;

	string system_error_text(int err);
}

#endif // OOBASE_STL_ALLOCATOR_H_INCLUDED_
