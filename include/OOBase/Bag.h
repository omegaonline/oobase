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

#ifndef OOBASE_BAG_H_INCLUDED_
#define OOBASE_BAG_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	template <typename T, typename Allocator = HeapAllocator>
	class Bag
	{
	public:
		Bag() : m_data(NULL), m_size(0), m_capacity(0)
		{}

		~Bag()
		{
			clear();

			Allocator::free(m_data);
		}

		int reserve(size_t capacity)
		{
			return reserve_i(capacity + m_size);
		}

		int add(const T& value)
		{
			if (m_size+1 > m_capacity)
			{
				size_t cap = (m_capacity == 0 ? 4 : m_capacity*2);
				int err = reserve_i(cap);
				if (err != 0)
					return err;
			}

			// Placement new with copy constructor
			::new (&m_data[m_size]) T(value);

			// This is exception safe as the increment happens here
			++m_size;
			return 0;
		}

		bool remove(const T& value)
		{
			// This is just really useful!
			for (size_t pos = 0;pos < m_size;++pos)
			{
				if (m_data[pos] == value)
				{
					remove_at(pos);
					return true;
				}
			}

			return false;
		}

		bool pop(T* value = NULL)
		{
			if (m_size == 0)
				return false;

			if (value)
				*value = m_data[m_size-1];

			m_data[--m_size].~T();
			return true;
		}

		void remove_at(size_t pos)
		{
			if (m_data && pos < m_size)
			{
				m_data[pos] = m_data[--m_size];
				m_data[m_size].~T();
			}
		}

		void clear()
		{
			while (m_size > 0)
				m_data[--m_size].~T();
		}

		bool empty() const
		{
			return (m_size == 0);
		}

		size_t size() const
		{
			return m_size;
		}

		T* at(size_t pos)
		{
			return (pos >= m_size ? NULL : &m_data[pos]);
		}

		const T* at(size_t pos) const
		{
			return (pos >= m_size ? NULL : &m_data[pos]);
		}

	protected:
		T*     m_data;
		size_t m_size;

		void remove_at_sorted(size_t pos)
		{
			if (m_data && pos < m_size)
			{
				for(--m_size;pos < m_size;++pos)
					m_data[pos] = m_data[pos+1];

				m_data[m_size].~T();
			}
		}

	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur...
		// and you probably don't want to be copying these around
		Bag(const Bag&);
		Bag& operator = (const Bag&);

		size_t m_capacity;

		int reserve_i(size_t capacity)
		{
			if (m_capacity < capacity)
			{
				T* new_data = static_cast<T*>(Allocator::allocate(capacity*sizeof(T)));
				if (!new_data)
					return ERROR_OUTOFMEMORY;

#if defined(OOBASE_HAVE_EXCEPTIONS)
				size_t i = 0;
				try
				{
					for (i=0;i<m_size;++i)
						::new (&new_data[i]) T(m_data[i]);
				}
				catch (...)
				{
					for (;i>0;--i)
						new_data[i-1].~T();

					Allocator::free(new_data);
					throw;
				}

				for (i=0;i<m_size;++i)
					m_data[i].~T();
#else
				for (size_t i=0;i<m_size;++i)
				{
					::new (&new_data[i]) T(m_data[i]);
					new_data[i-1].~T();
				}
#endif
				Allocator::free(m_data);
				m_data = new_data;
				m_capacity = capacity;
			}
			return 0;
		}
	};
}

#endif // OOBASE_BAG_H_INCLUDED_
