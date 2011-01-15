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
	template <typename T>
	class CriticalAllocator
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
			typedef CriticalAllocator<U> other;
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
		CriticalAllocator() throw() 
		{}

		CriticalAllocator(const CriticalAllocator&) throw() 
		{}

		template <typename U>
		CriticalAllocator (const CriticalAllocator<U>&) throw() 
		{}

		template<typename U>
		CriticalAllocator& operator = (const CriticalAllocator<U>&) throw()
		{
			return *this;
		}

		~CriticalAllocator() throw() 
		{}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() 
		{
		   return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const T& value) 
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
		pointer allocate(size_type num, const void* = 0) 
		{
			if (!num)
				return 0;

			void* p = 0;
			if (num > 1)
				p = OOBase::Allocate(num*sizeof(T),1,"OOBase::CriticalAllocator::allocate()",0);
			else
				p = OOBase::Allocate(sizeof(T),0,"OOBase::CriticalAllocator::allocate()",0);
			
			if (!p)
				::OOBase::CallCriticalFailureMem("OOBase::CriticalAllocator::allocate()",0);

			return static_cast<pointer>(p);
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type num) 
		{
			if (p && num)
			{
				if (num > 1)
					OOBase::Free(p,1);
				else
					OOBase::Free(p,0);
			}	
		}
	};

	template <typename T>
	class LocalAllocator
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
			typedef LocalAllocator<U> other;
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
		LocalAllocator() throw() 
		{}

		LocalAllocator(const LocalAllocator&) throw() 
		{}

		template <typename U>
		LocalAllocator (const LocalAllocator<U>&) throw() 
		{}

		template<typename U>
		LocalAllocator& operator = (const LocalAllocator<U>&) throw()
		{
			return *this;
		}

		~LocalAllocator() throw() 
		{}

		// return maximum number of elements that can be allocated
		size_type max_size() const throw() 
		{
		   return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// initialize elements of allocated storage p with value value
		void construct(pointer p, const T& value) 
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
		pointer allocate(size_type num, const void* = 0) 
		{
			if (!num)
				return 0;

			void* p = OOBase::Allocate(num*sizeof(T),2,"OOBase::LocalAllocator::allocate()",0);
			if (!p)
				::OOBase::CallCriticalFailureMem("OOBase::LocalAllocator::allocate()",0);

			return static_cast<pointer>(p);
		}

		// deallocate storage p of deleted elements
		void deallocate(pointer p, size_type)
		{
			if (p)
				OOBase::Free(p,2);
		}
	};
}

namespace std
{
	// return that all specializations of this allocator are interchangeable
	template <typename T1, typename T2>
	bool operator == (const OOBase::CriticalAllocator<T1>&, const OOBase::CriticalAllocator<T2>&) throw() 
	{
		return true;
	}

	template <typename T1, typename T2>
	bool operator != (const OOBase::CriticalAllocator<T1>&, const OOBase::CriticalAllocator<T2>&) throw() 
	{
		return false;
	}

	// return that all specializations of this allocator are interchangeable
	template <typename T1, typename T2>
	bool operator == (const OOBase::LocalAllocator<T1>&, const OOBase::LocalAllocator<T2>&) throw() 
	{
		return true;
	}

	template <typename T1, typename T2>
	bool operator != (const OOBase::LocalAllocator<T1>&, const OOBase::LocalAllocator<T2>&) throw() 
	{
		return false;
	}
}

namespace OOBase
{
	// Some useful typedefs
	typedef std::basic_string<char, std::char_traits<char>, OOBase::CriticalAllocator<char> > string;
	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, OOBase::CriticalAllocator<wchar_t> > wstring;

	typedef std::basic_ostringstream<char, std::char_traits<char>, OOBase::CriticalAllocator<char> > ostringstream;

	typedef std::basic_string<char, std::char_traits<char>, OOBase::LocalAllocator<char> > local_string;
	typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, OOBase::LocalAllocator<wchar_t> > local_wstring;

	string system_error_text(int err);
}

#endif // OOBASE_ALLOCATOR_H_INCLUDED_
