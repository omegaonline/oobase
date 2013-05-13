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
	
	namespace detail
	{
		// See http://isthe.com/chongo/tech/comp/fnv/ for details
		template <size_t d>
		struct FNV;

		template <>
		struct FNV<4>
		{
			static const size_t offset_bias = 0x811c9dc5;

			static void hash(size_t& hval)
			{
				//hval *= 0x01000193;
				hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
			}
		};

		template <>
		struct FNV<8>
		{
			static const uint64_t offset_bias = 0xcbf29ce484222325ULL;

			static void hash(uint64_t& hval)
			{
				//hval *= 0x100000001b3ULL;
				hval += (hval << 1) + (hval << 4) + (hval << 5) +
						(hval << 7) + (hval << 8) + (hval << 40);
			}
		};
	}

	template <>
	struct Hash<char*>
	{
		static size_t hash(const char* c)
		{
			size_t hash = detail::FNV<sizeof(size_t)>::offset_bias;
			do
			{
				hash ^= static_cast<unsigned char>(*c);
				detail::FNV<sizeof(size_t)>::hash(hash);
			}
			while (*c++ != '\0');
			return hash;
		}

		static size_t hash(const char* c, size_t len)
		{
			size_t hash = detail::FNV<sizeof(size_t)>::offset_bias;
			for (size_t i=0;i<len;++i)
			{
				hash ^= static_cast<unsigned char>(*c++);
				detail::FNV<sizeof(size_t)>::hash(hash);
			}
			return hash;
		}
	};

	template <>
	struct Hash<const char*>
	{
		static size_t hash(const char* c)
		{
			return Hash<char*>::hash(c);
		}

		static size_t hash(const char* c, size_t len)
		{
			return Hash<char*>::hash(c,len);
		}
	};

#if defined(OOBASE_STRING_H_INCLUDED_)
	template <>
	struct Hash<String>
	{
		static size_t hash(const String& v)
		{
			return Hash<const char*>::hash(v.c_str());
		}
	};

	template <>
	struct Hash<LocalString>
	{
		static size_t hash(const LocalString& v)
		{
			return Hash<const char*>::hash(v.c_str());
		}
	};
