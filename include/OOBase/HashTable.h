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
#if defined(_MSC_VER)
			typedef unsigned __int64 fnv_uint64;
#else
			typedef size_t fnv_uint64;
#endif
			static const fnv_uint64 offset_bias = 0xcbf29ce484222325ULL;

			static void hash(fnv_uint64& hval)
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
				hash ^= *c;
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
				hash ^= *c++;
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
			HashTableNode(const K1& k, const V1& v) : m_in_use(2), m_key(k), m_value(v)
			{}

			template <typename K1, typename V1>
			static void inplace_copy(void* p, const K1& k, const V1& v)
			{
				::new (p) HashTableNode(k,v);
			}

			int m_in_use;
			K m_key;
			V m_value;
		};

		template <typename K, typename V>
		struct HashTableNode<K,V,true>
		{
			template <typename K1, typename V1>
			static void inplace_copy(void* p, const K1& k, const V1& v)
			{
				static_cast<HashTableNode*>(p)->m_in_use = 2;
				static_cast<HashTableNode*>(p)->m_key = k;
				static_cast<HashTableNode*>(p)->m_value = v;
			}

			int m_in_use;
			K m_key;
			V m_value;
		};
	}

	template <typename K, typename V, typename Allocator = CrtAllocator, typename H = OOBase::Hash<K> >
	class HashTable : public Allocating<Allocator>, public NonCopyable
	{
		typedef Allocating<Allocator> baseClass;
		typedef detail::HashTableNode<K,V,detail::is_pod<detail::HashTable::PODCheck<K,V> >::value> Node;

	public:
		static const size_t npos = size_t(-1);

		HashTable(const H& h = H()) : baseClass(), m_data(NULL), m_size(0), m_count(0), m_hash(h), m_clone(false)
		{}

		HashTable(AllocatorInstance& allocator, const H& h = H()) : baseClass(allocator), m_data(NULL), m_size(0), m_count(0), m_hash(h), m_clone(false)
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

		void remove_at(size_t pos)
		{
			m_data[pos].~Node();
			m_data[pos].m_in_use = 1;
			--m_count;

			size_t next = (pos+1) & (m_size-1);
			if (m_data[next].m_in_use == 0)
				m_data[pos].m_in_use = 0;
		}

		bool remove(const K& key, V* value = NULL)
		{
			size_t pos = find_i(key);
			if (pos == npos)
				return false;
			
			if (value)
				*value = m_data[pos].m_value;
			
			remove_at(pos);
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

					remove_at(i);
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
		Node*    m_data;
		size_t   m_size;
		size_t   m_count;
		const H& m_hash;

		mutable bool m_clone;

		int clone(bool grow)
		{
			m_clone = false;

			size_t new_size = (m_size == 0 ? 16 : (grow ? m_size * 2 : m_size));
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
						err = insert_i(new_data,new_size,m_data[i].m_key,m_data[i].m_value);
					}
					catch (...)
					{
						destroy(new_data,new_size);
						throw;
					}
#else
					int err = insert_i(new_data,new_size,m_data[i].m_key,m_data[i].m_value);
#endif
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

		void destroy(Node* data, size_t size)
		{
			for (size_t i=0;i<size;++i)
			{
				if (data[i].m_in_use == 2)
					data[i].~Node();
			}
			baseClass::free(data);
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
					Node::inplace_copy(&data[h1],key,value);
					return 0;
				}

				if (data[h1].m_key == key)
					return EEXIST;
			}
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
