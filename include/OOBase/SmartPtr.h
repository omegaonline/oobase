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

#ifndef OOBASE_SMARTPTR_H_INCLUDED_
#define OOBASE_SMARTPTR_H_INCLUDED_

#include "RefCount.h"
#include "UniquePtr.h"

namespace OOBase
{
	namespace detail
	{
		template <typename T, typename Allocator>
		struct SmartPtrNode : public RefCounted, public Allocating<Allocator>
		{
			T* m_data;

			SmartPtrNode(T* p) : m_data(p)
			{}

			SmartPtrNode(T* p, AllocatorInstance& allocator) : Allocating<Allocator>(allocator), m_data(p)
			{}

			~SmartPtrNode()
			{
				Allocating<Allocator>::delete_free(m_data);
			}

			static SmartPtrNode* create(T* data)
			{
				SmartPtrNode* p = NULL;
				if (!Allocating<Allocator>::allocate_new(p,data))
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				return p;
			}

			virtual void destroy()
			{
				Allocating<Allocator>::delete_this(this);
			}
		};

		template <typename T, typename Allocator>
		class SmartPtrImpl : public SafeBoolean
		{
			typedef SmartPtrNode<T,Allocator> nodeType;

		public:
			SmartPtrImpl(T* ptr) : m_node(NULL)
			{
				if (ptr)
				{
					nodeType* node = NULL;
					if (!Allocator::allocate_new(node,ptr))
						OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
					m_node = node;
				}
			}

			SmartPtrImpl(T* ptr, AllocatorInstance& allocator) : m_node(NULL)
			{
				if (ptr)
				{
					nodeType* node = NULL;
					if (!allocator.allocate_new(node,ptr,allocator))
						OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
					m_node = node;
				}
			}

			T* get() const
			{
				return (m_node ? m_node->m_data : NULL);
			}

			operator bool_type() const
			{
				return get() != NULL ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
			}

		private:
			RefPtr<nodeType> m_node;
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class SmartPtr : public detail::SmartPtrImpl<T,Allocator>
	{
		typedef detail::SmartPtrImpl<T,Allocator> baseClass;

	public:
		explicit SmartPtr(T* ptr = NULL) : baseClass(ptr)
		{}

		explicit SmartPtr(T* ptr, AllocatorInstance& allocator) : baseClass(ptr,allocator)
		{}

		SmartPtr(AllocatorInstance& allocator) : baseClass(NULL,allocator)
		{}

		T& operator *() const
		{
			return *baseClass::get();
		}

		T* operator ->() const
		{
			return baseClass::get();
		}
	};

	template <typename Allocator>
	class SmartPtr<void,Allocator> : public detail::SmartPtrImpl<void,Allocator>
	{
		typedef detail::SmartPtrImpl<void,Allocator> baseClass;

	public:
		explicit SmartPtr(void* ptr = NULL) : baseClass(ptr)
		{}

		explicit SmartPtr(void* ptr, AllocatorInstance& allocator) : baseClass(ptr,allocator)
		{}

		SmartPtr(AllocatorInstance& allocator) : baseClass(NULL,allocator)
		{}
	};

	template<class T1, class A1, class T2, class A2>
	bool operator == (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() == u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator != (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() != u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator < (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() < u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator <= (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() <= u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator > (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() > u2.get();
	}

	template<class T1, class A1, class T2, class A2>
	bool operator >= (const SmartPtr<T1, A1>& u1, const SmartPtr<T2, A2>& u2)
	{
		return u1.get() >= u2.get();
	}
}

#endif // OOBASE_SMARTPTR_H_INCLUDED_
