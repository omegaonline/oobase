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
		template <typename T, typename Destructor, typename Allocator>
		class SmartPtrNode : public RefCounted
		{
		public:
			T* m_data;

			static SmartPtrNode* create(T* data)
			{
				void* p = Allocator::allocate(sizeof(SmartPtrNode),alignof<SmartPtrNode>::value);
				if (!p)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

				return ::new (p) SmartPtrNode(data);
			}

		protected:
			SmartPtrNode(T* p) : m_data(p)
			{}

			~SmartPtrNode()
			{
				Destructor::destroy(m_data);
			}

			virtual void destroy()
			{
				this->~SmartPtrNode();
				Allocator::free(this);
			}
		};

		template <typename T, typename Destructor>
		class SmartPtrNode<T,Destructor,AllocatorInstance> : public RefCounted
		{
		public:
			T* m_data;

			static SmartPtrNode* create(AllocatorInstance& allocator, T* data)
			{
				void* p = allocator.allocate(sizeof(SmartPtrNode),alignof<SmartPtrNode>::value);
				if (!p)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

				return ::new (p) SmartPtrNode(allocator,data);
			}

		private:
			AllocatorInstance& m_allocator;

			SmartPtrNode(AllocatorInstance& allocator, T* p) : m_data(p), m_allocator(allocator)
			{}

			~SmartPtrNode()
			{
				Destructor::destroy(m_allocator,m_data);
			}

			virtual void destroy()
			{
				AllocatorInstance& allocator = m_allocator;
				this->~SmartPtrNode();
				allocator.free(this);
			}
		};

		template <typename T, typename Destructor, typename Allocator>
		class SmartPtrImpl
		{
			typedef SmartPtrNode<T,Destructor,Allocator> nodeType;

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

	template <typename T, typename Destructor = StdDeleteDestructor >
	class SmartPtr : public detail::SmartPtrImpl<T,Destructor,typename Destructor::Allocator>
	{
		typedef detail::SmartPtrImpl<T,Destructor,typename Destructor::Allocator> baseClass;

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
			return static_cast<T2*>(baseClass::value());
		}
	};

	namespace detail
	{
		template <typename T>
		class LocalPtrBase
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

		private:
			LocalPtrBase(const LocalPtrBase&);
			LocalPtrBase& operator = (const LocalPtrBase&);
		};

		template <typename T, typename Destructor, typename Allocator>
		class LocalPtrImpl : public LocalPtrBase<T>
		{
		public:
			LocalPtrImpl(T* p) : LocalPtrBase<T>(p)
			{}

			~LocalPtrImpl()
			{
				if (LocalPtrBase<T>::m_data)
					Destructor::destroy(LocalPtrBase<T>::m_data);
			}

		protected:
			void assign(T* p)
			{
				if (p != LocalPtrBase<T>::m_data)
				{
					if (LocalPtrBase<T>::m_data)
						Destructor::destroy(LocalPtrBase<T>::m_data);

					LocalPtrBase<T>::m_data = p;
				}
			}
		};

		template <typename T, typename Destructor>
		class LocalPtrImpl<T,Destructor,AllocatorInstance> : public LocalPtrBase<T>
		{
		public:
			LocalPtrImpl(AllocatorInstance& allocator, T* p) : LocalPtrBase<T>(p), m_allocator(allocator)
			{}

			~LocalPtrImpl()
			{
				if (LocalPtrBase<T>::m_data)
					Destructor::destroy(m_allocator,LocalPtrBase<T>::m_data);
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
						Destructor::destroy(m_allocator,LocalPtrBase<T>::m_data);

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

	template <typename T>
	class TempPtr : public detail::LocalPtrImpl<T,FreeDestructor<AllocatorInstance>,AllocatorInstance>
	{
		typedef detail::LocalPtrImpl<T,FreeDestructor<AllocatorInstance>,AllocatorInstance> baseClass;

	public:
		TempPtr(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
		{
			static_assert(!detail::is_pod<T>::value,"Do not use POD types in TempPtr");
		}

		TempPtr& operator = (T* p)
		{
			baseClass::assign(p);
			return *this;
		}

		bool reallocate(size_t count)
		{
			T* p = static_cast<T*>(baseClass::m_allocator.reallocate(baseClass::m_data,sizeof(T)*count,alignof<T>::value));
			if (p)
				baseClass::m_data = p;

			return (p ? true : false);
		}
	};

	template <>
	class TempPtr<void> : public detail::LocalPtrImpl<void,FreeDestructor<AllocatorInstance>,AllocatorInstance>
	{
		typedef detail::LocalPtrImpl<void,FreeDestructor<AllocatorInstance>,AllocatorInstance> baseClass;

	public:
		TempPtr(AllocatorInstance& allocator, void* ptr = NULL) : baseClass(allocator,ptr)
		{}

		TempPtr& operator = (void* p)
		{
			baseClass::assign(p);
			return *this;
		}

		bool reallocate(size_t bytes, size_t align)
		{
			void* p = baseClass::m_allocator.reallocate(baseClass::m_data,bytes,align);
			if (p)
				baseClass::m_data = p;

			return (p ? true : false);
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
