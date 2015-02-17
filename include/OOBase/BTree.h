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

#include <stdlib.h>

namespace OOBase
{
	template <typename K, typename V, typename Compare, size_t B, typename Allocator>
	class BTree;

	namespace detail
	{
		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPage
		{
		public:
			BTreeInternalPage* m_parent;

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

			void dump(BTreeInternalPage* parent)
			{
				assert(m_parent == parent);

				::printf("Internal:%p\n",this);

				for (size_t i=0;i<m_key_count;++i)
					::printf("        %c ",m_keys[i]);
				::printf("\n ");
				for (size_t i=0;i<m_key_count;++i)
					::printf("      / \\ ");
				::printf("\n");

				if (!m_leafs)
				{
					for (size_t i=0;i<m_key_count+1;++i)
						::printf("%p ",m_pages[i]);
					::printf("\n\n");

					for (size_t i=0;i<m_key_count+1;++i)
						static_cast<BTreeInternalPage*>(m_pages[i])->dump(this);
				}
				else
				{
					for (size_t i=0;i<m_key_count+1;++i)
						static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[i])->dump(this);
					::printf("\n");
				}

				::printf("\n");
			}

			void destroy(BTree<K,V,Compare,B,Allocator>* tree)
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

			bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = find_page(value.first,tree->m_compare);
				if (m_leafs)
					return static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[page])->insert(tree,value);
				else
					return static_cast<BTreeInternalPage*>(m_pages[page])->insert(tree,value);
			}

			bool insert_page(BTree<K,V,Compare,B,Allocator>* tree, void* page, const K& key)
			{
				if (m_key_count == B - 1)
					return split(tree,page,key);

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
			const bool m_leafs;
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

			bool split(BTree<K,V,Compare,B,Allocator>* tree, void* child_page, const K& child_key)
			{
				BTreeInternalPage* page = m_leafs ?
						tree->new_internal_leaf(m_parent,NULL)
						: tree->new_internal_page(m_parent,NULL);

				if (!page)
					return false;

				if (!m_parent)
				{
					if (!(m_parent = tree->new_internal_page(NULL,this)))
					{
						page->destroy(tree);
						return false;
					}
					page->m_parent = m_parent;
					tree->m_root_page = m_parent;
				}

				size_t half = (m_key_count+1)/2;
				K page_key;
				OOBase::swap(page_key,m_keys[half]);

				for (size_t pos = half+1; pos < m_key_count; ++pos)
				{
					OOBase::swap(page->m_keys[page->m_key_count],m_keys[pos]);
					OOBase::swap(page->m_pages[page->m_key_count],m_pages[pos]);

					// Update parent pointer of children
					if (page->m_leafs)
						static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(page->m_pages[page->m_key_count])->m_parent = page;
					else
						static_cast<BTreeInternalPage*>(page->m_pages[page->m_key_count])->m_parent = page;

					++page->m_key_count;
				}
				OOBase::swap(page->m_pages[page->m_key_count],m_pages[m_key_count]);
				if (page->m_leafs)
					static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(page->m_pages[page->m_key_count])->m_parent = page;
				else
					static_cast<BTreeInternalPage*>(page->m_pages[page->m_key_count])->m_parent = page;

				m_key_count = half;

				if (tree->m_compare(child_key,page_key))
					insert_page(tree,child_page,child_key);
				else
				{
					if (page->m_leafs)
						static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(child_page)->m_parent = page;
					else
						static_cast<BTreeInternalPage*>(child_page)->m_parent = page;
					page->insert_page(tree,child_page,child_key);
				}

				if (!m_parent->insert_page(tree,page,page_key))
				{
					remove_page(tree,child_key);
					page->remove_page(tree,child_key);

					OOBase::swap(m_keys[m_key_count++],page_key);
					for (size_t i=0;i<page->m_key_count;++i)
					{
						OOBase::swap(m_keys[m_key_count],page->m_keys[i]);
						OOBase::swap(m_pages[m_key_count],page->m_pages[i]);
						if (m_leafs)
							static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[m_key_count])->m_parent = this;
						else
							static_cast<BTreeInternalPage*>(m_pages[m_key_count])->m_parent = this;

						m_key_count++;
					}
					OOBase::swap(m_pages[m_key_count],page->m_pages[page->m_key_count]);
					if (m_leafs)
						static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(m_pages[m_key_count])->m_parent = this;
					else
						static_cast<BTreeInternalPage*>(m_pages[m_key_count])->m_parent = this;
					page->destroy(tree);
					return false;
				}
				return true;
			}

			void* const* find_exact(const K& key, const Compare& compare) const
			{
				size_t start = 0;
				for (size_t end = m_key_count;start < end;)
				{
					size_t mid = start + (end - start) / 2;
					if (compare(m_keys[mid],key))
						start = mid + 1;
					else if (m_keys[mid] == key)
						return &m_pages[mid];
					else
						end = mid;
				}
				return NULL;
			}

			bool remove_page(BTree<K,V,Compare,B,Allocator>* tree, const K& child_key)
			{
				void* const* p = find_exact(child_key,tree->m_compare);
				if (!p)
					return false;

				--m_key_count;
				for (size_t i = p - m_pages;i < m_key_count;++i)
				{
					OOBase::swap(m_keys[i],m_keys[i+1]);
					OOBase::swap(m_pages[i],m_pages[i+1]);
				}
				OOBase::swap(m_pages[m_key_count],m_pages[m_key_count+1]);
				return true;
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage
		{
		public:
			BTreeInternalPage<K,V,Compare,B,Allocator>* m_parent;

			BTreeLeafPage(BTreeInternalPage<K,V,Compare,B,Allocator>* parent, BTreeLeafPage* prev, BTreeLeafPage* next) :
					m_parent(parent), m_prev(prev), m_next(next), m_size(0)
			{}

			void dump(BTreeInternalPage<K,V,Compare,B,Allocator>* parent)
			{
				assert(m_parent == parent);

				::printf("[ ");

				size_t i=0;
				for (;i<m_size;++i)
					::printf("%c ",m_data[i].first);
				for (;i < B-1;++i)
					::printf("  ");

				::printf("]   ");
			}

			void destroy(BTree<K,V,Compare,B,Allocator>* tree)
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

			bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				if (m_size == B - 1)
					return split(tree,value);

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

		private:
			BTreeLeafPage* m_prev;
			BTreeLeafPage* m_next;

			size_t m_size;
			Pair<K,V> m_data[B-1];

			bool need_merge() const
			{
				return m_size == (m_parent ? B/2 : 0);
			}

			BTreeLeafPage* insert_leaf(BTree<K,V,Compare,B,Allocator>* tree)
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

			bool split(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				BTreeLeafPage* leaf = insert_leaf(tree);
				if (!leaf)
					return false;

				if (!m_parent)
				{
					if (!(m_parent = tree->new_internal_leaf(NULL,this)))
					{
						leaf->destroy(tree);
						return false;
					}
					leaf->m_parent = m_parent;
					tree->m_root_page = m_parent;
				}

				size_t half = m_size/2;
				bool insert_into_us = tree->m_compare(value.first,m_data[half].first);
				if (!insert_into_us)
					++half;

				for (size_t pos = half; pos < m_size; ++pos)
					OOBase::swap(leaf->m_data[leaf->m_size++],m_data[pos]);
				m_size = half;

				if (insert_into_us)
					insert(tree,value);
				else
					leaf->insert(tree,value);

				if (!m_parent->insert_page(tree,leaf,leaf->m_data[0].first))
				{
					remove_value(tree,value);
					leaf->remove_value(tree,value);

					for (size_t i=0;i<leaf->m_size;++i)
						OOBase::swap(m_data[m_size++],leaf->m_data[i]);
					leaf->destroy(tree);
					return false;
				}
				return true;
			}

			bool remove_value(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				const Pair<K,V>* p = find(value.first,tree->m_compare);
				if (!p)
					return false;

				--m_size;
				for (size_t i = p - m_data;i < m_size;++i)
					OOBase::swap(m_data[i],m_data[i+1]);
				return true;
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

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				internal_page_t* page = NULL;
				return Allocator::allocate_new(page,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal_page(internal_page_t* parent, internal_page_t* internal)
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

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				internal_page_t* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal_page(internal_page_t* parent, internal_page_t* internal)
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
			BTreeBase(AllocatorInstance& allocator) : m_allocator(allocator)
			{}

		private:
			AllocatorInstance& m_allocator;
		};
	}

	template <typename K, typename V, typename Compare = Less<K>, size_t B = 8, typename Allocator = CrtAllocator>
	class BTree : public detail::BTreeBase<K,V,Compare,B,Allocator>
	{
		typedef detail::BTreeBase<K,V,Compare,B,Allocator> baseClass;

		friend class detail::BTreeInternalPage<K,V,Compare,B,Allocator>;
		friend class detail::BTreeLeafPage<K,V,Compare,B,Allocator>;

	public:
		typedef K key_type;
		typedef Pair<K,V> value_type;
		typedef V mapped_type;
		typedef Compare key_compare;
		typedef Allocator allocator_type;
		typedef value_type& reference;
		typedef typename add_const<reference>::type const_reference;
		typedef value_type* pointer;
		typedef typename add_const<pointer>::type const_pointer;

		typedef detail::IteratorImpl<BTree,value_type,size_t> iterator;
		friend class detail::IteratorImpl<BTree,value_type,size_t>;
		typedef detail::IteratorImpl<const BTree,const value_type,size_t> const_iterator;
		friend class detail::IteratorImpl<const BTree,const value_type,size_t>;

		BTree(const Compare& comp = Compare()) : baseClass(), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(AllocatorInstance& allocator) : baseClass(allocator), m_compare(), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(const BTree& rhs) : m_compare(rhs.m_compare), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{
			/*for (size_t i=0;i<rhs.m_size;++i)
			{
				int err = push_back(rhs.m_data[i]);
				if (err)
					OOBase_CallCriticalFailure(err);
			}*/
		}

		~BTree()
		{
			if (m_root_page)
				m_root_page->destroy(this);
			else if (m_head)
				m_head->destroy(this);
		}

		BTree& operator = (const BTree& rhs)
		{
			BTree(rhs).swap(*this);
			return *this;
		}

		void swap(BTree& rhs)
		{
			OOBase::swap(m_compare,rhs.m_compare);
			OOBase::swap(m_root_page,rhs.m_root_page);
			OOBase::swap(m_head,rhs.m_head);
			OOBase::swap(m_tail,rhs.m_tail);
		}

		template <typename It>
		bool insert(It first, It last)
		{
			for (It i = first; i != last; ++i)
			{
				if (!insert(*i))
					return false;
			}
			return true;
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

		bool insert(const K& key, const V& value)
		{
			return insert(OOBase::make_pair(key,value));
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			return (find_i(key) != NULL);
		}

		size_t size() const
		{
			return 0;
		}

		bool empty() const
		{
			return size() != 0;
		}

		iterator begin()
		{
			return baseClass::empty() ? end() : iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return baseClass::empty() ? end() : const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : end());
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : end());
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

		void dump()
		{
			if (m_root_page)
				m_root_page->dump(NULL);
			else if (m_head)
				m_head->dump(NULL);

			::printf("\n******\n");
		}

	private:
		Compare m_compare;
		detail::BTreeInternalPage<K,V,Compare,B,Allocator>* m_root_page;
		detail::BTreeLeafPage<K,V,Compare,B,Allocator>* m_head;
		detail::BTreeLeafPage<K,V,Compare,B,Allocator>* m_tail;

		template <typename K1>
		const Pair<K,V>* find_i(const K1& key) const
		{
			if (m_root_page)
				return m_root_page->find(key,m_compare);
			return m_head ? m_head->find(key,m_compare) : NULL;
		}
	};
}

#endif // OOBASE_BTREE_H_INCLUDED_
