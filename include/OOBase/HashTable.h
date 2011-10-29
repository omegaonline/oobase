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

#ifndef OOBASE_HASHTABLE_H_INCLUDED_
#define OOBASE_HASHTABLE_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	template <typename T>
	struct Hash
	{
		static size_t hash(T v)
		{
			return v;
		}
	};

	template <typename T>
	struct Hash<T*>
	{
		static size_t hash(T* v)
		{
			size_t r = reinterpret_cast<size_t>(v);
			
			// Because pointers are commonly aligned
			return (r >> 4) | (r << (sizeof(size_t)*8 -4)); 
		}
	};
	
	template <typename K, typename V, typename Allocator = HeapAllocator, typename H = OOBase::Hash<K> >
	class HashTable
	{
	public:
		static const size_t npos = size_t(-1);

		HashTable(const H& h = H()) : m_data(NULL), m_size(0), m_count(0), m_hash(h), m_clone(false)
		{}
			
		~HashTable()
		{
			destroy(m_data,m_size);
		}

		int insert(const K& key, const V& value)
		{
			if (m_count+(m_size/7) >= m_size)
			{
				int err = clone(true);
				if (err != 0)
					return err;
			}
			else if (m_clone)
			{
				int err = clone(false);
				if (err != 0)
					return err;
			}
				
			int err = insert_i(m_data,m_size,key,value);
			if (err == 0)
				++m_count;
			
			return err;
		}

		int replace(const K& key, const V& value)
		{
			size_t pos = find_i(key);
			if (pos == npos)
				return insert(key,value);
			
			*at(pos) = value;
			return 0;
		}

		bool exists(const K& key) const
		{
			return (find_i(key) != npos);
		}
		
		bool find(const K& key, V& value) const
		{
			size_t pos = find_i(key);
			if (pos == npos)
				return false;
			
			value = m_data[pos].m_value;
			return true;
		}

		V* find(const K& key)
		{
			size_t pos = find_i(key);
			if (pos == npos)
				return NULL;
			
			return &m_data[pos].m_value;
		}
		
		bool erase(const K& key, V* value = NULL)
		{
			size_t pos = find_i(key);
			if (pos == npos)
				return false;
			
			if (value)
				*value = m_data[pos].m_value;
			
			del_node(pos);
			return true;
		}
		
		bool pop(K* key = NULL, V* value = NULL)
		{
			for (size_t i=0;i<m_size && m_count>0;++i)
			{
				if (m_data[i].m_in_use == 2)
				{
					if (key)
						*key = m_data[i].m_key;
					
					if (value)
						*value = m_data[i].m_value;
					
					del_node(i);
					return true;
				}
			}
			return false;
		}
		
		void clear()
		{
			for (size_t i=0;i<m_size && m_count>0;++i)
			{
				if (m_data[i].m_in_use == 2)
				{
					m_data[i].~Node();
					m_data[i].m_in_use = 0;
					--m_count;
				}
			}
		}
		
		bool empty() const
		{
			return (m_count == 0);
		}
		
		size_t size() const
		{
			return m_count;
		}

		size_t begin() const
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (m_data[i].m_in_use == 2)
					return i;
			}
			return npos;
		}

		size_t next(size_t pos) const
		{
			for (size_t i=pos+1;i<m_size;++i)
			{
				if (m_data[i].m_in_use == 2)
					return i;
			}
			return npos;
		}

		V* at(size_t pos)
		{
			return (m_data && pos < m_size && m_data[pos].m_in_use==2 ? &m_data[pos].m_value : NULL);
		}
		
		const V* at(size_t pos) const
		{
			return (m_data && pos < m_size && m_data[pos].m_in_use==2 ? &m_data[pos].m_value : NULL);
		}
		
		const K* key_at(size_t pos) const
		{
			return (m_data && pos < m_size && m_data[pos].m_in_use==2 ? &m_data[pos].m_key : NULL);
		}
		
	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur... 
		// and you probably don't want to be copying these around
		HashTable(const HashTable&);
		HashTable& operator = (const HashTable&);
	
		struct Node
		{
			Node(const K& k, const V& v) : m_in_use(2), m_key(k), m_value(v)
			{}
				
			int m_in_use;
			K   m_key;
			V   m_value;
		};
				
		Node*    m_data;
		size_t   m_size;
		size_t   m_count;
		const H& m_hash;

		mutable bool m_clone;

		void del_node(size_t pos)
		{
			m_data[pos].~Node();
			m_data[pos].m_in_use = 1;
			--m_count;

			size_t next = (pos+1) & (m_size-1);
			if (m_data[next].m_in_use == 0)
				m_data[pos].m_in_use = 0;
		}
		
		int clone(bool grow)
		{
			m_clone = false;

			size_t new_size = (m_size == 0 ? 16 : (grow ? m_size * 2 : m_size));
			Node* new_data = static_cast<Node*>(Allocator::allocate(new_size*sizeof(Node)));
			if (!new_data)
				return ERROR_OUTOFMEMORY;
			
			// Set nothing in use
			for (size_t i=0;i<new_size;++i)
				new_data[i].m_in_use = 0;
				
			// Now reinsert the contents of m_data into new_data
			for (size_t i=0;i<m_size;++i)
			{
				if (m_data[i].m_in_use == 2)
				{
					int err = insert_i(new_data,new_size,m_data[i].m_key,m_data[i].m_value);
					if (err != 0)
					{
						destroy(new_data,new_size);
						return err;
					}
				}
			}
			
			destroy(m_data,m_size);
			m_data = new_data;
			m_size = new_size;
			
			return 0;
		}
		
		static void destroy(Node* data, size_t size)
		{
			for (size_t i=0;i<size;++i)
			{
				if (data[i].m_in_use == 2)
					data[i].~Node();
			}
			Allocator::free(data);
		}

		size_t find_i(const K& key) const
		{
			if (m_count == 0)
				return npos;
			
			size_t start = m_hash.hash(key);
			for (size_t h = start;h < start + m_size;++h)
			{
				size_t h1 = h & (m_size-1);
				
				if (m_data[h1].m_in_use == 0)
					return npos;
				
				if (m_data[h1].m_in_use == 2 && m_data[h1].m_key == key)
					return h1;
			}

			m_clone = true;
			return npos;
		}
		
		int insert_i(Node* data, size_t size, const K& key, const V& value)
		{
			for (size_t h = m_hash.hash(key);;++h)
			{
				size_t h1 = h & (size-1);
				
				if (data[h1].m_in_use != 2)
				{
					// Placement new
					::new (&data[h1]) Node(key,value);
					return 0;
				}
				
				if (data[h1].m_key == key)
					return EEXIST;
			}
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
