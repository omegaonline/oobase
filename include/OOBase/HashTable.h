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
#include "Memory.h"

#include <string.h>

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
			return (r >> 16) | (r << (sizeof(size_t)*8 -16));
		}
	};

	template <typename T1, typename T2>
	struct Hash<Pair<T1,T2> >
	{
		static size_t hash(const Pair<T1,T2>& pair)
		{
			return Hash<T1>::hash(pair.first) ^ Hash<T2>::hash(pair.second);
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
		static size_t hash(const char* c, size_t len = size_t(-1))
		{
			if (len == size_t(-1))
				len = strlen(c);

			size_t hash = detail::FNV<sizeof(size_t)>::offset_bias;
			for (size_t i=0;i<len;++i)
			{
				hash ^= static_cast<unsigned char>(*c++);
				detail::FNV<sizeof(size_t)>::hash(hash);
			}
			return hash;
		}

		template <typename S>
		static size_t hash(const S& v)
		{
			return hash(v.c_str(),v.length());
		}
	};

	template <>
	struct Hash<const char*>
	{
		static size_t hash(const char* c, size_t len = size_t(-1))
		{
			return Hash<char*>::hash(c,len);
		}

		template <typename S>
		static size_t hash(const S& v)
		{
			return hash(v.c_str(),v.length());
		}
	};

#if defined(OOBASE_STRING_H_INCLUDED_)
	template <typename A>
	struct Hash<SharedString<A> >
	{
		template <typename S>
		static size_t hash(const S& v)
		{
			return Hash<const char*>::hash(v.c_str(),v.length());
		}

		static size_t hash(const char* c, size_t len = size_t(-1))
		{
			return Hash<const char*>::hash(c,len);
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
			HashTableNode(size_t h, const Pair<K,V>& item) : m_hash(h), m_data(item)
			{}

			static void inplace_copy(HashTableNode* p, size_t h, const Pair<K,V>& item)
			{
				::new (p) HashTableNode(h,item);
			}

			static void inplace_destroy(HashTableNode& p)
			{
				p.m_data.~Pair<K,V>();
			}

			size_t m_hash;
			Pair<K,V> m_data;
		};

		template <typename K, typename V>
		struct HashTableNode<K,V,true>
		{
			static void inplace_copy(void* p, size_t h, const Pair<K,V>& item)
			{
				static_cast<HashTableNode*>(p)->m_hash = h;
				static_cast<HashTableNode*>(p)->m_data = item;
			}

			static void inplace_destroy(HashTableNode&)
			{}

			size_t m_hash;
			Pair<K,V> m_data;
		};
	}

	template <typename K, typename V, typename Allocator = CrtAllocator, typename H = OOBase::Hash<K> >
	class HashTable : public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;

	protected:
		typedef detail::HashTableNode<K,V,detail::is_pod<detail::HashTable::PODCheck<K,V> >::value> Node;

	public:
		typedef detail::IteratorImpl<HashTable,Pair<K,V>,size_t> iterator;
		friend class detail::IteratorImpl<HashTable,Pair<K,V>,size_t>;
		typedef detail::IteratorImpl<const HashTable,const Pair<K,V>,size_t> const_iterator;
		friend class detail::IteratorImpl<const HashTable,const Pair<K,V>,size_t>;

		HashTable(const H& h = H()) : baseClass(), m_data(NULL), m_size(0), m_count(0), m_hash(h), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		HashTable(AllocatorInstance& allocator, const H& h = H()) : baseClass(allocator), m_data(NULL), m_size(0), m_count(0), m_hash(h), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		HashTable(const HashTable& rhs) : baseClass(rhs), m_data(NULL), m_size(0), m_count(0), m_hash(rhs.m_hash), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);

			for (size_t i=0;i<rhs.m_size;++i)
			{
				if (is_in_use(rhs.m_data[i].m_hash) && !insert(rhs.m_data[i].m_data))
					break;
			}
		}

		~HashTable()
		{
			for (size_t i=0;i<m_size && m_count;++i)
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

		iterator insert(typename call_traits<K>::param_type key, typename call_traits<V>::param_type value)
		{
			size_t pos = 0;
			Pair<K,V> temp_item(key,value);
			if (!insert_i(pos,temp_item))
				return m_end;

			return iterator(this,pos);
		}

		iterator insert(const Pair<K,V>& item)
		{
			return insert(item.first,item.second);
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			return (find_i(key) < m_size);
		}

		template <typename K1>
		const_iterator find(const K1& key) const
		{
			return const_iterator(this,find_i(key));
		}

		template <typename K1>
		iterator find(const K1& key)
		{
			return iterator(this,find_i(key));
		}

		template <typename K1>
		bool find(const K1& key, V& val) const
		{
			size_t pos = find_i(key);
			if (pos == size_t(-1))
				return false;

			val = m_data[pos].m_data.second;
			return true;
		}

		iterator erase(iterator iter)
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

			return iterator(this,pos);
		}

		template <typename K1>
		bool remove(const K1& key, V* value = NULL)
		{
			iterator i = find(key);
			if (i == m_end)
				return false;

			if (value)
				*value = i->second;

			erase(i);
			return true;
		}

		bool pop(K* key = NULL, V* value = NULL)
		{
			iterator i = begin();
			if (i == m_end)
				return false;

			if (key)
				*key = i->first;

			if (value)
				*value = i->second;

			erase(i);
			return true;
		}

		void clear()
		{
			for (size_t i=0;i<m_size && m_count;++i)
			{
				if (is_in_use(m_data[i].m_hash))
				{
					Node::inplace_destroy(m_data[i]);
					m_data[i].m_hash = 0;
				}
			}
			m_count = 0;
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

			return m_end;
		}

		const_iterator cbegin() const
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (is_in_use(m_data[i].m_hash))
					return const_iterator(this,i);
			}

			return m_cend;
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator end()
		{
			return m_end;
		}

		const_iterator cend() const
		{
			return m_cend;
		}

		const_iterator end() const
		{
			return m_cend;
		}

	private:
		Node*    m_data;
		size_t   m_size;
		size_t   m_count;
		H        m_hash;

		iterator m_end;
		const_iterator m_cend;

		static const size_t s_hi_bit = (size_t(1) << ((sizeof(size_t) * 8) - 1));

		template <typename K1>
		size_t hash_i(const K1& key) const
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

		template <typename K1>
		size_t find_i(const K1& key) const
		{
			if (m_count == 0)
				return size_t(-1);

			size_t h = hash_i(key);
			size_t pos = h & (m_size-1);
			for (size_t dist = 0;;++dist)
			{
				if (!m_data[pos].m_hash || dist > probe_distance(m_data[pos].m_hash,pos))
					break;

				if (m_data[pos].m_hash == h && m_data[pos].m_data.first == key)
					return pos;

				pos = (pos + 1) & (m_size-1);
			}

			return size_t(-1);
		}

		bool grow()
		{
			size_t new_size = (m_size == 0 ? 16 : m_size * 2);
			Node* new_data = static_cast<Node*>(baseClass::allocate(new_size * sizeof(Node),alignment_of<Node>::value));
			if (!new_data)
				return false;

			// Set nothing in use
			for (size_t i=0;i<new_size;++i)
				new_data[i].m_hash = 0;

			// Now insert the contents of m_data into copy
			size_t c = 0;
			for (size_t i=0;i<m_size && c<m_count;++i)
			{
				if (is_in_use(m_data[i].m_hash))
				{
					++c;
					size_t count = 0;
					insert_other(m_data[i].m_data,new_size,new_data,count);
				}
			}

			baseClass::free(m_data);
			m_data = new_data;
			m_size = new_size;

			return true;
		}

		size_t insert_other(Pair<K,V>& item, size_t size, Node* data, size_t& count)
		{
			size_t pos = 0;
			size_t h = hash_i(item.first);
			pos = h & (size-1);
			for (size_t dist = 0;;++dist)
			{
				if (!data[pos].m_hash)
				{
					Node::inplace_copy(&data[pos],h,item);
					++count;
					break;
				}

				if (data[pos].m_hash == h && data[pos].m_data.first == item.first)
				{
					// Replace existing
					Node::inplace_copy(&data[pos],h,item);
					break;
				}

				// If the existing element has probed less than us then swap, Robin Hood!
				size_t curr_dist = probe_distance(data[pos].m_hash,pos);
				if (curr_dist < dist)
				{
					if (is_deleted(data[pos].m_hash))
					{
						Node::inplace_copy(&data[pos],h,item);
						++count;
						break;
					}

					OOBase::swap(data[pos].m_hash,h);
					OOBase::swap(data[pos].m_data,item);
										
					dist = curr_dist;
				}

				pos = (pos + 1) & (size-1);
			}

			return pos;
		}

		bool insert_i(size_t& pos, Pair<K,V>& item)
		{
			if (m_count >= (m_size / 10 * 9))
			{
				if (!grow())
					return false;
			}

			pos = insert_other(item,m_size,m_data,m_count);
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

				Node::inplace_copy(&m_data[pos],m_data[next].m_hash,m_data[next].m_data);
				Node::inplace_destroy(m_data[next]);

				pos = next;
			}
			m_data[pos].m_hash |= s_hi_bit;
		}

		Pair<K,V>* at(size_t pos)
		{
			return (pos < m_size && is_in_use(m_data[pos].m_hash) ? &m_data[pos].m_data : NULL);
		}

		const Pair<K,V>* at(size_t pos) const
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

		void iterator_move(size_t& pos, ptrdiff_t n) const
		{
			for (ptrdiff_t i = 0;i < n && pos < m_size;++i)
				next(pos);
			for (ptrdiff_t i = n;i < 0 && pos < m_size;++i)
				prev(pos);
		}
	};
}

#endif // OOBASE_HASHTABLE_H_INCLUDED_
