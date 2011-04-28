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

#ifndef OOBASE_STACK_H_INCLUDED_
#define OOBASE_STACK_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	template <typename T, typename Allocator = HeapAllocator<NoFailure> >
	class Stack
	{
	public:
		Stack() : m_data(NULL), m_capacity(0), m_top(0)
		{}
			
		~Stack()
		{
			clear();
			
			Allocator::free(m_data);
		}
			
		int reserve(size_t capacity)
		{
			return reserve_i(capacity + m_top);
		}
		
		int push(const T& value)
		{
			if (m_top+1 > m_capacity)
			{
				size_t cap = (m_capacity == 0 ? 4 : m_capacity*2);
				int err = reserve_i(cap);
				if (err != 0)
					return err;
			}
			
			// Placement new with copy constructor
			new (&m_data[m_top]) T(value);
			
			// This is exception safe as the increment happens here
			++m_top;
			return 0;
		}
		
		bool erase(const T& value)
		{
			// This is just really useful!
			size_t pos = 0;
			for (;pos < m_top;++pos)
			{
				if (m_data[pos] == value)
					break;
			}
			
			if (pos == m_top)
				return false;
			
			for(--m_top;pos < m_top;++pos)
				m_data[pos] = m_data[pos+1];
			
			m_data[pos].~T();
			
			return true;
		}
		
		void clear()
		{
			while (m_top > 0)
				m_data[--m_top].~T();
		}
		
		bool pop(T* value = NULL)
		{
			if (m_top == 0)
				return false;
			
			if (value)
				*value = m_data[m_top-1]; 
			
			m_data[--m_top].~T();
			return true;
		}
		
		bool empty() const
		{
			return (m_top == 0);
		}			
		
		size_t size() const
		{
			return m_top;
		}
			
	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur...
		// and you probably don't want to be copying these around
		Stack(const Stack&);
		Stack& operator = (const Stack&);
	
		T*     m_data;
		size_t m_capacity;
		size_t m_top;
	
		int reserve_i(size_t capacity)
		{
			if (m_capacity < capacity)
			{
				T* new_data = static_cast<T*>(Allocator::allocate(capacity*sizeof(T)));
				if (!new_data)
					return ERROR_OUTOFMEMORY;
				
				for (size_t i=0;i<m_top;++i)
					new (&new_data[i]) T(m_data[i]);
									
				for (size_t i=0;i<m_top;++i)
					m_data[i].~T();
					
				Allocator::free(m_data);
				m_data = new_data;
				m_capacity = capacity;
			}
			return 0;
		}
	};
}

#endif // OOBASE_STACK_H_INCLUDED_
