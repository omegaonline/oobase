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

#include "Memory.h"

namespace OOBase
{
	template <typename V, typename Allocator = HeapAllocator>
	class Set
	{
	public:
		static const size_t npos = size_t(-1);

		Set() : m_data(NULL), m_capacity(0), m_size(0), m_sorted(true)
		{}

		~Set()
		{
			clear();

			Allocator::free(m_data);
		}

		int reserve(size_t capacity)
		{
			return reserve_i(capacity + m_size);
		}

		int insert(const V& value)
		{
			if (m_size+1 > m_capacity)
			{
				size_t cap = (m_capacity == 0 ? 4 : m_capacity*2);
				int err = reserve_i(cap);
				if (err != 0)
					return err;
			}

			// Placement new
			::new (&m_data[m_size]) V(value);

			// This is exception safe as the increment happens here
			++m_size;
			m_sorted = false;

			return 0;
		}

		void remove_at(size_t pos)
		{
			if (m_data && pos < m_size)
			{
				for(--m_size;pos < m_size;++pos)
					m_data[pos] = m_data[pos+1];

				m_data[pos].~V();
			}
		}

		bool remove(const V& value)
		{
			size_t pos = find_i(value,false);
			if (pos == npos)
				return false;

			remove_at(pos);

			return true;
		}

		bool pop(V* value = NULL)
		{
			if (m_size == 0)
				return false;

			if (value)
				*value = m_data[m_size-1];

			m_data[--m_size].~V();
			return true;
		}

		void clear()
		{
			while (m_size > 0)
				m_data[--m_size].~V();
		}

		template <typename V1>
		bool exists(V1 key) const
		{
			return (find_i(key,false) != npos);
		}

		bool empty() const
		{
			return (m_size == 0);
		}

		size_t size() const
		{
			return m_size;
		}

		V* at(size_t pos)
		{
			return (m_data && pos < m_size ? &m_data[pos] : NULL);
		}

		const V* at(size_t pos) const
		{
			return (m_data && pos < m_size ? &m_data[pos] : NULL);
		}

		void sort()
		{
			if (!m_sorted)
				sort(&default_sort);
		}

		void sort(bool (*less_than)(const V& v1, const V& v2))
		{
			// This is a Shell-sort because we mostly use sort() in cases where qsort() behaves badly
			// i.e. insertion at the end and then sort
			// see http://warp.povusers.org/SortComparison

			// Generate the split intervals
			// Knuth is my homeboy :)
			size_t h = 1;
			while (h <= m_size / 9)
				h = 3*h + 1;

			for (;h > 0; h /= 3)
			{
				for (size_t i = h; i < m_size; ++i)
				{
					V v = m_data[i];
					size_t j = i;

					while (j >= h && (*less_than)(v,m_data[j-h]))
					{
						m_data[j] = m_data[j-h];
						j -= h;
					}

					m_data[j] = v;
				}
			}

			m_sorted = (less_than == &default_sort);
		}

	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur...
		// and you probably don't want to be copying these around
		Set(const Set&);
		Set& operator = (const Set&);

		V*     m_data;
		size_t m_capacity;
		size_t m_size;

		mutable bool m_sorted;

		int reserve_i(size_t capacity)
		{
			if (m_capacity < capacity)
			{
				V* new_data = static_cast<V*>(Allocator::allocate(capacity*sizeof(V)));
				if (!new_data)
					return ERROR_OUTOFMEMORY;

				for (size_t i=0;i<m_size;++i)
					::new (&new_data[i]) V(m_data[i]);

				for (size_t i=0;i<m_size;++i)
					m_data[i].~V();

				Allocator::free(m_data);
				m_data = new_data;
				m_capacity = capacity;
			}
			return 0;
		}

		template <typename V1>
		size_t find_i(V1 key, bool first) const
		{
			const V* p = bsearch(key);

			// Scan for the first
			while (p && first && p > m_data && *(p-1) == *p)
				--p;

			return (p ? static_cast<size_t>(p - m_data) : npos);
		}

		template <typename V1>
		const V* bsearch(V1 key) const
		{
			// Always sort first
			const_cast<Set*>(this)->sort();

			const V* base = m_data;
			for (size_t span = m_size; span > 0; span /= 2)
			{
				const V* mid_point = base + (span / 2);
				if (*mid_point == key)
					return mid_point;

				if (*mid_point < key)
				{
					base = mid_point + 1;
					--span;
				}
			}
		    return NULL;
		}

		static bool default_sort(const V& v1, const V& v2)
		{
			return (v1 < v2);
		}
	};
}

#endif // OOBASE_SET_H_INCLUDED_
