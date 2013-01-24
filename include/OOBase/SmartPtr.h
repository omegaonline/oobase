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

#include "Memory.h"

namespace OOBase
{
	template <typename T>
	class DeleteDestructor
	{
	public:
		static void destroy(T* ptr)
		{
			delete ptr;
		}
	};

	template <typename T>
	class ArrayDeleteDestructor
	{
	public:
		static void destroy(T* ptr)
		{
			delete [] ptr;
		}
	};

	template <typename A>
	class FreeDestructor
	{
	public:
		static void destroy(void* ptr)
		{
			A::free(ptr);
		}
	};

	namespace detail
	{
		template <typename Allocator>
		class SmartPtrAlloc
		{
		protected:
			struct SmartPtrNodeBase
			{
				size_t m_refcount; ///< The reference count.
			};

			template <typename T>
			static void alloc_node(T*& n)
			{
				n = static_cast<T*>(Allocator::allocate(sizeof(T),alignof<T>::value));
				if (!n)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				n->m_refcount = 1;
			}

			template <typename T>
			static void free_node(T*& n)
			{
				Allocator::free(n);
				n = NULL;
			}

			template <typename T>
			static T* addref_node(T* n)
			{
				if (n)
					++n->m_refcount;
				return n;
			}
		};

		template <>
		class SmartPtrAlloc<AllocatorInstance>
		{
		protected:
			struct SmartPtrNodeBase
			{
				size_t             m_refcount; ///< The reference count.
				AllocatorInstance* m_node_alloc;
			};

			AllocatorInstance& m_allocator;

			SmartPtrAlloc(AllocatorInstance& a) : m_allocator(a)
			{}

			SmartPtrAlloc(const SmartPtrAlloc& rhs) : m_allocator(rhs.m_allocator)
			{}

			SmartPtrAlloc& operator = (const SmartPtrAlloc& rhs)
			{
				if (this != &rhs)
					 m_allocator = rhs.m_allocator;
				return *this;
			}

			template <typename T>
			void alloc_node(T*& n)
			{
				n = m_allocator.allocate<T>();
				if (!n)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				n->m_refcount = 1;
				n->m_node_alloc = &m_allocator;
			}

			template <typename T>
			static void free_node(T*& n)
			{
				n->m_node_alloc->free(n);
				n = NULL;
			}

			template <typename T>
			static T* addref_node(T* n)
			{
				if (n)
					++n->m_refcount;
				return n;
			}

		public:
			AllocatorInstance& get_allocator()
			{
				return m_allocator;
			}
		};

		template <typename T, typename Destructor, typename NodeAllocator>
		class SmartPtrBase : public SmartPtrAlloc<NodeAllocator>
		{
			typedef SmartPtrAlloc<NodeAllocator> baseClass;

		protected:
			struct SmartPtrNode : public baseClass::SmartPtrNodeBase
			{
				T*     m_data;
			};

			SmartPtrNode* m_node;

			SmartPtrBase(T* ptr) : m_node(new_node(ptr))
			{}

			SmartPtrBase(T* ptr, NodeAllocator& node_allocator) :
				baseClass(node_allocator),
				m_node(new_node(ptr))
			{}

			SmartPtrBase(const SmartPtrBase& rhs) :
				baseClass(rhs),
				m_node(baseClass::addref_node(rhs.m_node))
			{}

			SmartPtrBase& operator = (const SmartPtrBase& rhs)
			{
				if (this != &rhs)
				{
					release_node(m_node);
					m_node = baseClass::addref_node(rhs.m_node);
				}
				return *this;
			}

			~SmartPtrBase()
			{
				release_node(m_node);
			}

			static void release_node(SmartPtrNode*& n)
			{
				if (n && --n->m_refcount == 0)
				{
					Destructor::destroy(n->m_data);
					baseClass::free_node(n);
				}
			}

			void update_node(SmartPtrNode*& n, T* ptr)
			{
				if (n)
					release_node(n);

				n = new_node(ptr);
			}

		private:
			SmartPtrNode* new_node(T* ptr)
			{
				SmartPtrNode* n = NULL;
				if (ptr)
				{
					baseClass::alloc_node(n);
					n->m_data = ptr;
				}
				return n;
			}
		};

		template <typename T, typename NodeAllocator>
		class SmartPtrBase<T,FreeDestructor<AllocatorInstance>,NodeAllocator> : public SmartPtrAlloc<NodeAllocator>
		{
			typedef SmartPtrAlloc<NodeAllocator> baseClass;

