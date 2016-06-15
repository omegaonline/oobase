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

#include "Memory.h"
#include "Iterator.h"

namespace OOBase
{
	template <typename T, typename Allocator = CrtAllocator>
	class List : public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;

		struct ListNode
		{
			template <typename T1>
			ListNode(ListNode* prev, ListNode* next, T1 data) :
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

		List() : baseClass(), m_head(NULL), m_tail(m_head), m_size(0), m_end(NULL,NULL), m_cend(NULL,NULL)
		{
			iterator(this,NULL).swap(m_end);
			const_iterator(this,NULL).swap(m_cend);
		}
		
		List(AllocatorInstance& allocator) : baseClass(allocator), m_head(NULL), m_tail(m_head), m_size(0), m_end(NULL,NULL), m_cend(NULL,NULL)
		{
			iterator(this,NULL).swap(m_end);
			const_iterator(this,NULL).swap(m_cend);
		}
		
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

		iterator insert(typename call_traits<T>::param_type value, const iterator& before)
		{
			assert(before.check(this));
			return iterator(this,insert_before(value,before.deref()));
		}

		iterator push_back(typename call_traits<T>::param_type value)
		{
			return iterator(this,insert_tail(value));
		}

		iterator push_front(typename call_traits<T>::param_type value)
		{
			return iterator(this,insert_head(value));
		}

		void remove_at(iterator& iter)
		{
			assert(iter.check(this));
			remove(NULL,iter.deref());
		}

		template <typename T1>
		bool remove(const T1& value)
		{
			iterator i = find(value);
			if (i == m_end)
				return false;

			remove_at(i);
			return true;
		}

		bool pop_front(T* pval = NULL)
		{
			ListNode* p = m_head;
			return remove(pval,p);
		}

		bool pop_back(T* pval = NULL)
		{
			ListNode* p = m_tail;
			return remove(pval,p);
		}

		template <typename T1>
		iterator find(const T1& value)
		{
			iterator i = begin();
			for (;i != m_end;++i)
			{
				if (*i == value)
					break;
			}
			return i;
		}

		template <typename T1>
		const_iterator find(const T1& value) const
		{
			const_iterator i = cbegin();
			for (;i != m_end;++i)
			{
				if (*i == value)
					break;
			}
			return i;
		}

		iterator begin()
		{
			return iterator(this,m_head);
		}

		const_iterator cbegin() const
		{
			return const_iterator(this,m_head);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		const iterator& end()
		{
			return m_end;
		}

		const const_iterator& cend() const
		{
			return m_cend;
		}

		const const_iterator& end() const
		{
			return m_cend;
		}

	private:
		ListNode* m_head;
		ListNode* m_tail;
		size_t    m_size;

		iterator       m_end;
		const_iterator m_cend;

		T* at(ListNode* node)
		{
			return (node ? &node->m_data : NULL);
		}

		const T* at(const ListNode* node) const
		{
			return (node ? &node->m_data : NULL);
		}

		template <typename N>
		void iterator_move(N& node, ptrdiff_t n) const
		{
			if (!node && n < 0)
			{
				node = m_tail;
				n++;
			}
			for (ptrdiff_t i = 0;i < n && node;++i)
				node = node->m_next;
			for (ptrdiff_t i = n;i < 0 && node;++i)
				node = node->m_prev;
		}

		template <typename N>
		ptrdiff_t iterator_diff(N pos1, N pos2) const
		{
			ptrdiff_t r = -1;
			N pos = pos2;
			for (;pos != pos1 && pos;pos = pos->m_next)
				++r;

			if (!pos)
			{
				r = 0;
				for (pos = pos2;pos != pos1 && pos;pos = pos->m_prev)
					--r;

				assert(pos);
			}
			return r;
		}

		ListNode* insert_before(typename call_traits<T>::param_type value, ListNode* before)
		{
			ListNode* new_node = NULL;
			if (!baseClass::allocate_new_ref(new_node,before->m_prev,before,value))
				return NULL;

			if (!before->m_prev)
				m_head = new_node;
			else
				before->m_prev->m_next = new_node;

			before->m_prev = new_node;

			++m_size;
			return new_node;
		}

		ListNode* insert_head(typename call_traits<T>::param_type value)
		{
			if (m_head)
				return insert_before(value,m_head);

			ListNode* null = NULL;
			ListNode* new_node = NULL;
			if (!baseClass::allocate_new_ref(new_node,null,null,value))
				return NULL;

			m_head = new_node;
			m_tail = new_node;

			++m_size;
			return new_node;
		}

		ListNode* insert_tail(typename call_traits<T>::param_type value)
		{
			if (!m_tail)
				return insert_head(value);

			ListNode* null = NULL;
			ListNode* new_node = NULL;
			if (!baseClass::allocate_new_ref(new_node,m_tail,null,value))
				return NULL;

			m_tail->m_next = new_node;
			m_tail = new_node;

			++m_size;
			return new_node;
		}

		bool remove(T* pval, ListNode*& curr)
		{
			if (!curr)
				return false;

			if (pval)
				*pval = curr->m_data;

			if (curr->m_prev)
				curr->m_prev->m_next = curr->m_next;
			else
				m_head = curr->m_next;

			if (curr->m_next)
				curr->m_next->m_prev = curr->m_prev;
			else
				m_tail = curr->m_prev;

			ListNode* next = curr->m_next;
			baseClass::delete_free(curr);

			--m_size;
			curr = next;
			return true;
		}
	};
}

#endif // OOBASE_LIST_H_INCLUDED_
