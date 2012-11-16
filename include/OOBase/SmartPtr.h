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
#include "Memory.h"

namespace OOBase
{
	template <typename T>
	class DeleteDestructor
	{
	public:
		typedef CrtAllocator Allocator;

		static void destroy(T* ptr)
		{
			delete ptr;
		}
	};

	template <typename T>
	class ArrayDeleteDestructor
	{
	public:
		typedef CrtAllocator Allocator;

		static void destroy(T* ptr)
		{
			delete [] ptr;
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

	namespace detail
	{
		template <typename T, typename Destructor>
		class SmartPtrBase
		{
		protected:
			struct SmartPtrNode
			{
				T*    m_data;
				size_t m_refcount; ///< The reference count.
			};

			SmartPtrNode* m_node;

			SmartPtrBase(T* ptr) : m_node(new_node(ptr))
			{}

			SmartPtrBase(SmartPtrNode* node) : m_node(node)
			{}

			static void release_node(SmartPtrNode*& n)
			{
				if (n && Atomic<size_t>::Decrement(n->m_refcount) == 0)
				{
					Destructor::destroy(n->m_data);
					Destructor::Allocator::free(n);
					n = NULL;
				}
			}

			static void update_node(SmartPtrNode*& n, T* ptr)
			{
				if (n)
					release_node(n);

				n = new_node(ptr);
			}

		private:
			static SmartPtrNode* new_node(T* ptr)
			{
				SmartPtrNode* n = NULL;
				if (ptr)
				{
					n = static_cast<SmartPtrNode*>(Destructor::Allocator::allocate(sizeof(SmartPtrNode),alignof<SmartPtrNode>::value));

					if (!n)
						OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

					n->m_refcount = 1;
					n->m_data = ptr;
				}
				return n;
			}
		};

		template <typename T>
		class SmartPtrBase<T,FreeDestructor<AllocatorInstance> >
		{
		protected:
			struct SmartPtrNode
			{
				T*                 m_data;
				size_t             m_refcount; ///< The reference count.
				AllocatorInstance* m_alloc;
			};

			SmartPtrNode* m_node;

			SmartPtrBase(AllocatorInstance& allocator, T* ptr) : m_node(new_node(allocator,ptr))
			{}

			SmartPtrBase(SmartPtrNode* node) : m_node(node)
			{}

			static void release_node(SmartPtrNode*& n)
			{
				if (n && Atomic<size_t>::Decrement(n->m_refcount) == 0)
				{
					n->m_alloc->free(n->m_data);
					n->m_alloc->free(n);
					n = NULL;
				}
			}

			static void update_node(SmartPtrNode*& n, T* ptr)
			{
				if (n->m_data != ptr)
				{
					AllocatorInstance* a = n->m_alloc;
					release_node(n);
					n = new_node(*a,ptr);
				}
			}

		private:
			static SmartPtrNode* new_node(AllocatorInstance& allocator, T* ptr)
			{
				SmartPtrNode* n = static_cast<SmartPtrNode*>(allocator.allocate(sizeof(SmartPtrNode),alignof<SmartPtrNode>::value));
				if (!n)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

				n->m_refcount = 1;
				n->m_data = ptr;
				n->m_alloc = &allocator;
				return n;
			}

		public:
			AllocatorInstance& get_allocator()
			{
				return *m_node->m_alloc;
			}

			bool reallocate(size_t count)
			{
				T* p = static_cast<T*>(get_allocator().reallocate(m_node->m_data,sizeof(T)*count,alignof<T>::value));
				if (p)
					update_node(m_node,p);
				return (p ? true : false);
			}
		};

		template <typename T, typename Destructor>
		class SmartPtrImpl : public SmartPtrBase<T,Destructor>
		{
			typedef SmartPtrBase<T,Destructor> baseClass;

		public:
			SmartPtrImpl(T* ptr = NULL) : baseClass(ptr)
			{}

			SmartPtrImpl(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
			{}

			SmartPtrImpl(const SmartPtrImpl& rhs) : baseClass(addref_node(rhs.m_node))
			{}

			SmartPtrImpl& operator = (T* ptr)
			{
				baseClass::update_node(this->m_node,ptr);
				return *this;
			}

			SmartPtrImpl& operator = (const SmartPtrImpl& rhs)
			{
				if (this != &rhs)
				{
					baseClass::release_node(this->m_node);
					this->m_node = addref_node(rhs.m_node);
				}
				return *this;
			}

			~SmartPtrImpl()
			{
				baseClass::release_node(this->m_node);
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

			static typename baseClass::SmartPtrNode* addref_node(typename baseClass::SmartPtrNode* n)
			{
				if (n)
					Atomic<size_t>::Increment(n->m_refcount);
				return n;
			}
		};
	}

	template <typename T, typename Destructor = DeleteDestructor<T> >
	class SmartPtr : public detail::SmartPtrImpl<T,Destructor>
	{
		typedef detail::SmartPtrImpl<T,Destructor> baseClass;

	public:
		SmartPtr(T* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(typename Destructor::Allocator& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
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

	template <typename Destructor>
	class SmartPtr<void,Destructor> : public detail::SmartPtrImpl<void,Destructor>
	{
		typedef detail::SmartPtrImpl<void,Destructor> baseClass;

	public:
		SmartPtr(void* ptr = NULL) : baseClass(ptr)
		{}

		SmartPtr(typename Destructor::Allocator& allocator, void* ptr = NULL) : baseClass(allocator,ptr)
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
	class TempPtr : public SmartPtr<T,FreeDestructor<AllocatorInstance> >
	{
		typedef SmartPtr<T,FreeDestructor<AllocatorInstance> > baseClass;

	public:
		TempPtr(AllocatorInstance& allocator, T* ptr = NULL) : baseClass(allocator,ptr)
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
}

#endif // OOBASE_SMARTPTR_H_INCLUDED_
