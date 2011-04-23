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

#include "../config-base.h"

namespace OOBase
{
	template <typename T>
	struct Hash
	{
		static size_t hash(T v)
		{
			static_assert(false,"You need to implement Hash() for your type");
			return 0;
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
	
	template <>
	struct Hash<size_t>
	{
		static size_t hash(size_t v)
		{
			return v;
		}
	};
	
	template <typename K, typename V, typename Allocator, typename H = OOBase::Hash<K> >
	class HashTable
	{
	public:
		HashTable() : m_data(NULL), m_size(0), m_count(0)
		{}
			
		~HashTable()
		{
			destroy(m_data,m_size);
		}
			
		int insert(const K& key, const V& value)
		{
			if (m_count+(m_size/8) >= m_size)
			{
				int err = grow();
				if (err != 0)
					return err;
			}
				
			int err = insert_i(m_data,m_size,key,value);
			if (err == 0)
				++m_count;
			
			return err;
		}
		
		bool find(const K& key, V& value) const
		{
			size_t pos = find(key);
			if (pos == size_t(-1))
				return false;
			
			value = m_data[pos].m_value;
			return true;
		}

		bool find(const K& key, V*& value)
		{
			size_t pos = find(key);
			if (pos == size_t(-1))
				return false;
			
			value = &m_data[pos].m_value;
			return true;
		}
		
		bool erase(const K& key, V* value = NULL)
		{
			size_t pos = find(key);
			if (pos == size_t(-1))
				return false;
			
			if (value)
				*value = m_data[pos].m_value;
			
			m_data[pos].~Node();
			--m_count;
			return true;
		}
		
		bool pop(K* key = NULL, V* value = NULL)
		{
			for (size_t i=0;i<m_size && m_count>0;++i)
			{
				if (m_data[i].m_in_use)
				{
					if (key)
						*key = m_data[i].m_key;
					
					if (value)
						*value = m_data[i].m_value;
					
					m_data[i].~Node();
					--m_count;
					return true;
				}
			}
			return false;
		}
		
		void clear()
		{
			for (size_t i=0;i<m_size && m_count>0;++i)
			{
				if (m_data[i].m_in_use)
				{
					m_data[i].~Node();
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
		
	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur... 
		// and you probably don't want to be copying these around
		HashTable(const HashTable&);
		HashTable& operator = (const HashTable&);
	
		struct Node
		{
			Node(const K& k, const V& v) : m_in_use(true), m_key(k), m_value(v)
			{}
				
			~Node() 
			{
				m_in_use = false;
			}
			
			bool m_in_use;
			K    m_key;
			V    m_value;
		};
				
		Node*  m_data;
		size_t m_size;
		size_t m_count;
		
		int grow()
		{
			size_t new_size = (m_size == 0 ? 16 : m_size * 2);
			Node* new_data = static_cast<Node*>(Allocator::allocate(new_size*sizeof(Node)));
			if (!new_data)
				return ERROR_OUTOFMEMORY;
			
			// Set nothing in use
			for (size_t i=0;i<new_size;++i)
				new_data[i].m_in_use = false;
				
			// Now reinsert the contents of m_data into new_data
			for (size_t i=0;i<m_size;++i)
			{
				if (m_data[i].m_in_use)
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
				if (data[i].m_in_use)
					data[i].~Node();
			}
			Allocator::free(data);
		}
		
		void rehash(size_t& h) const
		{
			h += (m_size/4) + 1;
		}
		
		size_t find(const K& key) const
		{
			if (m_count == 0)
				return size_t(-1);
			
			for (size_t h = H::hash(key);;rehash(h))
			{
				size_t h1 = h & (m_size-1);
				
				if (!m_data[h1].m_in_use)
					return size_t(-1);
				
				if (m_data[h1].m_key == key)
					return h1;
			}
		}
		
		int insert_i(Node* data, size_t size, const K& key, const V& value)
		{
			for (size_t h = H::hash(key);;rehash(h))
			{
				size_t h1 = h & (size-1);
				
				if (!data[h1].m_in_use)
				{
					// Placement new
					new (&data[h1]) Node(key,value);
					return 0;
				}
				
				if (data[h1].m_key == key)
					return EEXIST;
			}
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
