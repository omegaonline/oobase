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

#ifndef OOBASE_UNIQUEPTR_H_INCLUDED_
#define OOBASE_UNIQUEPTR_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace detail
	{
		template <typename T>
		class UniquePtrBase : public NonCopyable, public SafeBoolean
		{
		public:
			T* release()
			{
				T* old = m_ptr;
				m_ptr = NULL;
				return old;
			}

			T* get() const
			{
				return m_ptr;
			}

			T& operator *() const
			{
				return *m_ptr;
			}

			T* operator ->() const
			{
				return m_ptr;
			}

			operator bool_type() const
			{
				return m_ptr ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
			}

		protected:
			UniquePtrBase(T* ptr = NULL) : m_ptr(ptr)
			{}

			T* m_ptr;
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class UniquePtr : public detail::UniquePtrBase<T>
	{
		typedef detail::UniquePtrBase<T> baseClass;

	public:
		explicit UniquePtr(T* ptr = NULL) : baseClass(ptr)
		{}

		~UniquePtr()
		{
			reset();
		}

		void reset(T* ptr = NULL)
		{
			T* old = baseClass::m_ptr;
			baseClass::m_ptr = ptr;
			if (old)
				Allocator::delete_free(old);
		}

		bool reallocate(size_t count)
		{
			reset(static_cast<T*>(Allocator::reallocate(baseClass::m_ptr,sizeof(T)*count,alignment_of<T>::value)));
			return *this;
		}
	};

	template <typename T>
	class UniquePtr<T,AllocatorInstance> : public detail::UniquePtrBase<T>
	{
		typedef detail::UniquePtrBase<T> baseClass;

	public:
		UniquePtr(AllocatorInstance& alloc) : baseClass(), m_alloc(alloc)
		{}

		explicit UniquePtr(T* ptr, AllocatorInstance& alloc) : baseClass(ptr), m_alloc(alloc)
		{}

		~UniquePtr()
		{
			reset();
		}

		void reset(T* ptr = NULL)
		{
			T* old = baseClass::m_ptr;
			baseClass::m_ptr = ptr;
			if (old)
				m_alloc.delete_free(old);
		}

		bool reallocate(size_t count)
		{
			reset(static_cast<T*>(m_alloc.reallocate(baseClass::m_ptr,sizeof(T)*count,alignment_of<T>::value)));
			return *this;
		}

	private:
		AllocatorInstance& m_alloc;
	};

	template <typename T, typename Allocator>
	class UniquePtr<T[],Allocator> : public UniquePtr<T,Allocator>
	{
	public:
		T& operator [](size_t i) const
		{
			return detail::UniquePtrBase<T>::m_ptr[i];
		}
	};

	template <class T>
	class TempPtr : public UniquePtr<T,AllocatorInstance>
	{
	public:
		TempPtr(AllocatorInstance& alloc) : UniquePtr<T,AllocatorInstance>(NULL,alloc)
		{}

		explicit TempPtr(T* ptr, AllocatorInstance& alloc) : UniquePtr<T,AllocatorInstance>(ptr,alloc)
		{}
	};

	template<class T1, class A1, class T2, class A2>
	bool operator == (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() == u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator != (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() != u2.get();
	}
}

#endif // OOBASE_UNIQUEPTR_H_INCLUDED_
