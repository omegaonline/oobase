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

#ifndef OOBASE_STACK_H_INCLUDED_
#define OOBASE_STACK_H_INCLUDED_

#include "Bag.h"

namespace OOBase
{
	template <typename T, typename Allocator = CrtAllocator>
	class Stack : private Bag<T,Allocator>
	{
	public:
		Stack() : Bag<T,Allocator>()
		{}

		Stack(Allocator& allocator) : Bag<T,Allocator>(allocator)
		{}
			
		int push(const T& value)
		{
			return Bag<T,Allocator>::add(value);
		}
		
		void remove_at(size_t pos)
		{
			Bag<T,Allocator>::remove_at_sorted(pos);
		}
		
		void clear()
		{
			Bag<T,Allocator>::clear();
		}
		
		bool pop(T* value = NULL)
		{
			return Bag<T,Allocator>::pop(value);
		}
		
		bool empty() const
		{
			return Bag<T,Allocator>::empty();
		}			
		
		size_t size() const
		{
			return Bag<T,Allocator>::size();
		}

		const T* at(size_t pos) const
		{
			return Bag<T,Allocator>::at(pos);
		}
	};
}

#endif // OOBASE_STACK_H_INCLUDED_
