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

#ifndef OOBASE_HASHCACHE_H_INCLUDED_
#define OOBASE_HASHCACHE_H_INCLUDED_

#include "HashTable.h"
#include "Table.h"

namespace OOBase
{
	template <typename K, typename V, typename Allocator, typename Container>
	class Cache
	{
	public:
		static const size_t npos = Container::npos;

		Cache(size_t size) : m_cache(NULL), m_size(size), m_clock(0)
		{}

		~Cache()
		{
			clear();
			Allocator::free(m_cache);
		}

		int insert(const K& key, const V& value)
		{
			if (!m_cache)
			{
				m_cache = static_cast<CacheEntry*>(Allocator::allocate(m_size*sizeof(CacheEntry)));
				if (!m_cache)
					return ERROR_OUTOFMEMORY;

				// Set nothing in use
				for (size_t i=0;i<m_size;++i)
				{
					m_cache[i].m_in_use = 0;
					m_cache[i].m_clk = 0;
				}
			}

			int err = m_table.insert(key,value);
			if (err != 0)
				return err;

			while (m_cache[m_clock].m_clk)
			{
				m_cache[m_clock].m_clk = 0;
				m_clock = (m_clock+1) % m_size;
			}

			if (m_cache[m_clock].m_in_use)
			{
				m_table.remove(key);
				m_cache[m_clock].~CacheEntry();
				m_cache[m_clock].m_in_use = 0;
			}

			::new (&m_cache[m_clock]) CacheEntry(key);

			return 0;
		}

		int replace(const K& key, const V& value)
		{
			V* v = m_table.find(key);
			if (!v)
				return insert(key,value);

			*v = value;
			return 0;
		}

		template <typename K1>
		bool find(K1 key, V& value) const
		{
			return m_table.find(key,value);
		}

		template <typename K1>
		V* find(K1 key)
		{
			return m_table.find(key);
		}

		void clear()
		{
			for (size_t i=0;i<m_size;++i)
			{
				if (m_cache[m_clock].m_in_use)
				{
					m_cache[m_clock].~CacheEntry();
					m_cache[i].m_in_use = 0;
				}
				m_cache[i].m_clk = 0;
			}
			m_table.clear();
		}

	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur...
		// and you probably don't want to be copying these around
		Cache(const Cache&);
		Cache& operator = (const Cache&);

		struct CacheEntry
		{
			K        m_key;
			unsigned m_in_use : 1;
			unsigned m_clk    : 1;

			CacheEntry(const K& k) : m_key(k), m_in_use(1), m_clk(1)
			{}
		};

		CacheEntry*  m_cache;
		const size_t m_size;
		size_t       m_clock;

	protected:
		Container    m_table;
	};

	template <typename K, typename V, typename Allocator = HeapAllocator>
	class TableCache : public Cache<K,V,Allocator,Table<K,V,Allocator> >
	{
	public:
		TableCache(size_t size) : Cache<K,V,Allocator,Table<K,V,Allocator> >(size)
		{}

		template <typename K1>
		size_t find_first(K1 key)
		{
			return this->m_table.find_first(key);
		}

		V* at(size_t pos)
		{
			return this->m_table.at(pos);
		}

		const V* at(size_t pos) const
		{
			return this->m_table.at(pos);
		}

		const K* key_at(size_t pos) const
		{
			return this->m_table.key_at(pos);
		}
	};

	template <typename K, typename V, typename Allocator = HeapAllocator, typename H = OOBase::Hash<K> >
	class HashCache : public Cache<K,V,Allocator,HashTable<K,V,Allocator,H> >
	{
	public:
		HashCache(size_t size) : Cache<K,V,Allocator,HashTable<K,V,Allocator,H> >(size)
		{}
	};
}

#endif // OOBASE_HASHCACHE_H_INCLUDED_
