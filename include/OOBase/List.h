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

		template <typename T, typename Allocator>
		class SlabContainer : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

			struct SlabBlock : public NonCopyable
			{
				SlabBlock*     m_next;
				unsigned short m_mask;
				T              m_data[sizeof(unsigned short)*8];
			};

		public:
			SlabContainer() : baseClass(), m_block_head(NULL)
			{}

			SlabContainer(AllocatorInstance& allocator) : baseClass(allocator), m_block_head(NULL)
			{}

			~SlabContainer()
			{
				while (m_block_head)
				{
					SlabBlock* next = m_block_head->m_next;
					assert(m_block_head->m_mask == (unsigned short)(-1));
					baseClass::free(m_block_head);
					m_block_head = next;
				}
			}

		protected:
			int alloc_node(T*& new_node)
			{
				SlabBlock*& block = m_block_head;
				while (block && block->m_mask == 0)
					block = block->m_next;

				if (!block)
				{
					block = static_cast<SlabBlock*>(baseClass::allocate(sizeof(SlabBlock),alignment_of<SlabBlock>::value));
					if (!block)
						return ERROR_OUTOFMEMORY;

					block->m_mask = (unsigned short)(-1);
					block->m_next = NULL;
				}

				// Find the index of the lowest set bit in block->mask
				unsigned int idx = ffs(block->m_mask);
				new_node = block->m_data[idx];

				// Clear the bit
				block->m_mask &= ~(1 << idx);
				return 0;
			}

			void free_node(T* node)
			{

			}

		private:
			SlabBlock* m_block_head;
		};

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
	}

	template <typename T, typename Allocator = CrtAllocator>
	class List : public detail::SlabContainer<detail::ListNode<T>,Allocator>
	{
		typedef detail::SlabContainer<detail::ListNode<T>,Allocator> baseClass;

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

#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				new_node = ::new (new_node) detail::ListNode<T>(prev,next,value);
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				baseClass::free_node(new_node);
				throw;
			}
#endif
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
