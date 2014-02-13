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

#ifndef OOBASE_SET_H_INCLUDED_
#define OOBASE_SET_H_INCLUDED_

#include "Vector.h"

namespace OOBase
{
	template <typename T, typename Compare = Less<T>, typename Allocator = CrtAllocator>
	class Set : public detail::VectorImpl<T,Allocator>
	{
		typedef detail::VectorImpl<T,Allocator> baseClass;

	public:
		typedef T value_type;
		typedef Compare key_compare;
		typedef Allocator allocator_type;
		typedef T& reference;
		typedef typename add_const<reference>::type const_reference;
		typedef T* pointer;
		typedef typename add_const<pointer>::type const_pointer;

		typedef detail::IteratorImpl<Set,value_type,size_t> iterator;
		friend class detail::IteratorImpl<Set,value_type,size_t>;
		typedef detail::IteratorImpl<const Set,const value_type,size_t> const_iterator;
		friend class detail::IteratorImpl<const Set,const value_type,size_t>;

		explicit Set(const Compare& comp) : baseClass(), m_compare(comp)
		{}

		explicit Set(AllocatorInstance& allocator) : baseClass(allocator), m_compare()
		{}

		Set(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp)
		{}

		Set(const Set& rhs) : baseClass(rhs), m_compare(rhs.m_compare)
		{}

		Set& operator = (const Set& rhs)
		{
			Set(rhs).swap(*this);
			return *this;
		}

		void swap(Set& rhs)
		{
			baseClass::swap(rhs);
			OOBase::swap(m_compare,rhs.m_compare);
		}

		template <typename It>
		int insert(It first, It last)
		{
			int err = 0;
			for (It i = first; i != last; ++i)
			{
				if ((err = insert(*i)) != 0)
					break;
			}
			return err;
		}

		template <typename T1>
		int insert(const T1& value)
		{
			size_t start = 0;
			for (size_t end = this->m_size;start < end;)
			{
				size_t mid = start + (end - start) / 2;
				if (m_compare(this->m_data[mid],value))
					start = mid + 1;
				else
					end = mid;
			}
			return baseClass::insert_at(start,value);
		}

		iterator erase(iterator iter)
		{
			assert(iter.check(this));
			return iterator(this,baseClass::remove_at(iter.deref(),1));
		}

		iterator erase(iterator first, iterator last)
		{
			assert(first.check(this) && last.check(this));
			return iterator(this,baseClass::remove_at(first.deref(),last.deref() - first.deref()));
		}

		template <typename T1>
		bool remove(const T1& value)
		{
			iterator i = find(value);
			if (i == end())
				return false;

			erase(i);
			return true;
		}

		bool pop_back()
		{
			return baseClass::pop_back();
		}

		template <typename T1>
		bool exists(const T1& value) const
		{
			const T* p = bsearch(value);
			return (p && *p == value);
		}

		template <typename T1>
		iterator find(const T1& value)
		{
			const T* p = bsearch(value);
			if (!p || *p != value)
				return end();

			// Scan for the first
			while (p && p > this->m_data && *(p-1) == value)
				--p;

			return (p ? iterator(this,static_cast<size_t>(p - this->m_data)) : end());
		}

		template <typename T1>
		const_iterator find(const T1& value) const
		{
			const T* p = bsearch(value);
			if (!p || *p != value)
				return end();

			// Scan for the first
			while (p && p > this->m_data && *(p-1) == value)
				--p;

			return (p ? const_iterator(this,static_cast<size_t>(p - this->m_data)) : end());
		}

		iterator begin()
		{
			return baseClass::empty() ? end() : iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return baseClass::empty() ? end() : const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : end());
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : end());
		}

		iterator end()
		{
			return iterator(this,size_t(-1));
		}

		const_iterator cend() const
		{
			return const_iterator(this,size_t(-1));
		}

		const_iterator end() const
		{
			return cend();
		}

	private:
		template <typename K1>
		const T* bsearch(const K1& key) const
		{
			const T* base = this->m_data;
			const T* mid_point = NULL;
			for (size_t span = this->m_size; span > 0; span /= 2)
			{
				mid_point = base + (span / 2);
				if (m_compare(*mid_point,key))
				{
					base = mid_point + 1;
					--span;
				}
				else if (*mid_point == key)
					return mid_point;
			}
			return NULL;
		}
		Compare m_compare;
	};
}

#endif // OOBASE_SET_H_INCLUDED_
