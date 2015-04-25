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

#include "Iterator.h"

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
			template <typename V1>
			HashTableNode(size_t h, const K& k, V1 v) : m_hash(h), m_data(k,v)
			{}

			template <typename V1>
			static void inplace_copy(HashTableNode* p, size_t h, const K& k, V1 v)
			{
				::new (p) HashTableNode(h,k,v);
			}

			static void inplace_destroy(HashTableNode& p)
			{
				p.m_data.~pair();
			}

			size_t m_hash;
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
			template <typename V1>
			static void inplace_copy(void* p, size_t h, const K& k, V1 v)
			{
				static_cast<HashTableNode*>(p)->m_hash = h;
				static_cast<HashTableNode*>(p)->m_data.key = k;
				static_cast<HashTableNode*>(p)->m_data.value = v;
			}

			static void inplace_destroy(HashTableNode&)
			{}

			size_t m_hash;
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

	protected:
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
			for (size_t i=0;i<m_size;++i)
			{
				if (is_in_use(m_data[i].m_hash))
					Node::inplace_destroy(m_data[i]);
			}
			baseClass::free(m_data);
		}

		HashTable& operator = (const HashTable& rhs)
		{
			HashTable(rhs).swap(*this);
			return *this;
		}

		void swap(HashTable& rhs)
		{
			baseClass::swap(rhs);
			OOBase::swap(m_data,rhs.m_data);
			OOBase::swap(m_size,rhs.m_size);
			OOBase::swap(m_count,rhs.m_count);
			OOBase::swap(m_hash,rhs.m_hash);
		}

		iterator insert(const K& key, const V& value)
		{
			size_t pos = 0;
			if (!insert_i(pos,key,value))
				return end();

			++m_count;
			return iterator(this,pos);
		}

		bool exists(const K& key) const
		{
			return (find_i(key) < m_size);
		}

		const_iterator find(const K& key) const
		{
			return const_iterator(this,find_i(key));
		}

		iterator find(const K& key)
		{
			return iterator(this,find_i(key));
		}

		void remove_at(iterator& iter)
		{
			assert(iter.check(this));
			size_t pos = iter.deref();

			if (is_in_use(m_data[pos].m_hash))
			{
				Node::inplace_destroy(m_data[pos]);
				--m_count;

				ripple(pos);
			}

			while (pos < m_size && !is_in_use(m_data[pos].m_hash))
				++pos;

			if (pos >= m_size)
				pos = size_t(-1);

			iter = iterator(this,pos);
		}

		bool remove(const K& key, V* value = NULL)
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
				if (is_in_use(m_data[i].m_hash))
				{
					Node::inplace_destroy(m_data[i]);
					m_data[i].m_hash = 0;
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
				if (is_in_use(m_data[i].m_hash))
					return iterator(this,i);
			}

			return end();
		}

		const_iterator cbegin() const
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (is_in_use(m_data[i].m_hash))
					return iterator(this,i);
			}

			return end();
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator end()
		{
			return iterator(this,size_t(-1));
		}

		const_iterator cend() const
		{
			return const_iterator(this,size_t(-1));
		}

		const_iterator end() const
		{
			return cend();
		}

	private:
		Node*    m_data;
		size_t   m_size;
		size_t   m_count;
		H        m_hash;

		static const size_t s_hi_bit = (size_t(1) << ((sizeof(size_t) * 8) - 1));

		HashTable(const HashTable& h, Node* data, size_t size) : baseClass(h), m_data(data), m_size(size), m_count(0), m_hash(h.m_hash)
		{
			// Set nothing in use
			for (size_t i=0;i<m_size;++i)
				m_data[i].m_hash = 0;
		}

		size_t hash_i(const K& key) const
		{
			size_t h = m_hash.hash(key);

			// Clear top bit
			h &= ~s_hi_bit;

			// Never use 0 as it is used as marker
			if (!h)
				h = 1;

			return h;
		}

		size_t desired_pos(size_t hash) const
		{
			return hash & (m_size-1);
		}

		static bool is_deleted(size_t hash)
		{
			// Top bit is used as a tombstone
			return (hash & s_hi_bit) != 0;
		}

		static bool is_in_use(size_t hash)
		{
			return hash != 0 && !is_deleted(hash);
		}

		size_t probe_distance(size_t hash, size_t pos) const
		{
			return (pos + m_size - (hash & (m_size-1))) & (m_size-1);
		}

		size_t find_i(const K& key) const
		{
			if (m_count == 0)
				return size_t(-1);

			size_t h = hash_i(key);
			size_t pos = h & (m_size-1);
			for (size_t dist = 0;;++dist)
			{
				if (!m_data[pos].m_hash || dist > probe_distance(m_data[pos].m_hash,pos))
					break;

				if (m_data[pos].m_hash == h && m_data[pos].m_data.key == key)
					return pos;

				pos = (pos + 1) & (m_size-1);
			}

			return size_t(-1);
		}

		bool clone()
		{
			size_t new_size = (m_size == 0 ? 16 : m_size * 2);
			Node* new_data = static_cast<Node*>(baseClass::allocate(new_size * sizeof(Node),alignment_of<Node>::value));
			if (!new_data)
				return false;

			HashTable copy(*this,new_data,new_size);

			// Now insert the contents of m_data into copy
			for (size_t i=0;i<m_size;++i)
			{
				if (is_in_use(m_data[i].m_hash))
				{
					if (copy.insert(m_data[i].m_data.key,m_data[i].m_data.value) == copy.end())
						return false;
				}
			}

			// Swap ourselves with the copy
			swap(copy);
			return true;
		}

		bool insert_i(size_t& pos, K key, V value)
		{
			if (m_count >= (m_size / 10 * 9))
			{
				if (!clone())
					return false;
			}

			size_t h = hash_i(key);
			pos = h & (m_size-1);
			for (size_t dist = 0;;++dist)
			{
				if (!m_data[pos].m_hash)
				{
					Node::inplace_copy(&m_data[pos],h,key,value);
					++m_count;
					break;
				}

				if (m_data[pos].m_hash == h && m_data[pos].m_data.key == key)
				{
					Node::inplace_copy(&m_data[pos],h,key,value);
					break;
				}

				// If the existing element has probed less than us then swap, Robin Hood!
				size_t curr_dist = probe_distance(m_data[pos].m_hash,pos);
				if (curr_dist < dist)
				{
					if (is_deleted(m_data[pos].m_hash))
					{
						Node::inplace_copy(&m_data[pos],h,key,value);
						++m_count;
						break;
					}

					OOBase::swap(m_data[pos].m_hash,h);
					OOBase::swap(m_data[pos].m_data.key,key);
					OOBase::swap(m_data[pos].m_data.value,value);

					dist = curr_dist;
				}

				pos = (pos + 1) & (m_size-1);
			}

			return true;
		}

		void ripple(size_t start)
		{
			size_t pos = start;
			for (int i=0;i < 3;++i)
			{
				size_t next = (pos & (m_size-1));
				if (next == start)
					break;

				if (!m_data[next].m_hash || probe_distance(m_data[next].m_hash,next) == 0)
				{
					m_data[pos].m_hash = 0;
					return;
				}

				Node::inplace_copy(&m_data[pos],m_data[next].m_hash,m_data[next].m_data.key,m_data[next].m_data.value);
				Node::inplace_destroy(m_data[next]);

				pos = next;
			}
			m_data[pos].m_hash |= s_hi_bit;
		}

		typename Node::pair* at(size_t pos)
		{
			return (pos < m_size && is_in_use(m_data[pos].m_hash) ? &m_data[pos].m_data : NULL);
		}

		const typename Node::pair* at(size_t pos) const
		{
			return (pos < m_size && is_in_use(m_data[pos].m_hash) ? &m_data[pos].m_data : NULL);
		}

		void next(size_t& pos) const
		{
			while (++pos < m_size && !is_in_use(m_data[pos].m_hash))
				;

			if (pos >= m_size)
				pos = size_t(-1);
		}

		void prev(size_t& pos) const
		{
			if (pos > m_size)
				pos = m_size;

			while (pos-- > 0 && !is_in_use(m_data[pos].m_hash))
				;
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
