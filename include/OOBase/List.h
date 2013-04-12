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
	namespace detail
	{
		template <typename T>
		struct ListNode : public NonCopyable
		{
			ListNode(ListNode* prev, ListNode* next, const T& data) :
				m_prev(prev), m_next(next), m_data(data)
			{}

			ListNode* m_prev;
			ListNode* m_next;
			T         m_data;
		};

		template <typename T>
		struct ListBlock : public NonCopyable
		{
			ListBlock*     m_next;
			unsigned short m_mask;
		};

		template <typename Allocator, typename T, bool POD = false>
		class ListBase : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			ListBase() : baseClass(), m_head(NULL)
			{}

			ListBase(AllocatorInstance& allocator) : baseClass(allocator), m_head(NULL)
			{}

			~ListBase()
			{

			}

		protected:
			int alloc_node(ListNode<T>*& new_node, const T& value, ListNode<T>* prev, ListNode<T>* next)
			{
				ListBlock<T>*& ins = m_head;
				while (ins && ins->m_mask == 0)
					ins = ins->m_next;

				if (!ins)
				{
					ins = baseClass::allocate(sizeof(ListBlock<T>) + ((sizeof(ins->m_mask)*8) * sizeof(ListNode<T>)),alignment_of<ListBlock<T> >::value);
					if (!ins)
						return ERROR_OUTOFMEMORY;

					ins->m_mask = (unsigned short)(-1);
					ins->m_next = NULL;
				}

				unsigned int idx;
#if defined(HAVE___BUILTIN_FFS)
				idx = __builtin_ffs(ins->m_mask) - 1;
#elif defined(_MSC_VER)
				{
					unsigned long i = 0;
					_BitScanForward(&i,ins->m_mask);
					idx = i;
				}
#else
				{
					unsigned short idx_mask = (ins->m_mask & (~ins->m_mask + 1));
					unsigned short i0 = (idx_mask & 0xAAAA) ?  1 : 0;
					unsigned short i1 = (idx_mask & 0xCCCC) ?  2 : 0;
					unsigned short i2 = (idx_mask & 0xF0F0) ?  4 : 0;
					unsigned short i3 = (idx_mask & 0xFF00) ?  8 : 0;
					idx = i3 | i2 | i1 | i0;
				}
#endif
				return 0;
			}

			void free_node(ListNode<T>* node)
			{

			}

		private:
			ListBlock<T>* m_head;
		};

		template <typename Allocator, typename T>
		class ListBase<Allocator,T,true> : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			ListBase() : baseClass()
			{}

			ListBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			~ListBase()
			{

			}

		protected:

		private:

		};
	}

	template <typename T, typename Allocator>
	class List : public detail::ListBase<Allocator,T,detail::is_pod<T>::value>
	{
		typedef detail::ListBase<Allocator,T,detail::is_pod<T>::value> baseClass;

	public:
		typedef detail::IteratorImpl<List,T,detail::ListNode<T>*> iterator;
		typedef detail::IteratorImpl<const List,const T,const detail::ListNode<T>*> const_iterator;

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
			assert(before.check(this));
			return remove(pval,before.deref());
		}

		template <typename T1>
		bool remove(const T1& value)
		{
			iterator i = find(value);
			return remove_at(i);
		}

		bool pop_front(T* pval = NULL)
		{
			return (remove(pval,m_head) != NULL);
		}

		bool pop_back(T* pval = NULL)
		{
			return (remove(pval,m_tail) != NULL);
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
		detail::ListNode<T>* m_head;
		detail::ListNode<T>* m_tail;
		size_t               m_size;

		T* at(detail::ListNode<T>* node)
		{
			return (node ? &node->m_data : NULL);
		}

		const T* at(const detail::ListNode<T>* node) const
		{
			return (node ? &node->m_data : NULL);
		}

		detail::ListNode<T>* insert(const T& value, detail::ListNode<T>* next, int& err)
		{
			detail::ListNode<T>* prev = (next ? next->m_prev : m_tail);

			detail::ListNode<T>* new_node = NULL;
			err = baseClass::alloc_node(new_node,value,prev,next);
			if (err)
				return NULL;

			(next ? next->m_prev : m_tail) = new_node;
			(prev ? prev->m_next : m_head) = new_node;

			++m_size;
			return new_node;
		}

		bool remove(T* pval, detail::ListNode<T>*& curr)
		{
			if (!curr)
				return false;

			detail::ListNode<T>* next = curr->m_next;
			(curr->m_next ? curr->m_next : m_tail) = curr->m_prev;
			(curr->m_prev ? curr->m_prev : m_head) = next;

			baseClass:free_node(curr);

			--m_size;
			curr = next;
			return true;
		}
	};
}

#endif // OOBASE_LIST_H_INCLUDED_
