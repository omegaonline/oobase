///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#ifndef OOBASE_LIST_H_INCLUDED_
#define OOBASE_LIST_H_INCLUDED_

#include "Bag.h"

namespace OOBase
{
	template <typename T, typename Allocator = CrtAllocator>
	class List : public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;

		struct ListNode
		{
			ListNode(ListNode* prev, ListNode* next, const T& data) :
				m_prev(prev), m_next(next), m_data(data)
			{}

			ListNode* m_prev;
			ListNode* m_next;
			T         m_data;
		};

		friend class detail::IteratorImpl<List,T,ListNode*>;
		friend class detail::IteratorImpl<const List,const T,const ListNode*>;

	public:
		typedef detail::IteratorImpl<List,T,ListNode*> iterator;
		typedef detail::IteratorImpl<const List,const T,const ListNode*> const_iterator;

		List() : baseClass(), m_head(NULL), m_tail(m_head), m_size(0)
		{}

		List(AllocatorInstance& allocator) : baseClass(allocator), m_head(NULL), m_tail(m_head), m_size(0)
		{}

		~List()
		{
			clear();
		}

		void clear()
		{
			while (pop_back())
				;
		}

		bool empty() const
		{
			return (m_size == 0);
		}

		size_t size() const
		{
			return m_size;
		}

		iterator insert(const T& value, const iterator& before, int& err)
		{
			assert(before.check(this));
			return iterator(*this,insert(value,before.deref(),err));
		}

		iterator push_back(const T& value, int& err)
		{
			return iterator(*this,insert(value,NULL,err));
		}

		int push_front(const T& value)
		{
			int err = 0;
			insert(value,m_head,err);
			return err;
		}

		bool remove_at(iterator& iter)
		{
			assert(iter.check(this));
			return remove(NULL,iter.deref());
		}

		template <typename T1>
		bool remove(const T1& value)
		{
			iterator i = find(value);
			return remove_at(i);
		}

		bool pop_front(T* pval = NULL)
		{
			return remove(pval,m_head);
		}

		bool pop_back(T* pval = NULL)
		{
			return remove(pval,m_tail);
		}

		template <typename T1>
		iterator find(const T1& value)
		{
			iterator i = begin();
			for (;i != end();++i)
			{
				if (*i == value)
					break;
			}
			return i;
		}

		template <typename T1>
		const_iterator find(const T1& value) const
		{
			const_iterator i = begin();
			for (;i != end();++i)
			{
				if (*i == value)
					break;
			}
			return i;
		}

		T* at(const iterator& iter)
		{
			assert(iter.check(this));
			return at(iter.deref());
		}

		const T* at(const iterator& iter) const
		{
			assert(iter.check(this));
			return at(iter.deref());
		}

		const T* at(const const_iterator& iter) const
		{
			assert(iter.check(this));
			return at(iter.deref());
		}

		iterator begin()
		{
			return iterator(*this,m_head);
		}

		const_iterator begin() const
		{
			return const_iterator(*this,m_head);
		}

		iterator end()
		{
			return iterator(*this,NULL);
		}

		const_iterator end() const
		{
			return const_iterator(*this,NULL);
		}

	private:
		ListNode* m_head;
		ListNode* m_tail;
		size_t    m_size;

		T* at(ListNode* node)
		{
			return (node ? &node->m_data : NULL);
		}

		const T* at(const ListNode* node) const
		{
			return (node ? &node->m_data : NULL);
		}

		void next(ListNode*& node) const
		{
			if (node)
				node = node->m_next;
		}

		void next(const ListNode*& node) const
		{
			if (node)
				node = node->m_next;
		}

		void prev(ListNode*& node) const
		{
			if (node)
				node = node->m_prev;
		}

		void prev(const ListNode*& node) const
		{
			if (node)
				node = node->m_prev;
		}

		ListNode* insert(const T& value, ListNode* next, int& err)
		{
			ListNode* prev = (next ? next->m_prev : m_tail);

			ListNode* new_node = NULL;
			err = baseClass::allocate_new(new_node,prev,next,value);
			if (err)
				return NULL;

			(next ? next->m_prev : m_tail) = new_node;
			(prev ? prev->m_next : m_head) = new_node;

			++m_size;
			return new_node;
		}

		bool remove(T* pval, ListNode*& curr)
		{
			if (!curr)
				return false;

			if (pval)
				*pval = curr->m_data;

			ListNode* next = curr->m_next;
			(curr->m_next ? curr->m_next : m_tail) = curr->m_prev;
			(curr->m_prev ? curr->m_prev : m_head) = next;

			baseClass::delete_free(curr);

			--m_size;
			curr = next;
			return true;
		}
	};
}

#endif // OOBASE_LIST_H_INCLUDED_
