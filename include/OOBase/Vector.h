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
		typedef detail::IteratorImpl<Vector,T,size_t> iterator;
		typedef detail::IteratorImpl<const Vector,const T,size_t> const_iterator;

		Vector() : baseClass()
		{}

		Vector(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		int push_back(const T& value)
		{
			return baseClass::append(value);
		}

		bool pop_back(T* value = NULL)
		{
			return baseClass::remove_at(this->m_size - 1,false,value);
		}

		template <typename T1>
		iterator find(const T1& value)
		{
			size_t pos = 0;
			for (;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
					break;
			}
			return iterator(*this,pos);
		}

		template <typename T1>
		const_iterator find(const T1& value) const
		{
			size_t pos = 0;
			for (;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
					break;
			}
			return const_iterator(*this,pos);
		}

		int insert(const T& value, const iterator& iter)
		{
			assert(iter.check(this));
			return baseClass::insert_at(value,iter.deref());
		}

		bool remove_at(const iterator& iter, T* pval = NULL)
		{
			assert(iter.check(this));
			return baseClass::remove_at(iter.deref(),true,pval);
		}

		T* at(const iterator& iter)
		{
			assert(iter.check(this));
			return baseClass::at(iter.deref());
		}

		const T* at(const const_iterator& iter) const
		{
			assert(iter.check(this));
			return baseClass::at(iter.deref());
		}

		T* at(size_t pos)
		{
			return baseClass::at(pos);
		}

		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
		}

		T& operator [](size_t pos)
		{
			return *at(pos);
		}

		const T& operator [](size_t pos) const
		{
			return *at(pos);
		}

		iterator begin()
		{
			return iterator(*this,0);
		}

		const_iterator begin() const
		{
			return const_iterator(*this,0);
		}

		iterator end()
		{
			return iterator(*this,this->m_size);
		}

		const_iterator end() const
		{
			return const_iterator(*this,this->m_size);
		}
	};
}

#endif // OOBASE_VECTOR_H_INCLUDED_
