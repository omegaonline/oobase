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
					m_node = nodeType::create(ptr);
			}

			SmartPtrImpl(AllocatorInstance& allocator, T* ptr) : m_node(nodeType::create(allocator,ptr))
			{}

			T* get()
			{
				return (m_node ? m_node->m_data : NULL);
			}

			const T* get() const
			{
				return (m_node ? m_node->m_data : NULL);
			}

			operator bool_type() const
			{
				return get() != NULL ? &SafeBoolean::this_type_does_not_support_comparisons : NULL;
			}

		protected:
			void assign(T* ptr)
			{
				if (ptr)
					m_node = nodeType::create(ptr);
				else
					m_node = NULL;
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
		SmartPtr(T* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
		{}

		SmartPtr& operator = (T* p)
		{
			baseClass::assign(p);
			return *this;
		}

		T& operator *()
		{
			return *baseClass::get();
		}

		const T& operator *() const
		{
			return *baseClass::get();
		}

		T* operator ->()
		{
			return baseClass::get();
		}

		const T* operator ->() const
		{
			return baseClass::get();
		}
	};

	template <typename Allocator>
	class SmartPtr<void,Allocator> : public detail::SmartPtrImpl<void,Allocator>
	{
		typedef detail::SmartPtrImpl<void,Allocator> baseClass;

	public:
		SmartPtr(void* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(AllocatorInstance& allocator, void* ptr = NULL) : baseClass(allocator,ptr)
		{}

		SmartPtr& operator = (void* p)
		{
			baseClass::assign(p);
			return *this;
		}
	};
}

#endif // OOBASE_SMARTPTR_H_INCLUDED_
