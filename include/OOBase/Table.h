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

// Because I don't want to #include <stdlib.h> in a header
extern "C" void *bsearch(const void *key, const void *base, size_t nmemb,size_t size, int (*compar)(const void *, const void *));

namespace OOBase
{
	template <typename K, typename V, typename Allocator = HeapAllocator>
	class Table
	{
	public:
		static const size_t npos = size_t(-1);

		Table() : m_data(NULL), m_capacity(0), m_size(0)
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
		
		int insert(const K& key, const V& value, bool do_sort = true)
		{
			if (m_size+1 > m_capacity)
			{
				size_t cap = (m_capacity == 0 ? 4 : m_capacity*2);
				int err = reserve_i(cap);
				if (err != 0)
					return err;
			}
			
			// Placement new
			new (&m_data[m_size]) Node(key,value);
			
			// This is exception safe as the increment happens here
			++m_size;
			
			if (do_sort)
				sort();
			
			return 0;
		}
		
		void erase(size_t pos)
		{
			if (m_data && pos < m_size)
			{
				for(--m_size;pos < m_size;++pos)
					m_data[pos] = m_data[pos+1];
				
				m_data[pos].~Node();
			}
		}
		
		bool erase(const K& key, V* value = NULL)
		{
			size_t pos = find(key);
			if (pos == npos)
				return false;
			
			if (value)
				*value = m_data[pos].m_value;
			
			erase(pos);
			
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

		template <typename T>
		size_t find(T key, bool first = false) const
		{
			const Node* p = static_cast<Node*>(::bsearch(&key,m_data,m_size,sizeof(Node),&compare<T>));
			
			// Scan for the first
			while (first && p > m_data && (p-1)->m_key == p->m_key)
				--p;

			return (p ? static_cast<size_t>(p - m_data) : npos);
		}
				
		template <typename T>
		bool find(T key, V& value, bool first = false) const
		{
			size_t pos = find(key,first);
			if (pos == npos)
				return false;
			
			value = m_data[pos].m_value;
			return true;
		}

		int replace(const K& key, const V& value)
		{
			size_t pos = find(key);
			if (pos == npos)
				return  insert(key,value);
			
			*at(pos) = value;
			return 0;
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
					
					while (j >= h && v.m_key < m_data[j-h].m_key)
					{
						m_data[j] = m_data[j-h];
						j -= h;
					}
					
					m_data[j] = v;
				}
			}
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
	
		int reserve_i(size_t capacity)
		{
			if (m_capacity < capacity)
			{
				Node* new_data = static_cast<Node*>(Allocator::allocate(capacity*sizeof(Node)));
				if (!new_data)
					return ERROR_OUTOFMEMORY;
				
				for (size_t i=0;i<m_size;++i)
					new (&new_data[i]) Node(m_data[i].m_key,m_data[i].m_value);
				
				for (size_t i=0;i<m_size;++i)
					m_data[i].~Node();
										
				Allocator::free(m_data);
				m_data = new_data;
				m_capacity = capacity;
			}
			return 0;
		}
					
		template <typename P>
		static int compare(const void* pa, const void* pb)
		{
			const P& a = *static_cast<const P*>(pa);
			const K& b = static_cast<const Node*>(pb)->m_key;
			
			if (b == a)
				return 0;
			if (b > a)
				return -1;
			else 
				return 1;
		}
	};
}

#endif // OOBASE_TABLE_H_INCLUDED_
