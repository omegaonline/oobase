///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#ifndef OOBASE_QUEUE_H_INCLUDED_
#define OOBASE_QUEUE_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	template <typename T, typename Allocator>
	class Queue
	{
	public:
		Queue() : m_data(NULL), m_capacity(0), m_front(0), m_back(0)
		{}
			
		~Queue()
		{
			clear();					
			Allocator::free(m_data);
		}
			
		int push(const T& val)
		{
			if (m_capacity == 0 || size() == (m_capacity - 1))
			{
				int err = grow();
				if (err != 0)
					return err;
			}
			
			// Placement new
			new (&m_data[m_back]) T(val);
				
			m_back = (m_back + 1) % m_capacity;
			return 0;
		}
		
		T* front()
		{
			if (m_front == m_back)
				return NULL;
			
			return &m_data[m_front];
		}
		
		bool pop(T* value = NULL)
		{
			if (m_front == m_back)
				return false;
						
			if (value)
				*value = m_data[m_front];
			
			m_data[m_front].~T();
			m_front = (m_front + 1) % m_capacity;
			return true;
		}
		
		void clear()
		{
			for (;m_capacity != 0 && m_front != m_back;m_front = (m_front+1) % m_capacity)
				m_data[m_front].~T();
		}
		
		size_t size() const
		{
			if (!m_capacity)
				return 0;

			return (m_capacity - m_front + m_back) % m_capacity;
		}
		
		bool empty() const
		{
			return (m_front == m_back);
		}
			
	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur... 
		// and you probably don't want to be copying these around
		Queue(const Queue&);
		Queue& operator = (const Queue&);
	
		T*     m_data;
		size_t m_capacity;
		size_t m_front;
		size_t m_back;
	
		int grow()
		{
			size_t new_size = (m_capacity == 0 ? 8 : m_capacity * 2);
			T* new_data = static_cast<T*>(Allocator::allocate(new_size*sizeof(T)));
			if (!new_data)
			{
#if defined(_WIN32)
				return ERROR_OUTOFMEMORY;
#else
				return ENOMEM;
#endif
			}
				
			// Now copy the contents of m_data into new_data
			size_t new_back = 0;
			for (size_t i=m_front;m_capacity != 0 && i != m_back; i = (i+1) % m_capacity)
				new (&new_data[new_back++]) T(m_data[i]);
			
			for (size_t i=m_front;m_capacity != 0 && i != m_back; i = (i+1) % m_capacity)
				m_data[i].~T();
						
			Allocator::free(m_data);
			m_data = new_data;
			m_capacity = new_size;
			m_front = 0;
			m_back = new_back;
			
			return 0;
		}
	};
}

#endif // OOBASE_QUEUE_H_INCLUDED_
