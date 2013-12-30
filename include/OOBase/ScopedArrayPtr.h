///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#ifndef OOBASE_SCOPED_ARRAY_PTR_H_INCLUDED_
#define OOBASE_SCOPED_ARRAY_PTR_H_INCLUDED_

#include "Vector.h"

namespace OOBase
{
	template <typename T, typename Allocator = ThreadLocalAllocator, size_t COUNT = 256/sizeof(T)>
	class ScopedArrayPtr : public NonCopyable, public SafeBoolean
	{
	public:
		ScopedArrayPtr() : m_data(m_static)
		{}

		ScopedArrayPtr(AllocatorInstance& alloc) : m_data(m_static), m_dynamic(alloc)
		{}

		ScopedArrayPtr(size_t count) : m_data(m_static)
		{
			reallocate(count);
		}

		ScopedArrayPtr(size_t count, AllocatorInstance& alloc) : m_data(m_static), m_dynamic(alloc)
		{
			reallocate(count);
		}

		~ScopedArrayPtr()
		{
			static_assert(detail::is_pod<T>::value,"ScopedArrayPtr must be of a POD type");
		}

		bool reallocate(size_t count)
		{
			if (m_data == m_static)
			{
				if (count > COUNT)
				{
					for (size_t i=0;i<COUNT;++i)
					{
						int err = m_dynamic.push_back(m_static[i]);
						if (err)
							return false;
					}
					m_data = m_dynamic.data();
				}
			}
			else
			{
				if (m_dynamic.resize(count) != 0)
				{
					m_data = NULL;
					return false;
				}
				m_data = m_dynamic.data();
			}
			return true;
		}

		T* get() const
		{
			return m_data;
		}

		size_t size() const
		{
			return count() * sizeof(T);
		}

		size_t count() const
		{
			return (m_data == m_static ? COUNT : m_dynamic.size());
		}

		operator bool_type() const
		{
			return m_data != NULL ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
		}

	private:
		T           m_static[COUNT];
		T*          m_data;

		Vector<T,Allocator> m_dynamic;
	};
}


#endif // OOBASE_SCOPED_ARRAY_PTR_H_INCLUDED_
