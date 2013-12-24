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

namespace OOBase
{
	class StdDeleteDestructor
	{
	public:
		typedef CrtAllocator Allocator;

		template <typename T>
		static void destroy(T* ptr)
		{
			delete ptr;
		}
	};

	class StdArrayDeleteDestructor
	{
	public:
		typedef CrtAllocator Allocator;

		template <typename T>
		static void destroy(T* ptr)
		{
			delete [] ptr;
		}
	};

	template <typename A>
	class DeleteDestructor
	{
	public:
		typedef A Allocator;

		template <typename T>
		static void destroy(T* ptr)
		{
			ptr->~T();
			A::free(ptr);
		}
	};

	template <>
	class DeleteDestructor<AllocatorInstance>
	{
	public:
		typedef AllocatorInstance Allocator;

		template <typename T>
		static void destroy(AllocatorInstance& allocator, T* ptr)
		{
			allocator.delete_free(ptr);
		}
	};

	template <typename A>
	class FreeDestructor
	{
	public:
		typedef A Allocator;

		static void destroy(void* ptr)
		{
			A::free(ptr);
		}
	};

	template <>
	class FreeDestructor<AllocatorInstance>
	{
	public:
		typedef AllocatorInstance Allocator;

		static void destroy(AllocatorInstance& allocator, void* ptr)
		{
			allocator.free(ptr);
		}
	};

	namespace detail
	{
		template <typename T, typename T_Destructor, typename Allocator>
		struct SmartPtrNode : public RefCounted
		{
			T* m_data;

			SmartPtrNode(T* p) : m_data(p)
			{}

			~SmartPtrNode()
			{
				T_Destructor::destroy(m_data);
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

		template <typename T, typename T_Destructor>
		struct SmartPtrNode<T,T_Destructor,AllocatorInstance> : public RefCounted
		{
			T* m_data;
			AllocatorInstance& m_allocator;

			SmartPtrNode(AllocatorInstance& allocator, T* p) : m_data(p), m_allocator(allocator)
			{}

			~SmartPtrNode()
			{
				T_Destructor::destroy(m_allocator,m_data);
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
				AllocatorInstance& allocator = m_allocator;
				allocator.delete_free(this);
			}
		};

		template <typename T, typename T_Destructor, typename Allocator>
		class SmartPtrImpl
		{
			typedef SmartPtrNode<T,T_Destructor,Allocator> nodeType;

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

	template <typename T, typename T_Destructor = StdDeleteDestructor >
	class SmartPtr : public detail::SmartPtrImpl<T,T_Destructor,typename T_Destructor::Allocator>
	{
		typedef detail::SmartPtrImpl<T,T_Destructor,typename T_Destructor::Allocator> baseClass;

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

	namespace detail
	{
		template <typename T>
		class LocalPtrBase : public NonCopyable
		{
		public:
			LocalPtrBase(T* p) : m_data(p)
			{}

			T* detach()
			{
				T* p = m_data;
				m_data = NULL;
				return p;
			}

			operator T*()
			{
				return m_data;
			}

			operator const T*() const
			{
				return m_data;
			}

		protected:
			T* m_data;
		};

		template <typename T, typename T_Destructor, typename Allocator>
		class LocalPtrImpl : public LocalPtrBase<T>
		{
		public:
			LocalPtrImpl(T* p) : LocalPtrBase<T>(p)
			{}

			~LocalPtrImpl()
			{
				if (LocalPtrBase<T>::m_data)
					T_Destructor::destroy(LocalPtrBase<T>::m_data);
			}

		protected:
			void assign(T* p)
			{
				if (p != LocalPtrBase<T>::m_data)
				{
					if (LocalPtrBase<T>::m_data)
						T_Destructor::destroy(LocalPtrBase<T>::m_data);

					LocalPtrBase<T>::m_data = p;
				}
			}
		};

		template <typename T, typename T_Destructor>
		class LocalPtrImpl<T,T_Destructor,AllocatorInstance> : public LocalPtrBase<T>
		{
		public:
			LocalPtrImpl(AllocatorInstance& allocator, T* p) : LocalPtrBase<T>(p), m_allocator(allocator)
			{}

			~LocalPtrImpl()
			{
				if (LocalPtrBase<T>::m_data)
					T_Destructor::destroy(m_allocator,LocalPtrBase<T>::m_data);
			}

			AllocatorInstance& get_allocator() const
			{
				return m_allocator;
			}

		protected:
			AllocatorInstance& m_allocator;

			void assign(T* p)
			{
				if (p != LocalPtrBase<T>::m_data)
				{
					if (LocalPtrBase<T>::m_data)
						T_Destructor::destroy(m_allocator,LocalPtrBase<T>::m_data);

					LocalPtrBase<T>::m_data = p;
				}
			}
		};
	}

	template <typename T, typename Destructor = StdDeleteDestructor >
	class LocalPtr : public detail::LocalPtrImpl<T,Destructor,typename Destructor::Allocator>
	{
		typedef detail::LocalPtrImpl<T,Destructor,typename Destructor::Allocator> baseClass;

	public:
		LocalPtr(T* p = NULL) : baseClass(p)
		{}

		LocalPtr(AllocatorInstance& allocator, T* p = NULL) : baseClass(allocator,p)
		{}

		LocalPtr& operator = (T* p)
		{
			baseClass::assign(p);
			return *this;
		}

		T* operator ->()
		{
			assert(baseClass::m_data != NULL);

			return baseClass::m_data;
		}

		const T* operator ->() const
		{
			assert(baseClass::m_data != NULL);

			return baseClass::m_data;
		}
	};

	template <typename Destructor>
	class LocalPtr<void,Destructor> : public detail::LocalPtrImpl<void,Destructor,typename Destructor::Allocator>
	{
		typedef detail::LocalPtrImpl<void,Destructor,typename Destructor::Allocator> baseClass;

	public:
		LocalPtr(void* p = NULL) : baseClass(p)
		{}

		LocalPtr(AllocatorInstance& allocator, void* p = NULL) : baseClass(allocator,p)
		{}

		LocalPtr& operator = (void* p)
		{
			baseClass::assign(p);
			return *this;
		}

		template <typename T2>
		operator T2*()
		{
			return static_cast<T2*>(baseClass::m_data);
		}

		template <typename T2>
		operator const T2*() const
		{
			return static_cast<T2*>(baseClass::m_data);
		}
	};
}

#endif // OOBASE_SMARTPTR_H_INCLUDED_
