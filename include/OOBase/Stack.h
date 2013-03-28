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
	class Stack : public detail::BagImpl<T,Allocator>
	{
		typedef detail::BagImpl<T,Allocator> baseClass;

	public:
		Stack() : baseClass()
		{}

		Stack(AllocatorInstance& allocator) : baseClass(allocator)
		{}
			
		int push(const T& value)
		{
			return baseClass::push(value);
		}
		
		void remove_at(size_t pos)
		{
			baseClass::remove_at_sorted(pos);
		}

		bool remove(const T& value)
		{
			// This is just really useful!
			for (size_t pos = 0;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
				{
					remove_at(pos);
					return true;
				}
			}
			return false;
		}

		bool pop(T* value = NULL)
		{
			return baseClass::pop(value);
		}
		
		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
		}

		T* at(size_t pos)
		{
			return baseClass::at(pos);
		}
	};
}

#endif // OOBASE_STACK_H_INCLUDED_
