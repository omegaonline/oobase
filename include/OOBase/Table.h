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

#include "Bag.h"

namespace OOBase
{
	namespace detail
	{
		namespace Table
		{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4610 4510)
#endif
			template <typename K, typename V>
			struct PODCheck
			{
				K k;
				V v;
			};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
		}

		template <typename K, typename V, bool POD = false>
		struct TableNode
		{
			TableNode(const TableNode& n) : m_key(n.m_key), m_value(n.m_value)
			{}

			template <typename K1, typename V1>
			TableNode(const K1& k, const V1& v) : m_key(k), m_value(v)
			{}

			TableNode& operator = (const TableNode& rhs)
			{
				if (this != &rhs)
				{
					m_key = rhs.m_key;
					m_value = rhs.m_value;
				}
				return *this;
			}

			template <typename K1, typename V1>
			static TableNode build(const K1& k, const V1& v)
			{
				return TableNode(k,v);
			}

			K m_key;
			V m_value;
		};

		template <typename K, typename V>
		struct TableNode<K,V,true>
		{
			template <typename K1, typename V1>
			static TableNode build(const K1& k, const V1& v)
			{
				TableNode n = { k, v };
				return n;
			}

			K m_key;
			V m_value;
		};
	}

	template <typename K, typename V, typename Allocator = CrtAllocator>
	class Table : public detail::BagImpl<detail::TableNode<K,V,detail::is_pod<detail::Table::PODCheck<K,V> >::value>,Allocator>
	{
		typedef detail::TableNode<K,V,detail::is_pod<detail::Table::PODCheck<K,V> >::value> Node;
		typedef detail::BagImpl<detail::TableNode<K,V,detail::is_pod<detail::Table::PODCheck<K,V> >::value>,Allocator> baseClass;

	public:
		static const size_t npos = size_t(-1);

		Table() : baseClass(), m_sorted(true)
		{}

		Table(AllocatorInstance& allocator) : baseClass(allocator), m_sorted(true)
		{}

		template <typename K1, typename V1>
		int insert(const K1& key, const V1& value)
		{
			int err = baseClass::push(Node::build(key,value));
			if (!err)
				m_sorted = false;
			return err;
		}

		void remove_at(size_t pos)
		{
			baseClass::remove_at(pos,m_sorted);
		}

		template <typename K1>
		bool remove(const K1& key, V* value = NULL)
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return false;

			if (value)
				*value = *at(pos);

			remove_at(pos);

			return true;
		}

		bool pop(K* key = NULL, V* value = NULL)
		{
			if (key || value)
			{
				Node n;
				bool ret = baseClass::pop(&n);
				if (ret)
				{
					if (key)
						*key = n->m_key;

					if (value)
						*value = n->m_value;
				}
				return ret;
			}

			return baseClass::pop();
		}

		template <typename K1>
		bool exists(K1 key) const
		{
			return (find_i(key,false) != npos);
		}

		template <typename K1>
		V* find(K1 key)
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return NULL;

			return at(pos);
		}

		template <typename K1>
		const V* find(K1 key) const
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return NULL;

			return at(pos);
		}

		template <typename K1>
		bool find(K1 key, V& value, bool first = false) const
		{
			size_t pos = find_i(key,first);
			if (pos == npos)
				return false;

			value = *at(pos);
			return true;
		}

		template <typename K1>
		size_t find_first(K1 key) const
		{
			return find_i(key,true);
		}

		template <typename K1>
		size_t find_at(K1 key) const
		{
			return find_i(key,false);
		}

		V* at(size_t pos)
		{
			Node* n = baseClass::at(pos);
			return (n ? &n->m_value : NULL);
		}

		const V* at(size_t pos) const
		{
			const Node* n = baseClass::at(pos);
			return (n ? &n->m_value : NULL);
		}

		const K* key_at(size_t pos) const
		{
			const Node* n = baseClass::at(pos);
			return (n ? &n->m_key : NULL);
		}

		void sort()
		{
			if (!m_sorted)
				sort(&default_sort);
		}

		void sort(bool (*less_than)(const K& k1, const K& k2))
		{
			// This is a Shell-sort because we mostly use sort() in cases where qsort() behaves badly
			// i.e. insertion at the end and then sort
			// see http://warp.povusers.org/SortComparison

			// Generate the split intervals
			// Knuth is my homeboy :)
			size_t h = 1;
			while (h <= this->m_size / 9)
				h = 3*h + 1;
			
			for (;h > 0; h /= 3)
			{
				for (size_t i = h; i < this->m_size; ++i)
				{
					Node v = this->m_data[i];
					size_t j = i;

					while (j >= h && (*less_than)(v.m_key,this->m_data[j-h].m_key))
					{
						this->m_data[j] = this->m_data[j-h];
						j -= h;
					}

					this->m_data[j] = v;
				}
			}

			m_sorted = (less_than == &default_sort);
		}

	private:
		mutable bool m_sorted;

		template <typename K1>
		size_t find_i(K1 key, bool first) const
		{
			const Node* p = bsearch(key);

			// Scan for the first
			while (p && first && p > this->m_data && (p-1)->m_key == p->m_key)
				--p;

			return (p ? static_cast<size_t>(p - this->m_data) : npos);
		}

		template <typename K1>
		const Node* bsearch(K1 key) const
		{
			// Always sort first
			const_cast<Table*>(this)->sort();

			const Node* base = this->m_data;
			for (size_t span = this->m_size; span > 0; span /= 2)
			{
				const Node* mid_point = base + (span / 2);
				if (mid_point->m_key == key)
					return mid_point;

				if (mid_point->m_key < key)
				{
					base = mid_point + 1;
					--span;
				}
			}
			return NULL;
		}

		static bool default_sort(const K& k1, const K& k2)
		{
			return (k1 < k2);
		}
	};
}

#endif // OOBASE_TABLE_H_INCLUDED_
