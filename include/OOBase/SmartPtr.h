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
		template <typename T, typename T_Deleter, typename Allocator>
		struct SmartPtrNode : public RefCounted
		{
			T* m_data;

			SmartPtrNode(T* p) : m_data(p)
			{}

			~SmartPtrNode()
			{
				T_Deleter::destroy(m_data);
			}

			static SmartPtrNode* create(T* data)
			{
				SmartPtrNode* p = NULL;
				if (!Allocator::allocate_new(p,data))
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				return p;
			}

			virtual void destroy()
			{
				Allocator::delete_free(this);
			}
		};

		template <typename T>
		struct SmartPtrNode<T,DeleterInstance,AllocatorInstance> : public RefCounted
		{
			T* m_data;
			DeleterInstance m_deleter;

			SmartPtrNode(AllocatorInstance& allocator, T* p) : m_data(p), m_deleter(allocator)
			{}

			~SmartPtrNode()
			{
				m_deleter.destroy(m_data);
			}

			static SmartPtrNode* create(AllocatorInstance& allocator, T* data)
			{
				SmartPtrNode* p = NULL;
				if (!allocator.allocate_new(p,allocator,data))
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				return p;
			}

			virtual void destroy()
			{
				DeleterInstance deleter(m_deleter);
				deleter.destroy(this);
			}
		};

		template <typename T, typename T_Deleter, typename Allocator>
		class SmartPtrImpl
		{
			typedef SmartPtrNode<T,T_Deleter,Allocator> nodeType;

		public:
			SmartPtrImpl(T* ptr) : m_node(NULL)
			{
				if (ptr)
					m_node = nodeType::create(ptr);
			}

			SmartPtrImpl(AllocatorInstance& allocator, T* ptr) : m_node(nodeType::create(allocator,ptr))
			{}

			operator T*()
			{
				return value();
			}

			operator const T*() const
			{
				return value();
			}

		protected:
			T* value()
			{
				return (m_node ? m_node->m_data : NULL);
			}

			const T* value() const
			{
				return (m_node ? m_node->m_data : NULL);
			}

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

	template <typename T, typename T_Deleter = Deleter<CrtAllocator> >
	class SmartPtr : public detail::SmartPtrImpl<T,T_Deleter,typename T_Deleter::Allocator>
	{
		typedef detail::SmartPtrImpl<T,T_Deleter,typename T_Deleter::Allocator> baseClass;

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

		T* operator ->()
		{
			assert(baseClass::value() != NULL);

			return baseClass::value();
		}

		const T* operator ->() const
		{
			assert(baseClass::value() != NULL);

			return baseClass::value();
		}
	};

	template <typename Destructor>
	class SmartPtr<void,Destructor> : public detail::SmartPtrImpl<void,Destructor,typename Destructor::Allocator>
	{
		typedef detail::SmartPtrImpl<void,Destructor,typename Destructor::Allocator> baseClass;

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

		template <typename T2>
		operator T2*()
		{
			return static_cast<T2*>(baseClass::value());
		}

		template <typename T2>
		operator const T2*() const
		{
			return static_cast<const T2*>(baseClass::value());
		}
	};
}

#endif // OOBASE_SMARTPTR_H_INCLUDED_
