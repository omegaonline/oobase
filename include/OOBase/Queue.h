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

#ifndef OOBASE_QUEUE_H_INCLUDED_
#define OOBASE_QUEUE_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Allocator, typename T>
		class QueueBase : public Allocating<Allocator>
		{
			typedef Allocating<Allocator> baseClass;

		public:
			QueueBase() : baseClass(), m_data(NULL), m_capacity(0), m_front(0), m_back(0)
			{}

			QueueBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_capacity(0), m_front(0), m_back(0)
			{}

			QueueBase(const QueueBase& rhs) : baseClass(rhs), m_data(NULL), m_capacity(0), m_front(0), m_back(0)
			{}

			~QueueBase()
			{
				clear();
				baseClass::free(m_data);
			}

			void clear()
			{
				for (;m_capacity != 0 && m_front != m_back;m_front = (m_front+1) % m_capacity)
					m_data[m_front].~T();
			}

			void swap(QueueBase& rhs)
			{
				baseClass::swap(rhs);
				OOBase::swap(m_data,rhs.m_data);
				OOBase::swap(m_capacity,rhs.m_capacity);
				OOBase::swap(m_front,rhs.m_front);
				OOBase::swap(m_back,rhs.m_back);
			}

			T* front()
			{
				if (m_front == m_back)
					return NULL;

				return &m_data[m_front];
			}

			const T* front() const
			{
				if (m_front == m_back)
					return NULL;

				return &m_data[m_front];
			}

			template <typename T1>
			bool find(const T1& v) const
			{
				for (size_t pos = m_front;m_capacity != 0 && pos != m_back;pos = (pos+1) % m_capacity)
				{
					if (m_data[pos] == v)
						return true;
				}
				return false;
			}

			size_t size() const
			{
				if (!m_capacity)
					return 0;

				return (m_capacity - m_front + m_back) % m_capacity;
			}

			bool empty() const
			{
				return (m_front == m_back);
			}

		protected:
			T*     m_data;
			size_t m_capacity;
			size_t m_front;
			size_t m_back;
		};

		template <typename Allocator, typename T, bool POD = false>
		class QueuePODBase : public QueueBase<Allocator,T>
		{
			typedef QueueBase<Allocator,T> baseClass;

		public:
			QueuePODBase() : baseClass()
			{}

			QueuePODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			QueuePODBase(const QueuePODBase& rhs) : baseClass(rhs)
			{
				for (size_t pos = rhs.m_front;rhs.m_capacity != 0 && pos != rhs.m_back;pos = (pos+1) % rhs.m_capacity)
				{
					if (!push(rhs.m_data[pos]))
					{
						OOBase_CallCriticalFailure(system_error());
						break;
					}
				}
			}

			bool push(typename call_traits<T>::param_type val)
			{
				if (this->m_capacity == 0 || baseClass::size() == (this->m_capacity - 1))
				{
					if (!grow())
						return false;
				}

				::new (&this->m_data[this->m_back]) T(val);

				this->m_back = (this->m_back + 1) % this->m_capacity;
				return true;
			}

			bool pop(T* value = NULL)
			{
				if (this->m_front == this->m_back)
					return false;

				if (value)
					*value = this->m_data[this->m_front];

				this->m_data[this->m_front].~T();
				this->m_front = (this->m_front + 1) % this->m_capacity;
				return true;
			}

		protected:
			bool grow()
			{
				size_t new_size = (this->m_capacity == 0 ? 8 : this->m_capacity * 2);
				T* new_data = static_cast<T*>(baseClass::allocate(new_size*sizeof(T),alignment_of<T>::value));
				if (!new_data)
					return false;

				// Now copy the contents of m_data into new_data
				size_t new_back = 0;

#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
					for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
						::new (&new_data[new_back++]) T(this->m_data[i]);
				}
				catch (...)
				{
					while (new_back)
						new_data[--new_back].~T();

					baseClass::free(new_data);
					throw;
				}

				for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
					this->m_data[i].~T();

#else
				for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
				{
					::new (&new_data[new_back++]) T(this->m_data[i]);
					this->m_data[i].~T();
				}
#endif

				baseClass::free(this->m_data);
				this->m_data = new_data;
				this->m_capacity = new_size;
				this->m_front = 0;
				this->m_back = new_back;

				return true;
			}
		};

		template <typename Allocator, typename T>
		class QueuePODBase<Allocator,T,true> : public QueueBase<Allocator,T>
		{
			typedef QueueBase<Allocator,T> baseClass;

		public:
			QueuePODBase() : baseClass()
			{}

			QueuePODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			QueuePODBase(const QueuePODBase& rhs) : baseClass(rhs)
			{
				this->m_data = static_cast<T*>(baseClass::allocate(rhs.m_data,rhs.m_capacity*sizeof(T),alignment_of<T>::value));
				if (!this->m_data)
					OOBase_CallCriticalFailure(system_error());
				else
				{
					memcpy(this->m_data,rhs.m_data,rhs.m_capacity*sizeof(T));
					this->m_capacity = rhs.m_capacity;
					this->m_front = rhs.m_front;
					this->m_back = rhs.m_back;
				}
			}

			bool push(typename call_traits<T>::param_type val)
			{
				if (this->m_capacity == 0 || baseClass::size() == (this->m_capacity - 1))
				{
					if (!grow())
						return false;
				}

				this->m_data[this->m_back] = val;

				this->m_back = (this->m_back + 1) % this->m_capacity;
				return true;
			}

			bool pop(T* value = NULL)
			{
				if (this->m_front == this->m_back)
					return false;

				if (value)
					*value = this->m_data[this->m_front];

				this->m_front = (this->m_front + 1) % this->m_capacity;
				return true;
			}

		protected:
			bool grow()
			{
				size_t new_size = (this->m_capacity == 0 ? 8 : this->m_capacity * 2);
				T* new_data = static_cast<T*>(baseClass::reallocate(this->m_data,new_size*sizeof(T),alignment_of<T>::value));
				if (!new_data)
					return false;
				
				this->m_capacity = new_size;
				this->m_data = new_data;
				return true;
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Queue : public detail::QueuePODBase<Allocator,T,detail::is_pod<T>::value>
	{
		typedef detail::QueuePODBase<Allocator,T,detail::is_pod<T>::value> baseClass;

	public:
		Queue() : baseClass()
		{}

		Queue(AllocatorInstance& allocator) : baseClass(allocator)
		{}
		
		Queue(const Queue& rhs) : baseClass(rhs)
		{}

		Queue& operator = (const Queue& rhs)
		{
			Queue(rhs).swap(*this);
			return *this;
		}
	};

	template <typename T, typename A>
	void swap(Queue<T,A>& lhs, Queue<T,A>& rhs)
	{
		lhs.swap(rhs);
	}
}

#endif // OOBASE_QUEUE_H_INCLUDED_
