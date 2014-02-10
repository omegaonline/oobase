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
	template <typename K, typename V, typename Allocator = CrtAllocator>
	class Table : public detail::VectorImpl<Pair<K,V>,Allocator>
	{
		typedef Pair<K,V> Node;
		typedef detail::VectorImpl<Pair<K,V>,Allocator> baseClass;

	public:
		static const size_t npos = size_t(-1);

		Table() : baseClass(), m_sorted(true)
		{}

		explicit Table(AllocatorInstance& allocator) : baseClass(allocator), m_sorted(true)
		{}

		int insert(const K& key, const V& value)
		{
			int err = baseClass::push_back(make_pair(key,value));
			if (!err)
			{
				// Only clear sorted flag if we are sorted, and have inserted an
				// item that does not change the sort order
				if (m_sorted && this->m_size > 1 && !default_sort(*key_at(this->m_size-2),key))
					m_sorted = false;
			}

			return err;
		}

		bool erase(size_t pos, K* key = NULL, V* value = NULL)
		{
			Node node;
			if (!baseClass::erase(pos,&node))
				return false;

			if (key)
				*key = node.first;

			if (value)
				*value = node.second;

			return true;
		}

		template <typename K1>
		bool remove(const K1& key, V* value = NULL)
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return false;

			return erase(pos,NULL,value);
		}

		bool pop_back(K* key = NULL, V* value = NULL)
		{
			Node node;
			if (!baseClass::pop_back(&node))
				return false;

			if (key)
				*key = node.first;

			if (value)
				*value = node.second;

			return true;
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			return (find_i(key,false) != npos);
		}

		template <typename K1>
		V* find(const K1& key)
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return NULL;

			return at(pos);
		}

		template <typename K1>
		const V* find(const K1& key) const
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return NULL;

			return at(pos);
		}

		template <typename K1>
		bool find(const K1& key, V& value, bool first = false) const
		{
			size_t pos = find_i(key,first);
			if (pos == npos)
				return false;

			value = *at(pos);
			return true;
		}

		template <typename K1>
		size_t find_first(const K1& key) const
		{
			return find_i(key,true);
		}

		template <typename K1>
		size_t find_at(const K1& key) const
		{
			return find_i(key,false);
		}

		V* at(size_t pos)
		{
			Node* n = baseClass::at(pos);
			return (n ? &n->second : NULL);
		}

		const V* at(size_t pos) const
		{
			const Node* n = baseClass::at(pos);
			return (n ? &n->second : NULL);
		}

		const K* key_at(size_t pos) const
		{
			const Node* n = baseClass::at(pos);
			return (n ? &n->first : NULL);
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

					while (j >= h && (*less_than)(v.first,this->m_data[j-h].first))
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
		bool m_sorted;

		template <typename K1>
		size_t find_i(const K1& key, bool first) const
		{
			const Node* p = bsearch(key);

			// Scan for the first
			while (p && first && p > this->m_data && (p-1)->first == p->first)
				--p;

			return (p ? static_cast<size_t>(p - this->m_data) : npos);
		}

		template <typename K1>
		const Node* bsearch(const K1& key) const
		{
			// Always sort first
			const_cast<Table*>(this)->sort();

			const Node* base = this->m_data;
			for (size_t span = this->m_size; span > 0; span /= 2)
			{
				const Node* mid_point = base + (span / 2);
				if (mid_point->first == key)
					return mid_point;

				if (mid_point->first < key)
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
