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

#include "Memory.h"

namespace OOBase
{
	template <typename K, typename V, typename Allocator = HeapAllocator>
	class Table
	{
	public:
		static const size_t npos = size_t(-1);

		Table() : m_data(NULL), m_capacity(0), m_size(0), m_sorted(true)
		{}
			
		~Table()
		{
			clear();
			
			Allocator::free(m_data);
		}
		
		int reserve(size_t capacity)
		{
			return reserve_i(capacity + m_size);
		}
		
		int insert(const K& key, const V& value)
		{
			if (m_size+1 > m_capacity)
			{
				size_t cap = (m_capacity == 0 ? 4 : m_capacity*2);
				int err = reserve_i(cap);
				if (err != 0)
					return err;
			}
			
			// Placement new
			::new (&m_data[m_size]) Node(key,value);
			
			// This is exception safe as the increment happens here
			++m_size;
			m_sorted = false;
			
			return 0;
		}
		
		void remove_at(size_t pos)
		{
			if (m_data && pos < m_size)
			{
				m_data[pos] = m_data[--m_size];
				m_data[m_size].~Node();
				m_sorted = false;
			}
		}
		
		bool remove(const K& key, V* value = NULL)
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return false;
			
			if (value)
				*value = m_data[pos].m_value;
			
			remove_at(pos);
			
			return true;
		}
		
		bool pop(K* key = NULL, V* value = NULL)
		{
			if (m_size == 0)
				return false;
			
			if (key)
				*key = m_data[m_size-1].m_key; 
			
			if (value)
				*value = m_data[m_size-1].m_value; 
				
			m_data[--m_size].~Node();
			return true;
		}
		
		void clear()
		{
			while (m_size > 0)
				m_data[--m_size].~Node();
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
			
			return &m_data[pos].m_value;
		}

		template <typename K1>
		const V* find(K1 key) const
		{
			size_t pos = find_i(key,false);
			if (pos == npos)
				return NULL;

			return &m_data[pos].m_value;
		}

		template <typename K1>
		bool find(K1 key, V& value, bool first = false) const
		{
			size_t pos = find_i(key,first);
			if (pos == npos)
				return false;
			
			value = m_data[pos].m_value;
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
			return (m_data && pos < m_size ? &m_data[pos].m_value : NULL);
		}
		
		const V* at(size_t pos) const
		{
			return (m_data && pos < m_size ? &m_data[pos].m_value : NULL);
		}
		
		const K* key_at(size_t pos) const
		{
			return (m_data && pos < m_size ? &m_data[pos].m_key : NULL);
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
			while (h <= m_size / 9)
				h = 3*h + 1;
			
			for (;h > 0; h /= 3)
			{
				for (size_t i = h; i < m_size; ++i)
				{
					Node v = m_data[i];
					size_t j = i;
					
					while (j >= h && (*less_than)(v.m_key,m_data[j-h].m_key))
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
		Table(const Table&);
		Table& operator = (const Table&);
	
		struct Node
		{
			Node(const K& k, const V& v) : m_key(k), m_value(v)
			{}
				
			K m_key;
			V m_value;
		};
		
		Node*  m_data;
		size_t m_capacity;
		size_t m_size;

		mutable bool m_sorted;
	
		int reserve_i(size_t capacity)
		{
			if (m_capacity < capacity)
			{
				Node* new_data = static_cast<Node*>(Allocator::allocate(capacity*sizeof(Node)));
				if (!new_data)
					return ERROR_OUTOFMEMORY;
				
#if defined(OOBASE_HAVE_EXCEPTIONS)
				size_t i = 0;
				try
				{
					for (i=0;i<m_size;++i)
						::new (&new_data[i]) Node(m_data[i].m_key,m_data[i].m_value);
				}
				catch (...)
				{
					for (;i>0;--i)
						new_data[i-1].~Node();

					Allocator::free(new_data);
					throw;
				}
				
				for (i=0;i<m_size;++i)
					m_data[i].~Node();
#else
				for (size_t i=0;i<m_size;++i)
				{
					::new (&new_data[i]) Node(m_data[i].m_key,m_data[i].m_value);
					m_data[i].~Node();
				}
#endif
				Allocator::free(m_data);
				m_data = new_data;
				m_capacity = capacity;
			}
			return 0;
		}

		template <typename K1>
		size_t find_i(K1 key, bool first) const
		{
			const Node* p = bsearch(key);

			// Scan for the first
			while (p && first && p > m_data && (p-1)->m_key == p->m_key)
				--p;

			return (p ? static_cast<size_t>(p - m_data) : npos);
		}
					
		template <typename K1>
		const Node* bsearch(K1 key) const
		{
			// Always sort first
			const_cast<Table*>(this)->sort();

			const Node* base = m_data;
			for (size_t span = m_size; span > 0; span /= 2) 
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
