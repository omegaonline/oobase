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
	namespace detail
	{
		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeImpl;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeLeafPage;

		template <typename K, typename Compare>
		class BTreeCompareBase
		{
		public:
			virtual ~BTreeCompareBase()
			{}

			virtual bool less_than(typename call_traits<K>::param_type key) const = 0;
			virtual bool equal(typename call_traits<K>::param_type key) const = 0;
		};

		template <typename K1, typename K2, typename Compare>
		class BTreeCompare : public BTreeCompareBase<K1,Compare>
		{
		public:
			BTreeCompare(const Compare& compare, typename call_traits<K2>::param_type k) : m_compare(compare), m_key(k)
			{}

			bool less_than(typename call_traits<K1>::param_type key) const
			{
				return m_compare(key,m_key);
			}

			bool equal(typename call_traits<K1>::param_type key) const
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

			virtual void dump(String& str, unsigned int indent, BTreePageBase* parent) = 0;

			virtual void destroy(BTreeImpl<K,V,Compare,B,Allocator>* tree) = 0;
			virtual bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value) = 0;
			virtual const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const = 0;
			virtual bool remove(BTreeImpl<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare) = 0;
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageInternal;

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageBase : public BTreePageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreePageBase<K,V,Compare,B,Allocator> baseClass;
			friend class BTreeInternalPageInternal<K,V,Compare,B,Allocator>;

		public:
			BTreeInternalPageBase(BTreeInternalPageBase* parent, baseClass* page) :
					baseClass(parent), m_key_count(0)
			{
				memset(m_pages,0,sizeof(m_pages)); // DEBUG
				m_pages[0] = page;

			}

			virtual ~BTreeInternalPageBase()
			{}

			void destroy(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				for (size_t pos = 0; pos < m_key_count + 1; ++pos)
					m_pages[pos]->destroy(tree);

				tree->destroy_page(this);
			}

			void dump(String& str, unsigned int indent, BTreePageBase<K,V,Compare,B,Allocator>* parent)
			{
				assert(this->m_parent == parent);

				String str2;

				if (m_key_count == size_t(-1))
					m_key_count = 0;

				if (m_key_count)
				{
					size_t i=0;
					for (;i<m_key_count;++i)
					{
						if (!m_pages[i])
						{
							str2.printf("%*sNULL\n",indent + 2,"");
							str.append(str2);
						}
						else
							m_pages[i]->dump(str,indent + 2,this);


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
						m_pages[i]->dump(str,indent + 2,this);
				}
				else
				{
					if (!m_pages[0])
					{
						str2.printf("%*sNULL\n",indent + 2,"");
						str.append(str2);
					}
					else
						m_pages[0]->dump(str,indent + 2,this);

					str2.printf("%*s%s\n",indent,""," /");
					str.append(str2);
					str2.printf("%*s?\n",indent,"");
					str.append(str2);
				}
			}

			const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t page = find_page(compare);
				return m_pages[page]->find(compare);
			}

			bool insert_page(BTreeImpl<K,V,Compare,B,Allocator>* tree, baseClass* page, typename call_traits<K>::param_type key, size_t insert_pos)
			{
				if (m_key_count >= B - 1)
					return split(tree,insert_pos,page,key);

				for (size_t i = m_key_count;i-- > insert_pos;)
				{
					OOBase::swap(m_pages[i+1],m_pages[i+2]);
					OOBase::swap(m_keys[i],m_keys[i+1]);
				}
				m_pages[insert_pos+1] = page;
				m_keys[insert_pos] = key;
				++m_key_count;

				return true;
			}

		protected:
			size_t     m_key_count;
			K          m_keys[B-1];
			baseClass* m_pages[B];

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

			size_t find_page(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				return find_page(BTreeCompare<K,K,Compare>(tree->m_compare,value.first));
			}

			virtual BTreeInternalPageBase* new_page(BTreeImpl<K,V,Compare,B,Allocator>* tree) = 0;

			bool split(BTreeImpl<K,V,Compare,B,Allocator>* tree, size_t insert_pos, baseClass* child, typename call_traits<K>::param_type child_key)
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

				const size_t half = B/2;
				if (insert_pos < half)
				{
					size_t parent_pos = this->m_parent->find_page(BTreeCompare<K,K,Compare>(tree->m_compare,m_keys[half-1]));
					if (!this->m_parent->insert_page(tree,sibling,m_keys[half-1],parent_pos))
						return false;

					K sibling_key;
					OOBase::swap(m_keys[half-1],sibling_key);

					// Insert into us
					size_t i = half;
					for (; i < m_key_count; ++i)
					{
						OOBase::swap(sibling->m_keys[sibling->m_key_count],m_keys[i]);
						OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[i]);
						sibling->m_pages[sibling->m_key_count++]->m_parent = sibling;
					}
					OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[i]);
					sibling->m_pages[sibling->m_key_count]->m_parent = sibling;
					m_key_count = half - 1;

					return insert_page(tree,child,child_key,insert_pos);
				}
				else if (insert_pos == half)
				{
					size_t parent_pos = this->m_parent->find_page(BTreeCompare<K,K,Compare>(tree->m_compare,child_key));
					if (!this->m_parent->insert_page(tree,sibling,child_key,parent_pos))
						return false;

					// Insert into us
					size_t i = half;
					for (; i < m_key_count; ++i)
					{
						OOBase::swap(sibling->m_keys[sibling->m_key_count],m_keys[i]);
						OOBase::swap(sibling->m_pages[sibling->m_key_count+1],m_pages[i+1]);
						sibling->m_pages[sibling->m_key_count+1]->m_parent = sibling;
						sibling->m_key_count++;
					}
					sibling->m_pages[0] = child;
					sibling->m_pages[0]->m_parent = sibling;
					m_key_count = half;
					return true;
				}

				size_t parent_pos = this->m_parent->find_page(BTreeCompare<K,K,Compare>(tree->m_compare,m_keys[half]));
				if (!this->m_parent->insert_page(tree,sibling,m_keys[half],parent_pos))
					return false;

				K sibling_key;
				OOBase::swap(m_keys[half],sibling_key);

				size_t i = half+1;
				for (; i < insert_pos; ++i)
				{
					OOBase::swap(sibling->m_keys[sibling->m_key_count],m_keys[i]);
					OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[i]);
					sibling->m_pages[sibling->m_key_count++]->m_parent = sibling;
				}
				OOBase::swap(sibling->m_pages[sibling->m_key_count],m_pages[i]);
				sibling->m_pages[sibling->m_key_count]->m_parent = sibling;

				sibling->m_keys[sibling->m_key_count] = child_key;
				sibling->m_pages[sibling->m_key_count+1] = child;
				sibling->m_pages[sibling->m_key_count+1]->m_parent = sibling;
				++sibling->m_key_count;

				for (; i < m_key_count; ++i)
				{
					OOBase::swap(sibling->m_keys[sibling->m_key_count],m_keys[i]);
					OOBase::swap(sibling->m_pages[sibling->m_key_count+1],m_pages[i+1]);
					sibling->m_pages[sibling->m_key_count+1]->m_parent = sibling;
					++sibling->m_key_count;
				}
				m_key_count = half;
				return true;
			}

			baseClass* const* find_exact(typename call_traits<K>::param_type key, const Compare& compare) const
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
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageInternal : public BTreeInternalPageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeInternalPageBase<K,V,Compare,B,Allocator> baseClass;

			baseClass* new_page(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				return tree->new_internal_page(this->m_parent,NULL);
			}

			static const size_t s_min = B/2;

		public:
			BTreeInternalPageInternal(baseClass* parent, BTreeInternalPageBase<K,V,Compare,B,Allocator>* page) :
					baseClass(parent,page)
			{}

			bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = baseClass::find_page(tree,value);
				return this->m_pages[page]->insert(tree,value);
			}

			bool remove(BTreeImpl<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
			{
				size_t pos = baseClass::find_page(compare);
				if (!this->m_pages[pos]->remove(tree,compare))
					return false;

				baseClass* page = static_cast<baseClass*>(this->m_pages[pos]);
				if (page->m_key_count < s_min)
				{
					size_t i;
					baseClass* prev = pos > 0 ? static_cast<baseClass*>(this->m_pages[pos-1]) : NULL;
					baseClass* next = pos < this->m_key_count ? static_cast<baseClass*>(this->m_pages[pos+1]) : NULL;

					if (next && next->m_key_count > s_min)
					{
						size_t steal = (next->m_key_count - s_min + 1) / 2;
						for (i = steal;i-- > 0;)
						{
							OOBase::swap(page->m_keys[page->m_key_count++],this->m_keys[pos]);
							OOBase::swap(page->m_pages[page->m_key_count],next->m_pages[i]);
							page->m_pages[page->m_key_count]->m_parent = page;
							OOBase::swap(this->m_keys[pos],next->m_keys[i]);
						}
						next->m_key_count -= steal;
						for (i = 0; i < next->m_key_count; ++i)
						{
							OOBase::swap(next->m_keys[i],next->m_keys[i+steal]);
							OOBase::swap(next->m_pages[i],next->m_pages[i+steal]);
						}
						OOBase::swap(next->m_pages[i],next->m_pages[i+steal]);
					}
					else if (prev && prev->m_key_count > s_min)
					{
						size_t steal = (prev->m_key_count - s_min + 1) / 2;
						i = page->m_key_count;
						OOBase::swap(page->m_pages[i],page->m_pages[i+steal]);
						while (i-- > 0)
						{
							OOBase::swap(page->m_keys[i],page->m_keys[i+steal]);
							OOBase::swap(page->m_pages[i],page->m_pages[i+steal]);
						}
						page->m_key_count += steal;
						for (i = steal; i-- > 0;)
						{
							OOBase::swap(this->m_keys[pos-1],page->m_keys[i]);
							OOBase::swap(page->m_pages[i],prev->m_pages[prev->m_key_count--]);
							page->m_pages[i]->m_parent = page;
							OOBase::swap(this->m_keys[pos-1],prev->m_keys[prev->m_key_count]);
						}
					}
					else if (next)
					{
						i = next->m_key_count;
						OOBase::swap(next->m_pages[i],next->m_pages[i+page->m_key_count+1]);
						while (i-- > 0)
						{
							OOBase::swap(next->m_keys[i],next->m_keys[i+page->m_key_count+1]);
							OOBase::swap(next->m_pages[i],next->m_pages[i+page->m_key_count+1]);
						}
						for (i = 0;i < page->m_key_count;++i)
						{
							OOBase::swap(next->m_keys[i],page->m_keys[i]);
							OOBase::swap(next->m_pages[i],page->m_pages[i]);
							next->m_pages[i]->m_parent = next;
						}
						OOBase::swap(next->m_keys[i],this->m_keys[pos]);
						OOBase::swap(next->m_pages[i],page->m_pages[i]);
						next->m_pages[i]->m_parent = next;
						next->m_key_count += page->m_key_count+1;

						--this->m_key_count;
						this->m_pages[pos] = NULL; // DEBUG
						for (i = pos;i < this->m_key_count;++i)
						{
							OOBase::swap(this->m_keys[i],this->m_keys[i+1]);
							OOBase::swap(this->m_pages[i],this->m_pages[i+1]);
						}
						OOBase::swap(this->m_pages[i],this->m_pages[i+1]);
						tree->destroy_page(page);
					}
					else if (prev)
					{
						OOBase::swap(prev->m_keys[prev->m_key_count++],this->m_keys[pos-1]);
						OOBase::swap(prev->m_pages[prev->m_key_count],page->m_pages[0]);
						prev->m_pages[prev->m_key_count]->m_parent = prev;
						for (i = 0;i < page->m_key_count;++i)
						{
							OOBase::swap(prev->m_keys[prev->m_key_count++],page->m_keys[i]);
							OOBase::swap(prev->m_pages[prev->m_key_count],page->m_pages[i+1]);
							prev->m_pages[prev->m_key_count]->m_parent = prev;
						}
						--this->m_key_count;
						this->m_pages[pos+1] = NULL; // DEBUG
						for (i = pos;i < this->m_key_count;++i)
						{
							OOBase::swap(this->m_keys[i],this->m_keys[i+1]);
							OOBase::swap(this->m_pages[i+1],this->m_pages[i+2]);
						}
						tree->destroy_page(page);
					}
				}

				if (!this->m_parent && this->m_key_count == 0)
				{
					tree->m_root_page = this->m_pages[0];
					this->m_pages[0]->m_parent = NULL;
					tree->destroy_page(this);
				}
				return true;
			}
		};

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeInternalPageLeaf : public BTreeInternalPageBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeInternalPageBase<K,V,Compare,B,Allocator> baseClass;
			friend class BTreeLeafPage<K,V,Compare,B,Allocator>;

			baseClass* new_page(BTreeImpl<K,V,Compare,B,Allocator>* tree)
			{
				return tree->new_internal_leaf(this->m_parent,NULL);
			}

			void adjust_key(size_t pos)
			{
				if (pos > 0 && this->m_key_count > 0)
					this->m_keys[pos-1] = static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[pos])->m_data[0].first;
			}

		public:
			BTreeInternalPageLeaf(baseClass* parent, BTreeLeafPage<K,V,Compare,B,Allocator>* page) :
					baseClass(parent,page)
			{}

			bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				size_t page = baseClass::find_page(tree,value);
				return static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[page])->insert_i(tree,value,page);
			}

			bool remove(BTreeImpl<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
			{
				size_t pos = baseClass::find_page(compare);
				BTreeLeafPage<K,V,Compare,B,Allocator>* page = static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[pos]);
				if (!page->remove_i(tree,compare,pos))
					return false;

				if (page->m_size < page->s_min)
				{
					BTreeLeafPage<K,V,Compare,B,Allocator>* prev = pos > 0 ? static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[pos-1]) : NULL;
					BTreeLeafPage<K,V,Compare,B,Allocator>* next = pos < this->m_key_count ? static_cast<BTreeLeafPage<K,V,Compare,B,Allocator>*>(this->m_pages[pos+1]) : NULL;

					if (next && next->m_size > next->s_min)
					{
						size_t steal = (next->m_size - next->s_min + 1) / 2;
						size_t i = 0;
						for (; i < steal; ++i)
							OOBase::swap(page->m_data[page->m_size++],next->m_data[i]);
						for (; i < next->m_size; ++i)
							OOBase::swap(next->m_data[i - steal],next->m_data[i]);
						next->m_size -= steal;
						this->m_keys[pos] = next->m_data[0].first;
					}
					else if (prev && prev->m_size > prev->s_min)
					{
						size_t steal = (prev->m_size - prev->s_min + 1) / 2;
						size_t i = prev->m_size;
						while (i-- > 0)
							OOBase::swap(page->m_data[i],page->m_data[i+steal]);
						page->m_size += steal;
						prev->m_size -= steal;
						for (i = 0; i < steal; ++i)
							OOBase::swap(prev->m_data[prev->m_size + i],page->m_data[i]);
						this->m_keys[pos-1] = page->m_data[0].first;
					}
					else if (next)
					{
						size_t i = next->m_size;
						while (i-- > 0)
							OOBase::swap(next->m_data[i],next->m_data[i+page->m_size]);
						for (i = 0;i < page->m_size;++i)
							OOBase::swap(next->m_data[i],page->m_data[i]);
						next->m_size += page->m_size;

						if (pos < this->m_key_count)
						{
							K key;
							OOBase::swap(key,this->m_keys[pos]);
						}
						this->m_pages[pos] = NULL; // DEBUG
						--this->m_key_count;
						for (i = pos;i < this->m_key_count;++i)
						{
							OOBase::swap(this->m_keys[i],this->m_keys[i+1]);
							OOBase::swap(this->m_pages[i],this->m_pages[i+1]);
						}
						OOBase::swap(this->m_pages[i],this->m_pages[i+1]);
						page->destroy(tree);
					}
					else if (prev)
					{
						size_t i = 0;
						for (; i < page->m_size; ++i)
							OOBase::swap(page->m_data[i],prev->m_data[prev->m_size++]);

						if (pos < this->m_key_count)
						{
							K key;
							OOBase::swap(key,this->m_keys[pos]);
						}
						this->m_pages[pos] = NULL; // DEBUG
						for (i = pos;i < this->m_key_count;++i)
						{
							OOBase::swap(this->m_keys[i],this->m_keys[i+1]);
							OOBase::swap(this->m_pages[i+1],this->m_pages[i+2]);
						}
						--this->m_key_count;
						page->destroy(tree);
					}
				}

				if (!this->m_parent && this->m_key_count == 0)
				{
					tree->m_root_page = this->m_pages[0];
					this->m_pages[0]->m_parent = NULL;
					tree->destroy_page(this);
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

			void dump(String& str, unsigned int indent, BTreePageBase<K,V,Compare,B,Allocator>* parent)
			{
				assert(this->m_parent == parent);

				String str2;
				str2.printf("%*s",indent,"");
				str.append(str2);
				for (size_t i=0;i<m_size;++i)
				{
					if (m_data[i].first == 0)
						str2.printf("? ");
					else
						str2.printf("%c ",m_data[i].first);
					str.append(str2);
				}
				str2.printf("\n");
				str.append(str2);
			}

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

				tree->destroy_page(this);
			}

			bool insert(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value)
			{
				assert(!this->m_parent);
				return insert_i(tree,value,0);
			}

			const Pair<K,V>* find(const BTreeCompareBase<K,Compare>& compare) const
			{
				size_t pos = find_i(compare);
				return pos == size_t(-1) ? NULL : &m_data[pos];
			}

			bool remove(BTreeImpl<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare)
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
			Pair<K,V> m_data[B];

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

			bool insert_i(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value, size_t parent_pos)
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
					return split(tree,value,pos,parent_pos);

				return insert_entry(pos,value,parent_pos);
			}

			bool insert_entry(size_t insert_pos, const Pair<K,V>& value, size_t parent_pos)
			{
				for (size_t i = m_size; i-- > insert_pos;)
					OOBase::swap(m_data[i],m_data[i+1]);

				m_data[insert_pos] = value;
				++m_size;

				if (insert_pos == 0 && this->m_parent)
					static_cast<BTreeInternalPageLeaf<K,V,Compare,B,Allocator>*>(this->m_parent)->adjust_key(parent_pos);

				return true;
			}

			bool split(BTreeImpl<K,V,Compare,B,Allocator>* tree, const Pair<K,V>& value, size_t insert_pos, size_t parent_pos)
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

				// Find the half-way point
				size_t half = s_min;
				bool insert_into_us = (insert_pos < half);
				if (insert_pos != half)
				{
					if (insert_into_us)
						--half;
					if (!this->m_parent->insert_page(tree,sibling,m_data[half].first,parent_pos))
						return false;
				}
				else if (!this->m_parent->insert_page(tree,sibling,value.first,parent_pos))
					return false;

				if (insert_into_us)
				{
					for (size_t i = half; i < m_size; ++i)
						OOBase::swap(sibling->m_data[sibling->m_size++],m_data[i]);
					m_size = half;

					return insert_entry(insert_pos,value,parent_pos);
				}

				size_t i = half;
				for (;i < insert_pos; ++i)
					OOBase::swap(sibling->m_data[sibling->m_size++],m_data[i]);
				++sibling->m_size;
				for (; i < m_size; ++i)
					OOBase::swap(sibling->m_data[sibling->m_size++],m_data[i]);
				m_size = half;

				insert_pos -= half;
				sibling->m_data[insert_pos] = value;

				return true;
			}

			bool remove_i(BTreeImpl<K,V,Compare,B,Allocator>* tree, const BTreeCompareBase<K,Compare>& compare, size_t parent_pos)
			{
				size_t pos = find_i(compare);
				if (pos == size_t(-1))
					return false;

				// Swap out the value
				Pair<K,V>().swap(m_data[pos]);

				// Just ripple down
				--m_size;
				for (size_t i = pos; i < m_size; ++i)
					OOBase::swap(m_data[i],m_data[i+1]);

				if (!m_size && !this->m_parent)
				{
					tree->m_root_page = NULL;
					this->destroy(tree);
				}
				else if (pos == 0)
					static_cast<BTreeInternalPageLeaf<K,V,Compare,B,Allocator>*>(this->m_parent)->adjust_key(parent_pos);

				return true;
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
				leaf_page_t* page = NULL;
				return Allocator::allocate_new_ref(page,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				BTreeInternalPageLeaf<K,V,Compare,B,Allocator>* page = NULL;
				return Allocator::allocate_new_ref(page,parent,leaf) ? page : NULL;
			}

			internal_page_t* new_internal_page(internal_page_t* parent, internal_page_t* internal)
			{
				BTreeInternalPageInternal<K,V,Compare,B,Allocator>* page = NULL;
				return Allocator::allocate_new_ref(page,parent,internal) ? page : NULL;
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
				return m_allocator.allocate_new_ref(page,m_allocator,parent,prev,next) ? page : NULL;
			}

			internal_page_t* new_internal_leaf(internal_page_t* parent, leaf_page_t* leaf)
			{
				BTreeInternalPageLeaf<K,V,Compare,B,AllocatorInstance>* page = NULL;
				return m_allocator.allocate_new_ref(page,m_allocator,parent,leaf) ? page : NULL;
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

		template <typename K, typename V, typename Compare, size_t B, typename Allocator>
		class BTreeImpl : public BTreeBase<K,V,Compare,B,Allocator>
		{
			typedef BTreeBase<K,V,Compare,B,Allocator> baseClass;

		public:
			BTreeImpl(const Compare& comp = Compare()) : baseClass(), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			BTreeImpl(AllocatorInstance& allocator) : baseClass(allocator), m_compare(), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			BTreeImpl(const Compare& comp, AllocatorInstance& allocator) : baseClass(allocator), m_compare(comp), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{}

			BTreeImpl(const BTreeImpl& rhs) : baseClass(rhs), m_compare(rhs.m_compare), m_root_page(NULL), m_head(NULL), m_tail(NULL)
			{
				if (!insert(rhs.begin(),rhs.end()))
					OOBase_CallCriticalFailure(system_error());
			}

			~BTreeImpl()
			{
				static_assert(B > 2,"BTree must be of order > 2");

				if (m_root_page)
					m_root_page->destroy(this);
			}

			void swap(BTreeImpl& rhs)
			{
				OOBase::swap(m_compare,rhs.m_compare);
				OOBase::swap(m_root_page,rhs.m_root_page);
				OOBase::swap(m_head,rhs.m_head);
				OOBase::swap(m_tail,rhs.m_tail);
			}

			Compare m_compare;
			BTreePageBase<K,V,Compare,B,Allocator>* m_root_page;
			BTreeLeafPage<K,V,Compare,B,Allocator>* m_head;
			BTreeLeafPage<K,V,Compare,B,Allocator>* m_tail;
		};
	}

	template <typename K, typename V, typename Compare = Less<K>, size_t B = 8, typename Allocator = CrtAllocator>
	class BTree : public detail::BTreeImpl<K,V,Compare,B,Allocator>
	{
		typedef detail::BTreeImpl<K,V,Compare,B,Allocator> baseClass;

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

		BTree(const Compare& comp = Compare()) : baseClass(comp)
		{}

		BTree(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		BTree(const Compare& comp, AllocatorInstance& allocator) : baseClass(comp,allocator)
		{}

		BTree(const BTree& rhs) : baseClass(rhs)
		{
			if (!insert(rhs.begin(),rhs.end()))
				OOBase_CallCriticalFailure(system_error());
		}

		BTree& operator = (const BTree& rhs)
		{
			BTree(rhs).swap(*this);
			return *this;
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
			if (!this->m_root_page)
			{
				if (!(this->m_head = this->new_leaf(NULL,NULL,NULL)))
					return false;

				this->m_root_page = this->m_tail = this->m_head;
			}
			return this->m_root_page->insert(this,value);
		}

		bool insert(typename call_traits<K>::param_type key, typename call_traits<V>::param_type value)
		{
			return insert(OOBase::make_pair(key,value));
		}

		template <typename K1>
		bool remove(const K1& key)
		{
			return this->m_root_page && this->m_root_page->remove(this,detail::BTreeCompare<K,K1,Compare>(this->m_compare,key));
		}

		template <typename K1>
		bool exists(const K1& key) const
		{
			return (find_i<K1>(key) != NULL);
		}

		size_t size() const
		{
			return this->m_root_page ? this->m_root_page->size() : 0;
		}

		bool empty() const
		{
			return size() != 0;
		}

		iterator begin()
		{
			return this->m_head ? iterator(this,this->m_head->begin()) : end();
		}

		const_iterator cbegin() const
		{
			return this->m_head ? const_iterator(this,this->m_head->cbegin()) : cend();
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_tail ? iterator(this,this->m_tail->back()) : end());
		}

		const_iterator back() const
		{
			return (this->m_tail ? iterator(this,this->m_tail->back()) : end());
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
			if (this->m_root_page)
				this->m_root_page->dump(str,0,NULL);

			LOG_DEBUG(("BTree:\n%s",str.c_str()));
		}

		template <typename K1>
		bool find(const K1& key) const
		{
			return this->m_root_page && this->m_root_page->find(detail::BTreeCompare<K,K1,Compare>(this->m_compare,key)) != NULL;
		}
	};
}

#endif // OOBASE_BTREE_H_INCLUDED_
