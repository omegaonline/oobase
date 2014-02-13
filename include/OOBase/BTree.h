///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2014 Rick Taylor
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

#ifndef OOBASE_BTREE_H_INCLUDED_
#define OOBASE_BTREE_H_INCLUDED_

#include "Table.h"
#include "List.h"
#include "SharedPtr.h"

namespace OOBase
{
	namespace detail
	{
		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeImpl;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPage
		{
		public:
			BTreeInternalPage(BTreeInternalPage* parent, BTreeLeafPage<K,V,Compare,B,Allocator>* leaf) :
					m_parent(parent), m_leafs(true), m_key_count(0)
			{
				m_pages[0] = leaf;
			}

			BTreeInternalPage(BTreeInternalPage* parent, BTreeInternalPage* internal) :
					m_parent(parent), m_leafs(false), m_key_count(0)
			{
				m_pages[0] = internal;
			}

			void destroy(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				for (size_t pos = 0; pos < m_key_count + 1; ++pos)
				{
					if (m_leafs)
						static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[pos])->destroy(tree);
					else
						static_cast<BTreeInternalPage*>(m_pages[pos])->destroy(tree);
				}
				tree->destroy_internal(this);
			}

			bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = find_page(value.first,tree->m_compare);
				if (m_leafs)
					return static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[page])->insert(tree,value);
				else
					return static_cast<BTreeInternalPage*>(m_pages[page])->insert(tree,value);
			}

			bool insert_page(BTreeImpl<K,V,Compare,B,Allocator>* tree, void* page, const K& key)
			{
				if (m_key_count == B - 1)
					return split(tree) && m_parent->insert_page(tree,page,key);

				size_t pos = find_page(key,tree->m_compare);
				for (size_t i = m_key_count;i > pos;--i)
				{
					OOBase::swap(m_pages[i+1],m_pages[i]);
					OOBase::swap(m_keys[i],m_keys[i-1]);
				}
				m_pages[pos+1] = page;
				m_keys[pos] = key;
				++m_key_count;
				return true;
			}

			template <typename K1>
			const Pair<K,V>* find(const K1& key, const Compare& compare) const
			{
				size_t page = find_page(key,compare);
				if (m_leafs)
					return static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[page])->find(key,compare);
				else
					return static_cast<BTreeInternalPage*>(m_pages[page])->find(key,compare);
			}

		private:
			BTreeInternalPage* m_parent;
			bool m_leafs;
			size_t m_key_count;

			K m_keys[B-1];
			void* m_pages[B];

			bool need_merge() const
			{
				return m_key_count == (m_parent ? (B/2 + B%2) : 2);
			}

			template <typename K1>
			size_t find_page(const K1& key, const Compare& compare) const
			{
				size_t start = 0;
				for (size_t end = m_key_count;start < end;)
				{
					size_t mid = start + (end - start) / 2;
					if (compare(m_keys[mid],key))
						start = mid + 1;
					else
						end = mid;
				}
				return start;
			}

			bool split(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				BTreeInternalPage* page = tree->new_internal(m_parent,static_cast<BTreeInternalPage*>(NULL));
				if (!page)
					return false;

				if (!m_parent)
				{
					if (!(m_parent = tree->new_internal(NULL,this)))
					{
						page->destroy(tree);
						return false;
					}
					page->m_parent = m_parent;
					tree->m_root_page = m_parent;
				}

				size_t half = m_key_count/2;
				for (size_t pos = half; pos < m_key_count; ++pos)
				{
					OOBase::swap(page->m_keys[page->m_key_count],m_keys[pos]);
					OOBase::swap(page->m_pages[page->m_key_count],m_pages[pos]);
					++page->m_key_count;
				}
				OOBase::swap(page->m_pages[page->m_key_count],m_pages[m_key_count]);
				m_key_count = half;

				return m_parent->insert_page(tree,page,page->m_keys[0]);
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage
		{
			friend class BTreeImpl<K,V,Compare,B,Allocator>;

		public:
			BTreeLeafPage(BTreeInternalPage<K,V,Compare,B,Allocator>* parent, BTreeLeafPage* prev, BTreeLeafPage* next) :
					m_parent(parent), m_prev(prev), m_next(next), m_size(0)
			{}

			void destroy(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				if (m_prev)
					m_prev->m_next = m_next;
				else
					tree->m_head = m_next;

				if (m_next)
					m_next->m_prev = m_prev;
				else
					tree->m_tail = m_prev;

				tree->destroy_leaf(this);
			}

			bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				if (m_size == B - 1)
					return split(tree) && tree->insert(value);

				size_t pos = 0;
				for (size_t end = m_size;pos < end;)
				{
					size_t mid = pos + (end - pos) / 2;
					if (tree->m_compare(m_data[mid].first,value.first))
						pos = mid + 1;
					else
						end = mid;
				}

				for (size_t i = m_size;i > pos;--i)
					OOBase::swap(m_data[i],m_data[i-1]);
				m_data[pos] = value;
				++m_size;
				return true;
			}

			template <typename K1>
			const Pair<K,V>* find(const K1& key, const Compare& compare) const
			{
				const Pair<K,V>* base = m_data;
				const Pair<K,V>* mid_point = NULL;
				for (size_t span = m_size; span > 0; span /= 2)
				{
					mid_point = base + (span / 2);
					if (compare(mid_point->first,key))
					{
						base = mid_point + 1;
						--span;
					}
					else if (mid_point->first == key)
						return mid_point;
				}
				return NULL;
			}

			bool need_merge() const
			{
				return m_size == (m_parent ? B/2 : 0);
			}

		private:
			BTreeInternalPage<K,V,Compare,B,Allocator>* m_parent;
			BTreeLeafPage* m_prev;
			BTreeLeafPage* m_next;

			size_t m_size;
			Pair<K,V> m_data[B-1];

			BTreeLeafPage* insert_leaf(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				BTreeLeafPage* leaf = tree->new_leaf(m_parent,m_prev,this);
				if (leaf)
				{
					if (!m_prev)
						tree->m_head = leaf;
					else
						m_prev->m_next = leaf;
					m_prev = leaf;
				}
				return leaf;
			}

			bool split(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				BTreeLeafPage* leaf = insert_leaf(tree);
				if (!leaf)
					return false;

				if (!m_parent)
				{
					if (!(m_parent = tree->new_internal(NULL,this)))
					{
						leaf->destroy(tree);
						return false;
					}
					leaf->m_parent = m_parent;
					tree->m_root_page = m_parent;
				}

				size_t half = m_size/2 + m_size%2;
				for (size_t pos = half; pos < m_size; ++pos)
					OOBase::swap(leaf->m_data[leaf->m_size++],m_data[pos]);
				m_size = half;

				return m_parent->insert_page(tree,leaf,leaf->m_data[0].first);
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeBase
		{
			typedef BTreeLeafPage<K,V,Compare,B,Allocator> leaf_page_t;
			typedef BTreeInternalPage<K,V,Compare,B,Allocator> internal_page_t;

		public:
			leaf_page_t* new_leaf(internal_page_t* parent, leaf_page_t* prev, leaf_page_t* next)
			{
				leaf_page_t* page = NULL;
				return Allocator::allocate_new(page,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal(internal_page_t* parent, leaf_page_t* leaf)
			{
				internal_page_t* page = NULL;
				return Allocator::allocate_new(page,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal(internal_page_t* parent, internal_page_t* internal)
			{
				internal_page_t* page = NULL;
				return Allocator::allocate_new(page,parent,internal) ? page : NULL;
			}

			void destroy_leaf(leaf_page_t* page)
			{
				Allocator::delete_free(page);
			}

			void destroy_internal(internal_page_t* page)
			{
				Allocator::delete_free(page);
			}
		};

		template <typename K, typename V, typename Compare, size_t B>
		class BTreeBase<K,V,Compare,B,AllocatorInstance>
		{
			typedef BTreeLeafPage<K,V,Compare,B,AllocatorInstance> leaf_page_t;
			typedef BTreeInternalPage<K,V,Compare,B,AllocatorInstance> internal_page_t;

		public:
			leaf_page_t* new_leaf(internal_page_t* parent, leaf_page_t* prev, leaf_page_t* next)
			{
				leaf_page_t* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal(internal_page_t* parent, leaf_page_t* leaf)
			{
				internal_page_t* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal(internal_page_t* parent, internal_page_t* internal)
			{
				internal_page_t* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,internal) ? page : NULL;
			}

			void destroy_leaf(leaf_page_t* page)
			{
				m_allocator.delete_free(page);
			}

			void destroy_internal(internal_page_t* page)
			{
				m_allocator.delete_free(page);
			}

		protected:
			explicit BTreeBase(AllocatorInstance& allocator) : m_allocator(allocator)
			{}

		private:
			AllocatorInstance& m_allocator;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeImpl : public BTreeBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeBase<K,V,Compare,B,Allocator> baseClass;

			friend class BTreeInternalPage<K,V,Compare,B,Allocator>;
			friend class BTreeLeafPage<K,V,Compare,B,Allocator>;

		public:
			size_t size() const
			{
				return 0;
			}

			bool empty() const
			{
				return size() != 0;
			}

		protected:
			explicit BTreeImpl(const Compare& comp = Compare()) : baseClass(), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			explicit BTreeImpl(AllocatorInstance& allocator) : baseClass(allocator), m_compare(), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			BTreeImpl(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			~BTreeImpl()
			{
				if (m_root_page)
					m_root_page->destroy(this);
				else if (m_head)
					m_head->destroy(this);
			}

			bool insert(const Pair<K,V>& value)
			{
				if (m_root_page)
					return m_root_page->insert(this,value);

				if (!m_head)
				{
					if (!(m_head = baseClass::new_leaf(NULL,NULL,NULL)))
						return false;
					m_tail = m_head;
				}
				return m_head->insert(this,value);
			}

			template <typename K1>
			const Pair<K,V>* find(const K1& key) const
			{
				if (m_root_page)
					return m_root_page->find(key,m_compare);
				return m_head ? m_head->find(key,m_compare) : NULL;
			}

		private:
			Compare m_compare;
			BTreeInternalPage<K,V,Compare,B,Allocator>* m_root_page;
			BTreeLeafPage<K,V,Compare,B,Allocator>* m_head;
			BTreeLeafPage<K,V,Compare,B,Allocator>* m_tail;
		};
	}

	template <typename K, typename V, typename Compare = Less<K>, size_t B = 8, typename Allocator = CrtAllocator>
	class BTree : public detail::BTreeImpl<K,V,Compare,B,Allocator>
	{
		typedef detail::BTreeImpl<K,V,Compare,B,Allocator> baseClass;
	public:
		BTree() : baseClass()
		{}

		explicit BTree(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		bool insert(const Pair<K,V>& value)
		{
			return baseClass::insert(value) == 0;
		}
	};
}

#endif // OOBASE_BTREE_H_INCLUDED_
