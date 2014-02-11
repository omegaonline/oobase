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

#ifndef OOBASE_TABLE_H_INCLUDED_
#define OOBASE_TABLE_H_INCLUDED_

#include "Vector.h"

namespace OOBase
{
	template <typename K, typename V, typename Compare = Less<K>, typename Allocator = CrtAllocator>
	class Table : public detail::VectorImpl<Pair<K,V>,Allocator>
	{
		typedef detail::VectorImpl<Pair<K,V>,Allocator> baseClass;

	public:
		typedef K key_type;
		typedef Pair<K,V> value_type;
		typedef V mapped_type;
		typedef Compare key_compare;
		typedef Allocator allocator_type;
		typedef value_type& reference;
		typedef typename add_const<reference>::type const_reference;
		typedef value_type* pointer;
		typedef typename add_const<pointer>::type const_pointer;

		typedef detail::IteratorImpl<Table,value_type,size_t> iterator;
		friend class detail::IteratorImpl<Table,value_type,size_t>;
		typedef detail::IteratorImpl<const Table,const value_type,size_t> const_iterator;
		friend class detail::IteratorImpl<const Table,const value_type,size_t>;

		explicit Table(const Compare& comp = Compare()) : baseClass(), m_compare(comp)
		{}

		explicit Table(AllocatorInstance& allocator) : baseClass(allocator), m_compare()
		{}

		Table(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp)
		{}

		Table(const Table& rhs) : baseClass(rhs), m_compare(rhs.m_compare)
		{}

		Table& operator = (const Table& rhs)
		{
			Table(rhs).swap(*this);
			return *this;
		}

		void swap(Table& rhs)
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

		int insert(const Pair<K,V>& value)
		{
			const Pair<K,V>* p = bsearch(value.first);
			if (!p)
				return baseClass::push_back(value);
			else
				return baseClass::insert_at(p - this->m_data,value);
		}

		int insert(const K& key, const V& value)
		{
			return insert(OOBase::make_pair(key,value));
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

		template <typename K1>
		bool remove(const K1& key)
		{
			iterator i = find(key);
			if (i == end())
				return false;

			erase(i);
			return true;
		}

		bool pop_back()
		{
			return baseClass::pop_back();
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			const Pair<K,V>* p = bsearch(key);
			return (p && p->first == key);
		}

		template <typename K1>
		iterator find(const K1& key)
		{
			const Pair<K,V>* p = bsearch(key);
			if (!p || p->first != key)
				return end();

			// Scan for the first
			while (p && p > this->m_data && (p-1)->first == key)
				--p;

			return (p ? iterator(this,static_cast<size_t>(p - this->m_data)) : end());
		}

		template <typename K1>
		const_iterator find(const K1& key) const
		{
			const Pair<K,V>* p = bsearch(key);
			if (!p || p->first != key)
				return end();

			// Scan for the first
			while (p && p > this->m_data && (p-1)->first == key)
				--p;

			return (p ? const_iterator(this,static_cast<size_t>(p - this->m_data)) : end());
		}

		template <typename K1>
		bool find(const K1& key, V& value) const
		{
			const Pair<K,V>* p = bsearch(key);
			if (!p || p->first != key)
				return false;

			// Scan for the first
			while (p && p > this->m_data && (p-1)->first == key)
				--p;

			if (!p)
				return false;

			value = p->second;
			return true;
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
		const Pair<K,V>* bsearch(const K1& key) const
		{
			const Pair<K,V>* base = this->m_data;
			const Pair<K,V>* mid_point = NULL;
			for (size_t span = this->m_size; span > 0; span /= 2)
			{
				mid_point = base + (span / 2);
				if (m_compare(mid_point->first,key))
				{
					base = mid_point + 1;
					if (--span == 0)
						mid_point = NULL; // The end...
				}
				else if (mid_point->first == key)
					break;
			}
			return mid_point;
		}
		Compare m_compare;
	};
}

#endif // OOBASE_TABLE_H_INCLUDED_
