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
		template <typename T, typename Allocator>
		class UniquePtrBase :
				public NonCopyable,
				public SafeBoolean,
				public Allocating<Allocator>
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

			operator bool_type() const
			{
				return m_ptr != NULL ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
			}

		protected:
			UniquePtrBase(T* ptr) : m_ptr(ptr)
			{}

			UniquePtrBase(T* ptr, AllocatorInstance& alloc) : Allocating<Allocator>(alloc), m_ptr(ptr)
			{}

			T* m_ptr;
		};

		template <typename T, typename Allocator, bool POD = false>
		class UniquePtrImpl : public UniquePtrBase<T,Allocator>
		{
			typedef UniquePtrBase<T,Allocator> baseClass;

		public:
			~UniquePtrImpl()
			{
				reset();
			}

			void reset(T* ptr = NULL)
			{
				if (this->m_ptr != ptr)
				{
					T* old = this->m_ptr;
					this->m_ptr = ptr;
					if (old)
						baseClass::delete_free(old);
				}
			}

		protected:
			UniquePtrImpl(T* ptr) : baseClass(ptr)
			{}

			UniquePtrImpl(T* ptr, AllocatorInstance& alloc) : baseClass(ptr,alloc)
			{}
		};

		template <typename T, typename Allocator>
		class UniquePtrImpl<T,Allocator,true> : public UniquePtrBase<T,Allocator>
		{
			typedef UniquePtrBase<T,Allocator> baseClass;

		public:
			~UniquePtrImpl()
			{
				reset();
			}

			void reset(T* ptr = NULL)
			{
				if (this->m_ptr != ptr)
				{
					T* old = this->m_ptr;
					this->m_ptr = ptr;
					if (old)
						baseClass::free(old);
				}
			}

		protected:
			UniquePtrImpl(T* ptr) : baseClass(ptr)
			{}

			UniquePtrImpl(T* ptr, AllocatorInstance& alloc) : baseClass(ptr,alloc)
			{}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class UniquePtr : public detail::UniquePtrImpl<T,Allocator,detail::is_pod<T>::value>
	{
		typedef detail::UniquePtrImpl<T,Allocator,detail::is_pod<T>::value> baseClass;

	public:
		explicit UniquePtr(T* ptr = NULL) : baseClass(ptr)
		{}

		explicit UniquePtr(AllocatorInstance& alloc) : baseClass(NULL,alloc)
		{}

		UniquePtr(T* ptr, AllocatorInstance& alloc) : baseClass(ptr,alloc)
		{}

		T& operator *() const
		{
			return *this->m_ptr;
		}

		T* operator ->() const
		{
			return this->m_ptr;
		}
	};

	template <typename Allocator>
	class UniquePtr<void,Allocator> : public detail::UniquePtrImpl<void,Allocator,true>
	{
		typedef detail::UniquePtrImpl<void,Allocator,true> baseClass;

	public:
		explicit UniquePtr(void* ptr = NULL) : baseClass(ptr)
		{}

		explicit UniquePtr(AllocatorInstance& alloc) : baseClass(NULL,alloc)
		{}

		UniquePtr(void* ptr, AllocatorInstance& alloc) : baseClass(ptr,alloc)
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

	template<class T1, class A1, class T2, class A2>
	bool operator < (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() < u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator <= (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() <= u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator > (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() > u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator >= (const UniquePtr<T1, A1>& u1, const UniquePtr<T2, A2>& u2)
	{
		return u1.get() >= u2.get();
	}
}

#endif // OOBASE_UNIQUEPTR_H_INCLUDED_
