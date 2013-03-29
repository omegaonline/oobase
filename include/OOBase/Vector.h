///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#ifndef OOBASE_VECTOR_H_INCLUDED_
#define OOBASE_VECTOR_H_INCLUDED_

#include "Bag.h"

namespace OOBase
{
template <typename T, typename Allocator = CrtAllocator>
	class Vector : public detail::BagImpl<T,Allocator>
	{
		typedef detail::BagImpl<T,Allocator> baseClass;

	public:
		Vector() : baseClass()
		{}

		Vector(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		int push_back(const T& value)
		{
			return baseClass::push(value);
		}

		bool pop_back(T* value = NULL)
		{
			return baseClass::pop(value);
		}

		int insert(const T& value, size_t pos)
		{
			return baseClass::insert_at(value,pos);
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

		void remove_at(size_t pos)
		{
			baseClass::remove_at(pos,true);
		}

		T* at(size_t pos)
		{
			return baseClass::at(pos);
		}

		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
		}
	};
}

#endif // OOBASE_VECTOR_H_INCLUDED_
