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

		Table(const Compare& comp = Compare()) : baseClass(), m_compare(comp), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Table(AllocatorInstance& allocator) : baseClass(allocator), m_compare(), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Table(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Table(const Table& rhs) : baseClass(rhs), m_compare(rhs.m_compare), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

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
		bool insert(It first, It last)
		{
			for (It i = first; i != last; ++i)
			{
				if (insert(*i) == m_end)
					return false;
			}
			return true;
		}

		iterator insert(const Pair<K,V>& item)
		{
			size_t start = 0;
			for (size_t end = this->m_size;start < end;)
			{
				size_t mid = start + (end - start) / 2;
				if (m_compare(this->m_data[mid].first,item.first))
					start = mid + 1;
				else
					end = mid;
			}
			iterator r(m_end);
			if (baseClass::insert_at(start,item))
				r = iterator(this,start);
			return r;
		}

		iterator insert(typename call_traits<K>::param_type key, typename call_traits<V>::param_type value)
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
			if (i == m_end)
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
				return m_end;

			// Scan for the first
			while (p && p > this->m_data && (p-1)->first == key)
				--p;

			return (p ? iterator(this,static_cast<size_t>(p - this->m_data)) : m_end);
		}

		template <typename K1>
		const_iterator find(const K1& key) const
		{
			const Pair<K,V>* p = bsearch(key);
			if (!p || p->first != key)
				return m_cend;

			// Scan for the first
			while (p && p > this->m_data && (p-1)->first == key)
				--p;

			return (p ? const_iterator(this,static_cast<size_t>(p - this->m_data)) : m_cend);
		}

		template <typename K1>
		bool find_first(const K1& key, V& val) const
		{
			const_iterator i = find(key);
			if (!i)
				return false;

			val = i->second;
			return true;
		}

		iterator begin()
		{
			return baseClass::empty() ? m_end : iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return baseClass::empty() ? m_cend : const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : m_end);
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : m_cend);
		}

		const iterator& end()
		{
			return m_end;
		}

		const const_iterator& cend() const
		{
			return m_cend;
		}

		const const_iterator& end() const
		{
			return m_cend;
		}

	private:
		template <typename K1>
		const Pair<K,V>* bsearch(const K1& key) const
		{
			size_t start = 0;
			for (size_t end = this->m_size;start < end;)
			{
				size_t mid = start + (end - start) / 2;
				if (m_compare(this->m_data[mid].first,key))
					start = mid + 1;
				else if (this->m_data[mid].first == key)
					return &this->m_data[mid];
				else
					end = mid;
			}
			return NULL;
		}
		Compare m_compare;

		iterator m_end;
		const_iterator m_cend;
	};
}

#endif // OOBASE_TABLE_H_INCLUDED_