		protected:
			struct SmartPtrNode : public baseClass::SmartPtrNodeBase
			{
				T*                 m_data;
				AllocatorInstance* m_alloc;
			};

			SmartPtrNode* m_node;

			SmartPtrBase(AllocatorInstance& allocator, T* ptr) :
				baseClass(),
				m_node(new_node(allocator,ptr))
			{}

			SmartPtrBase(AllocatorInstance& allocator, T* ptr, NodeAllocator& node_allocator) :
				baseClass(node_allocator),
				m_node(new_node(allocator,ptr))
			{}

			SmartPtrBase(const SmartPtrBase& rhs) :
				baseClass(rhs),
				m_node(baseClass::addref_node(rhs.m_node))
			{}

			SmartPtrBase& operator = (const SmartPtrBase& rhs)
			{
				if (this != &rhs)
				{
					release_node(m_node);
					m_node = baseClass::addref_node(rhs.m_node);
				}
				return *this;
			}

			~SmartPtrBase()
			{
				release_node(m_node);
			}

			static void release_node(SmartPtrNode*& n)
			{
				if (n && --n->m_refcount == 0)
				{
					n->m_alloc->free(n->m_data);
					baseClass::free_node(n);
					n = NULL;
				}
			}

			void update_node(SmartPtrNode*& n, T* ptr)
			{
				if (n && n->m_data != ptr)
				{
					AllocatorInstance* a = n->m_alloc;
					release_node(n);
					n = new_node(*a,ptr);
				}
			}

		private:
			SmartPtrNode* new_node(AllocatorInstance& allocator, T* ptr)
			{
				SmartPtrNode* n = NULL;
				baseClass::alloc_node(n);
				n->m_data = ptr;
				n->m_alloc = &allocator;
				return n;
			}

		public:
			bool reallocate(size_t count)
			{
				T* p = NULL;
				if (m_node)
				{
					p = static_cast<T*>(m_node->m_alloc->reallocate(m_node->m_data,sizeof(T)*count,alignof<T>::value));
					if (p)
						update_node(m_node,p);
				}
				return (p ? true : false);
			}
		};

		template <typename T, typename Destructor, typename Allocator>
		class SmartPtrImpl : public SmartPtrBase<T,Destructor,Allocator>
		{
			typedef SmartPtrBase<T,Destructor,Allocator> baseClass;

		public:
			SmartPtrImpl(T* ptr = NULL) : baseClass(ptr)
			{}

			SmartPtrImpl(T* ptr, Allocator& node_allocator) : baseClass(ptr,node_allocator)
			{}

			SmartPtrImpl(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
			{}

			SmartPtrImpl(AllocatorInstance& allocator, T* ptr, Allocator& node_allocator) : baseClass(allocator,ptr,node_allocator)
			{}

			SmartPtrImpl& operator = (T* ptr)
			{
				baseClass::update_node(this->m_node,ptr);
				return *this;
			}

			T* detach()
			{
				T* v = NULL;
				if (this->m_node)
				{
					v = this->m_node->m_data;
					this->m_node->m_data = NULL;
				}
				return v;
			}

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
				return (this->m_node ? this->m_node->m_data : NULL);
			}

			const T* value() const
			{
				return (this->m_node ? this->m_node->m_data : NULL);
			}
		};
	}

	template <typename T, typename Destructor = DeleteDestructor<T>, typename Allocator = CrtAllocator >
	class SmartPtr : public detail::SmartPtrImpl<T,Destructor,Allocator>
	{
		typedef detail::SmartPtrImpl<T,Destructor,Allocator> baseClass;

