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

#ifndef OOBASE_STACKPTR_H_INCLUDED_
#define OOBASE_STACKPTR_H_INCLUDED_

#include "SmartPtr.h"

namespace OOBase
{
	template <typename T, size_t COUNT = 256/sizeof(T), typename Allocator = ThreadLocalAllocator>
	class StackArrayPtr
	{
	public:
		StackArrayPtr() : m_data(m_static), m_count(COUNT)
		{}

		StackArrayPtr(size_t count) : m_data(m_static), m_count(COUNT)
		{
			reallocate(count);
		}

		~StackArrayPtr()
		{}

		bool reallocate(size_t count)
		{
			if (count > COUNT)
			{
				m_data = m_dynamic = static_cast<T*>(Allocator::allocate(count * sizeof(T)));
				if (!m_data)
				{
					m_count = 0;
					return false;
				}

				m_count = count;
			}
			return true;
		}

		operator T* ()
		{
			return m_data;
		}

		operator const T* () const
		{
			return m_data;
		}

		size_t size() const
		{
			return m_count * sizeof(T);
		}

		size_t count() const
		{
			return m_count;
		}

	private:
		// No copying or assignment - these are stack allocated
		StackArrayPtr(const StackArrayPtr&);
		StackArrayPtr& operator = (const StackArrayPtr&);

		T           m_static[COUNT];
		T*          m_data;
		size_t      m_count;

		SmartPtr<T,FreeDestructor<Allocator> > m_dynamic;
	};
}

#endif // OOBASE_STACKPTR_H_INCLUDED_
