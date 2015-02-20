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

		template <typename K, typename Compare>
		class BTreeCompareBase
		{
		public:
			virtual ~BTreeCompareBase()
			{}

			virtual bool less_than(const K& key) const = 0;
			virtual bool equal(const K& key) const = 0;
		};

		template <typename K1, typename K2, typename Compare>
		class BTreeCompare : public BTreeCompareBase<K1,Compare>
		{
		public:
			BTreeCompare(const Compare& compare, const K2& k) : m_compare(compare), m_key(k)
			{}

			bool less_than(const K1& key) const
			{
				return m_compare(key,m_key);
			}

			bool equal(const K1& key) const
			{
				return key == m_key;
			}

		private:
			const Compare& m_compare;
			K2 m_key;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageBase;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreePageBase
		{
		public:
			BTreeInternalPageBase<K,V,Compare,B,Allocator>* m_parent;

			BTreePageBase(BTreeInternalPageBase<K,V,Compare,B,Allocator>* parent) :
					m_parent(parent)
			{}

			virtual ~BTreePageBase()
			{}

			virtual void destroy(BTree<K,V,Compare,B,Allocator>* tree) = 0;
			virtual void dump(String& str, unsigned int indent) = 0;
			virtual bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value) = 0;
			virtual const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const = 0;
			virtual bool remove(BTree<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare) = 0;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageBase : public BTreePageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreePageBase<K,V,Compare,B,Allocator> baseClass;

		public:
			BTreeInternalPageBase(BTreeInternalPageBase* parent, baseClass* page) :
					baseClass(parent), m_key_count(0)
			{
				m_pages[0] = page;
			}

			virtual ~BTreeInternalPageBase()
			{}

			void destroy(BTree<K,V,Compare,B,Allocator>* tree)
			{
				for (size_t pos = 0; pos < m_key_count + 1; ++pos)
					m_pages[pos]->destroy(tree);

				tree->destroy_page(this);
			}

			void dump(String& str, unsigned int indent)
			{
				String str2;
				size_t i=0;
				for (;i<m_key_count;++i)
				{
					m_pages[i]->dump(str,indent + 2);

					str2.printf("%*s%s\n",indent,""," /");
					str.append(str2);
					str2.printf("%*s%c\n",indent,"",m_keys[i]);
					str.append(str2);
					str2.printf("%*s%s\n",indent,""," \\");
					str.append(str2);
				}

				if (!m_pages[i])
				{
					str2.printf("%*sNULL\n",indent + 2,"");
					str.append(str2);
				}
				else
					m_pages[i]->dump(str,indent + 2);
			}

			bool insert_page(BTree<K,V,Compare,B,Allocator>* tree, baseClass* page, const K& key)
			{
				if (m_key_count >= B - 1)
					return split(tree,page,key);

				size_t pos = find_page(BTreeCompare<K,K,Compare>(tree->m_compare,key));
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

			const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t page = find_page(compare);
				return m_pages[page]->find(compare);
			}

		protected:
			size_t     m_key_count;
			K          m_keys[B-1];
			baseClass* m_pages[B];

			static const size_t s_min = (B+1)/2;

			size_t find_page(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t start = 0;
				for (size_t end = m_key_count;start < end;)
				{
					size_t mid = start + (end - start) / 2;
					if (compare.less_than(m_keys[mid]))
						start = mid + 1;
					else if (compare.equal(m_keys[mid]))
						return mid + 1;
					else
						end = mid;
				}
				return start;
			}

			size_t find_page(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				return find_page(BTreeCompare<K,K,Compare>(tree->m_compare,value.first));
			}

			virtual BTreeInternalPageBase* new_page(BTree<K,V,Compare,B,Allocator>* tree) = 0;

			bool split(BTree<K,V,Compare,B,Allocator>* tree, baseClass* child, const K& child_key)
			{
				// Make sure we have a parent
				if (!this->m_parent)
				{
					if (!(this->m_parent = tree->new_internal_page(NULL,this)))
						return false;
					tree->m_root_page = this->m_parent;
				}

				BTreeInternalPageBase* sibling = new_page(tree);
				if (!sibling)
					return false;

				// Copy half our values
				size_t half = (m_key_count+1)/2 ;
				K sibling_key;
				OOBase::swap(sibling_key,m_keys[half]);

				for (size_t pos = half+1; pos < m_key_count; ++pos)
				{
					OOBase::swap(sibling->m_keys[sibling->m_key_count],m_keys[pos]);
					OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[pos]);

					// Update parent pointer of children
					sibling->m_pages[sibling->m_key_count]->m_parent = sibling;

					++sibling->m_key_count;
				}
				OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[m_key_count]);
				sibling->m_pages[sibling->m_key_count]->m_parent = sibling;

				m_key_count = half;

				// Add the sibling
				if (!this->m_parent->insert_page(tree,sibling,sibling_key))
				{
					// Copy everything back
					OOBase::swap(m_keys[m_key_count++],sibling_key);
					for (size_t i=0;i<sibling->m_key_count;++i)
					{
						OOBase::swap(m_keys[m_key_count],sibling->m_keys[i]);
						OOBase::swap(m_pages[m_key_count],sibling->m_pages[i]);

						m_pages[m_key_count]->m_parent = this;

						m_key_count++;
					}
					OOBase::swap(m_pages[m_key_count],sibling->m_pages[sibling->m_key_count]);
					m_pages[m_key_count]->m_parent = this;
					sibling->destroy(tree);
					return false;
				}

				if (tree->m_compare(child_key,sibling->m_keys[0]))
					return insert_page(tree,child,child_key);
				
				child->m_parent = sibling;
				return sibling->insert_page(tree,child,child_key);
			}

			baseClass* const* find_exact(const K& key, const Compare& compare) const
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
				baseClass* const* p = find_exact(child_key,tree->m_compare);
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
		class BTreeInternalPageInternal : public BTreeInternalPageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeInternalPageBase<K,V,Compare,B,Allocator> baseClass;

			baseClass* new_page(BTree<K,V,Compare,B,Allocator>* tree)
			{
				return tree->new_internal_page(this->m_parent,NULL);
			}

		public:
			BTreeInternalPageInternal(baseClass* parent, BTreeInternalPageBase<K,V,Compare,B,Allocator>* page) :
					baseClass(parent,page)
			{}

			bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = baseClass::find_page(tree,value);
				return m_pages[page]->insert(tree,value);
			}

			bool remove(BTree<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
			{
				size_t page = baseClass::find_page(compare);
				if (!this->m_pages[page]->remove(tree,compare))
					return false;

				return true;
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageLeaf : public BTreeInternalPageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeInternalPageBase<K,V,Compare,B,Allocator> baseClass;
			friend class BTreeLeafPage<K,V,Compare,B,Allocator>;

			baseClass* new_page(BTree<K,V,Compare,B,Allocator>* tree)
			{
				return tree->new_internal_leaf(this->m_parent,NULL);
			}

			void adjust_key(size_t pos, const K& key)
			{
				if (pos > 0)
					this->m_keys[pos-1] = key;
			}

		public:
			BTreeInternalPageLeaf(baseClass* parent, BTreeLeafPage<K,V,Compare,B,Allocator>* page) :
					baseClass(parent,page)
			{}

			bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = baseClass::find_page(tree,value);
				return static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[page])->insert_i(tree,value,page);
			}

			bool remove(BTree<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
			{
				size_t page = baseClass::find_page(compare);
				if (!static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[page])->remove_i(tree,compare,page))
					return false;

				if (static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[page])->m_size == 0)
				{

				}

				return true;
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeIterator
		{
		public:
			BTreeIterator(BTreeLeafPage<K,V,Compare,B,Allocator>* page, size_t pos) : m_page(page), m_pos(pos)
			{}

			BTreeLeafPage<K,V,Compare,B,Allocator>* m_page;
			size_t                                  m_pos;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeConstIterator
		{
		public:
			BTreeConstIterator(const BTreeLeafPage<K,V,Compare,B,Allocator>* page, size_t pos) : m_page(page), m_pos(pos)
			{}

			const BTreeLeafPage<K,V,Compare,B,Allocator>* m_page;
			size_t                                        m_pos;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage : public BTreePageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreePageBase<K,V,Compare,B,Allocator> baseClass;
			friend class BTreeInternalPageLeaf<K,V,Compare,B,Allocator>;

		public:
			BTreeLeafPage(BTreeInternalPageBase<K,V,Compare,B,Allocator>* parent, BTreeLeafPage* prev, BTreeLeafPage* next) :
					baseClass(parent), m_prev(prev), m_next(next), m_size(0)
			{}

			void dump(String& str, unsigned int indent)
			{
				String str2;
				str2.printf("%*s",indent,"");
				str.append(str2);
				for (size_t i=0;i<m_size;++i)
				{
					str2.printf("%c ",m_data[i].first);
					str.append(str2);
				}
				str2.printf("\n");
				str.append(str2);
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

				tree->destroy_page(this);
			}

			bool insert(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				assert(!this->m_parent);
				return insert_i(tree,value,0);
			}

			const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t pos = find_i(compare);
				return pos == size_t(-1) ? NULL : &m_data[pos];
			}

			bool remove(BTree<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
			{
				assert(!this->m_parent);
				return remove_i(tree,compare,0);
			}

			BTreeIterator<K,V,Compare,B,Allocator> begin()
			{
				if (!m_size)
					return BTreeIterator<K,V,Compare,B,Allocator>(NULL,0);

				return BTreeIterator<K,V,Compare,B,Allocator>(this,0);
			}

			BTreeConstIterator<K,V,Compare,B,Allocator> cbegin() const
			{
				if (!m_size)
					return BTreeConstIterator<K,V,Compare,B,Allocator>(NULL,0);

				return BTreeConstIterator<K,V,Compare,B,Allocator>(this,0);
			}

		private:
			BTreeLeafPage* m_prev;
			BTreeLeafPage* m_next;

			size_t m_size;
			Pair<K,V> m_data[B-1];

			static const size_t s_min = (B+1)/2;

			size_t find_i(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t start = 0;
				for (size_t end = m_size;start < end;)
				{
					size_t mid = start + (end - start) / 2;
					if (compare.less_than(m_data[mid].first))
						start = mid + 1;
					else if (compare.equal(m_data[mid].first))
						return mid;
					else
						end = mid;
				}
				return size_t(-1);
			}

			bool steal_prev(size_t pos, size_t parent_pos)
			{
				size_t steal = (m_prev->m_size - s_min) / 2 + 1;
				for (size_t i = m_size; steal > 1 && i-- > pos;)
					OOBase::swap(m_data[i],m_data[i+steal-1]);
				
				for (size_t i = pos; i-- > 0;)
					OOBase::swap(m_data[i],m_data[i+steal]);

				for (size_t i = steal; i-- > 0;)
					OOBase::swap(m_data[i],m_prev->m_data[--m_prev->m_size]);

				adjust_parent_key(parent_pos);

				return true;
			}

			bool merge_prev(size_t pos, size_t parent_pos)
			{
				return true;
			}

			bool steal_next(size_t pos, size_t parent_pos)
			{
				size_t steal = (m_prev->m_size - s_min) / 2 + 1;
				
				for (size_t i = pos+1; i < m_size; ++i)
					OOBase::swap(m_data[i-1],m_data[i]);

				for (size_t i = 0; i < steal; ++i)
					OOBase::swap(m_data[m_size++],m_next->m_data[i]);

				for (size_t i = steal; i < m_next->m_size; ++i)
					OOBase::swap(m_next->m_data[i - steal],m_next->m_data[i]);

				m_next->m_size -= steal;

				adjust_parent_key(parent_pos+1);

				return true;
			}

			bool merge_next(size_t pos, size_t parent_pos)
			{
				return true;
			}

			void adjust_parent_key(size_t parent_pos)
			{
				if (this->m_parent)
					static_cast<BTreeInternalPageLeaf<K,V,Compare,B,Allocator>*>(this->m_parent)->adjust_key(parent_pos,this->m_data[0].first);
			}

			bool insert_i(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value, size_t parent_pos)
			{
				size_t pos = 0;
				for (size_t end = m_size;pos < end;)
				{
					size_t mid = pos + (end - pos) / 2;
					if (tree->m_compare(m_data[mid].first,value.first))
						pos = mid + 1;
					else if (m_data[mid].first == value.first)
					{
						m_data[mid].second = value.second;
						return true;
					}
					else
						end = mid;
				}

				if (m_size >= B)
					return split(tree,value);

				for (size_t i = m_size;i > pos;--i)
					OOBase::swap(m_data[i],m_data[i-1]);
				m_data[pos] = value;
				++m_size;

				if (pos == 0)
					adjust_parent_key(parent_pos);

				return true;
			}

			bool remove_i(BTree<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare, size_t parent_pos)
			{
				size_t pos = find_i(compare);
				if (pos == size_t(-1))
					return false;

				// Swap out the value
				Pair<K,V>().swap(m_data[pos]);

				if (this->m_parent && m_size <= s_min)
				{
					// Underflow, steal from a direct neighbour
					size_t prev = (m_prev && m_prev->m_parent == this->m_parent) ? m_prev->m_size : 0;
					size_t next = (m_next && m_next->m_parent == this->m_parent) ? m_next->m_size : 0;

					if (prev > s_min && prev > next)
						return steal_prev(pos,parent_pos);
					if (next > s_min)
						return steal_next(pos,parent_pos);
					/*if (prev)
						return merge_prev(pos,parent_pos);
					if (next)
						return merge_next(pos,parent_pos);*/
				}

				// Just ripple down
				--m_size;
				for (size_t i = pos; i < m_size; ++i)
					OOBase::swap(m_data[i],m_data[i+1]);

				if (pos == 0 && m_size)
					adjust_parent_key(parent_pos);

				return true;
			}
			
			bool split(BTree<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				// Make sure we have a parent
				if (!this->m_parent)
				{
					if (!(this->m_parent = tree->new_internal_leaf(NULL,this)))
						return false;
					tree->m_root_page = this->m_parent;
				}

				// New leaf page in position 'next'
				BTreeLeafPage* sibling = tree->new_leaf(this->m_parent,this,m_next);
				if (!sibling)
					return false;
				if (!m_next)
					tree->m_tail = sibling;
				else
					m_next->m_prev = sibling;
				m_next = sibling;

				// Copy half our values
				for (size_t pos = s_min; pos < m_size; ++pos)
					OOBase::swap(sibling->m_data[sibling->m_size++],m_data[pos]);
				m_size = s_min;

				if (!this->m_parent->insert_page(tree,sibling,sibling->m_data[0].first))
				{
					// Copy back again
					for (size_t i=0;i<sibling->m_size;++i)
						OOBase::swap(m_data[m_size++],sibling->m_data[i]);

					sibling->destroy(tree);
					return false;
				}

				return m_parent->insert(tree,value);
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeBase
		{
			typedef BTreePageBase<K,V,Compare,B,Allocator> base_page_t;
			typedef BTreeLeafPage<K,V,Compare,B,Allocator> leaf_page_t;
			typedef BTreeInternalPageBase<K,V,Compare,B,Allocator> internal_page_t;

		public:
			leaf_page_t* new_leaf(internal_page_t* parent, leaf_page_t* prev, leaf_page_t* next)
			{
				BTreeLeafPage<K,V,Compare,B,Allocator>* page = NULL;
				return Allocator::allocate_new(page,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				BTreeInternalPageLeaf<K,V,Compare,B,Allocator>* page = NULL;
				return Allocator::allocate_new(page,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal_page(internal_page_t* parent, internal_page_t* internal)
			{
				BTreeInternalPageInternal<K,V,Compare,B,Allocator>* page = NULL;
				return Allocator::allocate_new(page,parent,internal) ? page : NULL;
			}

			void destroy_page(base_page_t* page)
			{
				Allocator::delete_free(page);
			}
		};

		template <typename K, typename V, typename Compare, size_t B>
		class BTreeBase<K,V,Compare,B,AllocatorInstance>
		{
			typedef BTreePageBase<K,V,Compare,B,AllocatorInstance> base_page_t;
			typedef BTreeLeafPage<K,V,Compare,B,AllocatorInstance> leaf_page_t;
			typedef BTreeInternalPageBase<K,V,Compare,B,AllocatorInstance> internal_page_t;

		public:
			leaf_page_t* new_leaf(internal_page_t* parent, leaf_page_t* prev, leaf_page_t* next)
			{
				leaf_page_t* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				BTreeInternalPageLeaf<K,V,Compare,B,AllocatorInstance>* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal_page(internal_page_t* parent, internal_page_t* internal)
			{
				BTreeInternalPageInternal<K,V,Compare,B,AllocatorInstance>* page = NULL;
				return m_allocator.allocate_new(page,m_allocator,parent,internal) ? page : NULL;
			}

			void destroy_page(base_page_t* page)
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

		friend class detail::BTreeInternalPageBase<K,V,Compare,B,Allocator>;
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

		typedef detail::IteratorImpl<BTree,value_type,detail::BTreeIterator<K,V,Compare,B,Allocator> > iterator;
		friend class detail::IteratorImpl<BTree,value_type,detail::BTreeIterator<K,V,Compare,B,Allocator> >;
		typedef detail::IteratorImpl<const BTree,const value_type,detail::BTreeConstIterator<K,V,Compare,B,Allocator> > const_iterator;
		friend class detail::IteratorImpl<const BTree,const value_type,detail::BTreeConstIterator<K,V,Compare,B,Allocator> >;

		BTree(const Compare& comp = Compare()) : baseClass(), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(AllocatorInstance& allocator) : baseClass(allocator), m_compare(), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{}

		BTree(const BTree& rhs) : m_compare(rhs.m_compare), m_root_page(NULL), m_head(NULL), m_tail(NULL)
		{
			if (!insert(rhs.begin(),rhs.end()))
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
		}

		~BTree()
		{
			static_assert(B > 2,"BTree must be of order > 2");

			if (m_root_page)
				m_root_page->destroy(this);
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
			if (!m_root_page)
			{
				if (!(m_head = this->new_leaf(NULL,NULL,NULL)))
					return false;

				m_root_page = m_tail = m_head;
			}
			return m_root_page->insert(this,value);
		}

		bool insert(const K& key, const V& value)
		{
			return insert(OOBase::make_pair(key,value));
		}

		template <typename K1>
		bool remove(const K1& key)
		{
			return m_root_page && m_root_page->remove(this,detail::BTreeCompare<K,K1,Compare>(m_compare,key));
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			return (find_i(key) != NULL);
		}

		size_t size() const
		{
			return m_root_page ? m_root_page->size() : 0;
		}

		bool empty() const
		{
			return size() != 0;
		}

		iterator begin()
		{
			return m_head ? iterator(this,m_head->begin()) : end();
		}

		const_iterator cbegin() const
		{
			return m_head ? const_iterator(this,m_head->cbegin()) : cend();
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
			return iterator(this,detail::BTreeIterator<K,V,Compare,B,Allocator>(NULL,0));
		}

		const_iterator cend() const
		{
			return const_iterator(this,detail::BTreeConstIterator<K,V,Compare,B,Allocator>(NULL,0));
		}

		const_iterator end() const
		{
			return cend();
		}

		void dump()
		{
			OOBase::String str;
			if (m_root_page)
				m_root_page->dump(str,0);

			LOG_DEBUG(("BTree:\n%s",str.c_str()));
		}

		template <typename K1>
		bool find(const K1& key) const
		{
			return m_root_page && m_root_page->find(detail::BTreeCompare<K,K1,Compare>(m_compare,key)) != NULL;
		}

	private:
		Compare m_compare;
		detail::BTreePageBase<K,V,Compare,B,Allocator>* m_root_page;
		detail::BTreeLeafPage<K,V,Compare,B,Allocator>* m_head;
		detail::BTreeLeafPage<K,V,Compare,B,Allocator>* m_tail;
	};
}

#endif // OOBASE_BTREE_H_INCLUDED_