	public:
		SmartPtr(T* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(T* ptr, Allocator& node_allocator) : baseClass(ptr,node_allocator)
		{}

		SmartPtr(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
		{}

		SmartPtr(AllocatorInstance& allocator, T* ptr, Allocator& node_allocator) : baseClass(allocator,ptr,node_allocator)
		{}

		SmartPtr(const SmartPtr& rhs) : baseClass(rhs)
		{}

		SmartPtr& operator = (T* ptr)
		{
			baseClass::operator=(ptr);
			return *this;
		}

		SmartPtr& operator = (const SmartPtr& rhs)
		{
			if (this != &rhs)
				baseClass::operator=(rhs);

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

	template <typename Destructor, typename Allocator>
	class SmartPtr<void,Destructor,Allocator> : public detail::SmartPtrImpl<void,Destructor,Allocator>
	{
		typedef detail::SmartPtrImpl<void,Destructor,Allocator> baseClass;

	public:
		SmartPtr(void* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(void* ptr, Allocator& node_allocator) : baseClass(ptr,node_allocator)
		{}

		SmartPtr(AllocatorInstance& allocator, void* ptr = NULL) : baseClass(allocator,ptr)
		{}

		SmartPtr(AllocatorInstance& allocator, void* ptr, Allocator& node_allocator) : baseClass(allocator,ptr,node_allocator)
		{}

		SmartPtr(const SmartPtr& rhs) : baseClass(rhs)
		{}

		SmartPtr& operator = (void* ptr)
		{
			baseClass::operator=(ptr);
			return *this;
		}

		SmartPtr& operator = (const SmartPtr& rhs)
		{
			if (this != &rhs)
				baseClass::operator=(rhs);

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

	template <typename Destructor>
	class SmartPtr<void,Destructor,CrtAllocator> : public detail::SmartPtrImpl<void,Destructor,CrtAllocator>
	{
		typedef detail::SmartPtrImpl<void,Destructor,CrtAllocator> baseClass;

	public:
		SmartPtr(void* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(AllocatorInstance& allocator, void* ptr = NULL) : baseClass(allocator,ptr)
		{}

		SmartPtr(const SmartPtr& rhs) : baseClass(rhs)
		{}

		SmartPtr& operator = (void* ptr)
		{
			baseClass::operator=(ptr);
			return *this;
		}

		SmartPtr& operator = (const SmartPtr& rhs)
		{
			if (this != &rhs)
				baseClass::operator=(rhs);

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

	template <typename T>
	class TempPtr : public SmartPtr<T,FreeDestructor<AllocatorInstance>,AllocatorInstance>
	{
		typedef SmartPtr<T,FreeDestructor<AllocatorInstance>,AllocatorInstance> baseClass;

	public:
		TempPtr(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr,allocator)
		{}

		TempPtr(const TempPtr& rhs) : baseClass(rhs)
		{}

		TempPtr& operator = (T* ptr)
		{
			baseClass::operator=(ptr);
			return *this;
		}

		TempPtr& operator = (const TempPtr& rhs)
		{
			if (this != &rhs)
				baseClass::operator=(rhs);

			return *this;
		}
	};

	namespace detail
	{
		template <typename T, typename Destructor>
		class LocalPtrImpl
		{
		public:
			LocalPtrImpl(T* p = NULL) : m_data(p)
			{}

			~LocalPtrImpl()
			{
				if (m_data)
					Destructor::destroy(m_data);
			}

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
			LocalPtrImpl(const LocalPtrImpl&);
			LocalPtrImpl& operator = (const LocalPtrImpl&);
		};

		template <typename T>
		class LocalPtrImpl<T,FreeDestructor<AllocatorInstance> >
		{
		public:
			LocalPtrImpl(AllocatorInstance& allocator, T* p = NULL) : m_data(p), m_allocator(allocator)
			{}

			~LocalPtrImpl()
			{
				if (m_data)
					m_allocator.free(m_data);
			}

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
			LocalPtrImpl(const LocalPtrImpl&);
			LocalPtrImpl& operator = (const LocalPtrImpl&);

			AllocatorInstance& m_allocator;
		};
	}

	template <typename T, typename Destructor>
	class LocalPtr : public detail::LocalPtrImpl<T,Destructor>
	{
		typedef detail::LocalPtrImpl<T,Destructor> baseClass;

	public:
		LocalPtr(T* p = NULL) : baseClass(p)
		{}

		LocalPtr(AllocatorInstance& allocator, T* p = NULL) : baseClass(allocator,p)
		{}

		T* operator ->()
		{
			return baseClass::m_data;
		}

		const T* operator ->() const
		{
			return baseClass::m_data;
		}
	};

	template <typename Destructor>
	class LocalPtr<void,Destructor> : public detail::LocalPtrImpl<void,Destructor>
	{
		typedef detail::LocalPtrImpl<void,Destructor> baseClass;

	public:
		LocalPtr(void* p = NULL) : baseClass(p)
		{}

		LocalPtr(AllocatorInstance& allocator, void* p = NULL) : baseClass(allocator,p)
		{}

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