#endif

	namespace detail
	{
		namespace HashTable
		{
			template <typename K, typename V>
			struct PODCheck
			{
				K k;
				V v;
			};
		}

		template <typename K, typename V, bool POD = false>
		struct HashTableNode
		{
			template <typename K1, typename V1>
			HashTableNode(const K1& k, const V1& v) : m_in_use(2), m_data(k,v)
			{}

			template <typename K1, typename V1>
			static void inplace_copy(void* p, const K1& k, const V1& v)
			{
				::new (p) HashTableNode(k,v);
			}

			int m_in_use;
			struct pair
			{
				pair(K k, V v) : key(k), value(v)
				{}

				K key;
				V value;
			} m_data;
		};

		template <typename K, typename V>
		struct HashTableNode<K,V,true>
		{
			template <typename K1, typename V1>
			static void inplace_copy(void* p, const K1& k, const V1& v)
			{
				static_cast<HashTableNode*>(p)->m_in_use = 2;
				static_cast<HashTableNode*>(p)->m_data.key = k;
				static_cast<HashTableNode*>(p)->m_data.value = v;
			}

			int m_in_use;
			struct pair
			{
				K key;
				V value;
			} m_data;
		};
	}

	template <typename K, typename V, typename Allocator = CrtAllocator, typename H = OOBase::Hash<K> >
	class HashTable : public Allocating<Allocator>, public NonCopyable
	{
		typedef Allocating<Allocator> baseClass;
		typedef detail::HashTableNode<K,V,detail::is_pod<detail::HashTable::PODCheck<K,V> >::value> Node;

	public:
		typedef detail::IteratorImpl<HashTable,typename Node::pair,size_t> iterator;
		typedef detail::IteratorImpl<const HashTable,const typename Node::pair,size_t> const_iterator;

		friend class detail::IteratorImpl<HashTable,typename Node::pair,size_t>;
		friend class detail::IteratorImpl<const HashTable,const typename Node::pair,size_t>;

		HashTable(const H& h = H()) : baseClass(), m_data(NULL), m_size(0), m_count(0), m_hash(h)
		{}

		HashTable(AllocatorInstance& allocator, const H& h = H()) : baseClass(allocator), m_data(NULL), m_size(0), m_count(0), m_hash(h)
		{}

		~HashTable()
		{
			destroy(m_data,m_size);
		}

		template <typename K1, typename V1>
		iterator insert(K1 key, V1 value, int& err)
		{
			if (m_count+(m_size/7) >= m_size)
			{
				if ((err = clone()))
					return end();
			}

			size_t pos = 0;
			err = insert_i(m_data,m_size,pos,key,value,false);
			if (err)
				return end();

			++m_count;
			return iterator(this,pos);
		}

		template <typename K1, typename V1>
		int insert(K1 key, V1 value)
		{
			int err = 0;
			insert(key,value,err);
			return err;
		}

		template <typename K1, typename V1>
		iterator replace(K1 key, V1 value, int& err)
		{
			if (m_count+(m_size/7) >= m_size)
			{
				if ((err = clone()))
					return end();
			}

			size_t pos = 0;
			err = insert_i(m_data,m_size,pos,key,value,true);
			if (err)
				return end();

			++m_count;
			return iterator(this,pos);
		}

		template <typename K1>
		bool exists(K1 key) const
		{
			return (find_i(key) < m_size);
		}

		template <typename K1>
		const_iterator find(K1 key) const
		{
			return const_iterator(this,find_i(key));
		}

		template <typename K1>
		iterator find(K1 key)
		{
			return iterator(this,find_i(key));
		}

		void remove_at(iterator& iter)
		{
			assert(iter.check(this));
			size_t pos = iter.deref();

			if (m_data[pos].m_in_use == 2)
			{
				m_data[pos].~Node();
				m_data[pos].m_in_use = 1;
				--m_count;

				size_t next = (pos+1) & (m_size-1);
				if (m_data[next].m_in_use == 0)
					m_data[pos].m_in_use = 0;
			}

			while (pos < m_size && m_data[pos].m_in_use != 2)
				++pos;

			iter = iterator(this,pos);
		}

		template <typename K1>
		bool remove(K1 key, V* value = NULL)
		{
			iterator i = find(key);
			if (i == end())
				return false;
			
			if (value)
				*value = i->value;
			
			remove_at(i);
			return true;
		}

		bool pop(K* key = NULL, V* value = NULL)
		{
			iterator i = begin();
			if (i == end())
				return false;

			if (key)
				*key = i->key;

			if (value)
				*value = i->value;

			remove_at(i);
			return true;
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

		iterator begin()
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (m_data[i].m_in_use == 2)
					return iterator(this,i);
			}

			return end();
		}

		const_iterator begin() const
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (m_data[i].m_in_use == 2)
					return iterator(this,i);
			}

			return end();
		}

		iterator end()
		{
			return iterator(this,m_size);
		}

		const_iterator end() const
		{
			return const_iterator(this,m_size);
		}

	private:
		Node*    m_data;
		size_t   m_size;
		size_t   m_count;
		const H& m_hash;

		int clone()
		{
			size_t new_size = (m_size == 0 ? 16 : m_size * 2);
			Node* new_data = static_cast<Node*>(baseClass::allocate(new_size * sizeof(Node),alignment_of<Node>::value));
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
#if defined(OOBASE_HAVE_EXCEPTIONS)
					int err = 0;
					try
					{
						size_t pos;
						err = insert_i(new_data,new_size,pos,m_data[i].m_data.key,m_data[i].m_data.value,false);
					}
					catch (...)
					{
						destroy(new_data,new_size);
						throw;
					}
#else
					size_t pos;
					int err = insert_i(new_data,new_size,pos,m_data[i].m_data.key,m_data[i].m_data.value,false);
#endif
					if (err)
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

		void destroy(Node* data, size_t size)
		{
			for (size_t i=0;i<size;++i)
			{
				if (data[i].m_in_use == 2)
					data[i].~Node();
			}
			baseClass::free(data);
		}

		template <typename K1>
		size_t find_i(K1 key) const
		{
			if (m_count == 0)
				return m_size;

			size_t start = m_hash.hash(key);
			for (size_t h = start;h < start + m_size;++h)
			{
				size_t h1 = h & (m_size-1);

				if (m_data[h1].m_in_use == 0)
					return m_size;
				
				if (m_data[h1].m_in_use == 2 && m_data[h1].m_data.key == key)
					return h1;
			}

			return m_size;
		}

		int insert_i(Node* data, size_t size, size_t& pos, const K& key, const V& value, bool replace)
		{
			for (size_t h = m_hash.hash(key);;++h)
			{
				pos = h & (size-1);

				if (data[pos].m_in_use != 2)
				{
					Node::inplace_copy(&data[pos],key,value);
					break;
				}

				if (data[pos].m_data.key == key)
				{
					if (!replace)
						return EEXIST;

					Node::inplace_copy(&data[pos],key,value);
					break;
				}
			}

			return 0;
		}

		typename Node::pair* at(size_t pos)
		{
			return (pos < m_size && m_data[pos].m_in_use == 2 ? &m_data[pos].m_data : NULL);
		}

		const typename Node::pair* at(size_t pos) const
		{
			return (pos < m_size && m_data[pos].m_in_use == 2 ? &m_data[pos].m_data : NULL);
		}

		void next(size_t& pos) const
		{
			while (pos < m_size && m_data[pos].m_in_use != 2)
				++pos;
		}

		void prev(size_t& pos) const
		{
			while (pos > 0 && m_data[pos].m_in_use != 2)
				--pos;

			if (m_data[pos].m_in_use != 2)
				pos = m_size;
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
