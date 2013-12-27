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
	class StdDeleter
	{
	public:
		template <typename T>
		static void destroy(T* ptr)
		{
			delete ptr;
		}
	};

	class StdArrayDeleter
	{
	public:
		template <typename T>
		static void destroy(T* ptr)
		{
			delete [] ptr;
		}
	};

	template <typename A>
	class Deleter
	{
	public:
		typedef A Allocator;

		template <typename T>
		static void destroy(T* ptr)
		{
			A::delete_free(ptr);
		}

		template <typename T>
		static T* reallocate(T* ptr, size_t count)
		{
			return static_cast<T*>(A::reallocate(ptr,sizeof(T)*count,alignment_of<T>::value));
		}
	};

	class DeleterInstance
	{
	public:
		typedef AllocatorInstance Allocator;

		DeleterInstance(AllocatorInstance& alloc) : m_alloc(alloc)
		{}

		template <typename T>
		void destroy(T* ptr)
		{
			m_alloc.delete_free(ptr);
		}

		template <typename T>
		T* reallocate(T* ptr, size_t count)
		{
			return static_cast<T*>(m_alloc.reallocate(ptr,sizeof(T)*count,alignment_of<T>::value));
		}

	private:
		AllocatorInstance& m_alloc;
	};

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

	template <typename T, typename Deleter>
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
			if (baseClass::m_ptr != ptr)
			{
				T* old = baseClass::m_ptr;
				baseClass::m_ptr = ptr;
				if (old)
					Deleter::destroy(old);
			}
		}

		bool reallocate(size_t count)
		{
			reset(Deleter::reallocate(baseClass::m_ptr,count));
			return (baseClass::m_ptr != NULL);
		}
	};

	template <typename T>
	class UniquePtr<T,DeleterInstance> : public detail::UniquePtrBase<T>
	{
		typedef detail::UniquePtrBase<T> baseClass;

	public:
		UniquePtr(AllocatorInstance& alloc) : baseClass(), m_deleter(alloc)
		{}

		explicit UniquePtr(T* ptr, AllocatorInstance& alloc) : baseClass(ptr), m_deleter(alloc)
		{}

		~UniquePtr()
		{
			reset();
		}

		void reset(T* ptr = NULL)
		{
			if (baseClass::m_ptr != ptr)
			{
				T* old = baseClass::m_ptr;
				baseClass::m_ptr = ptr;
				if (old)
					m_deleter.destroy(old);
			}
		}

		bool reallocate(size_t count)
		{
			reset(m_deleter.reallocate(baseClass::m_ptr,count));
			return (baseClass::m_ptr != NULL);
		}

	protected:
		DeleterInstance m_deleter;
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
	class TempPtr : public UniquePtr<T,DeleterInstance>
	{
		typedef UniquePtr<T,DeleterInstance> baseClass;
	public:
		TempPtr(AllocatorInstance& alloc) : baseClass(NULL,alloc)
		{}

		explicit TempPtr(T* ptr, AllocatorInstance& alloc) : baseClass(ptr,alloc)
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
