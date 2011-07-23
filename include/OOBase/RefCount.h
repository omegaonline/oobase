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

#ifndef OOBASE_REFCOUNT_H_INCLUDED_
#define OOBASE_REFCOUNT_H_INCLUDED_

#include "Atomic.h"

namespace OOBase
{
	class RefCounted
	{
	public:
		RefCounted() : m_refcount(1)
		{}

		/// Return a reference counted copy
		void addref()
		{
			++m_refcount;
		}

		/// Release a reference
		void release()
		{
			if (--m_refcount == 0)
				delete this;
		}

	protected:
		virtual ~RefCounted()
		{}

	private:
		// Prevent copying
		RefCounted(const RefCounted& rhs);
		RefCounted& operator = (const RefCounted& rhs);

		Atomic<size_t> m_refcount; ///< The reference count.
	};

	template <typename T>
	class RefPtr
	{
	public:
		RefPtr(T* ptr = NULL) : m_data(ptr)
		{ }

		RefPtr(const RefPtr& rhs) : m_data(rhs.m_data)
		{
			if (m_data)
				m_data->addref();
		}

		RefPtr& operator = (T* ptr)
		{
			if (m_data)
				m_data->release();
			
			m_data = ptr;
			return *this;
		}

		RefPtr& operator = (const RefPtr& rhs)
		{
			if (this != &rhs)
			{
				if (m_data)
					m_data->release();
				
				m_data = rhs.m_data;

				if (m_data)
					m_data->addref();
			}
			return *this;
		}

		~RefPtr()
		{
			if (m_data)
				m_data->release();
		}

		T* addref()
		{
			if (m_data)
				m_data->addref();
		
			return m_data;
		}

		T* operator ->()
		{
			return m_data;
		}

		const T* operator ->() const
		{
			return m_data;
		}

		operator T*()
		{
			return m_data;
		}

		operator const T*() const
		{
			return m_data;
		}

	private:
		T* m_data;
	};
}

#endif // OOBASE_REFCOUNT_H_INCLUDED_
