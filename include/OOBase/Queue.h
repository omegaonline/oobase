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
		class PODQueueBase : public AllocImpl<Allocator>
		{
			typedef AllocImpl<Allocator> baseClass;

		public:
			PODQueueBase() : baseClass(), m_data(NULL), m_capacity(0), m_front(0), m_back(0)
			{}

			PODQueueBase(Allocator& allocator) : baseClass(allocator), m_data(NULL), m_capacity(0), m_front(0), m_back(0)
			{}

		protected:
			T*     m_data;
			size_t m_capacity;
			size_t m_front;
			size_t m_back;

		private:
			// Do not allow copy constructors or assignment
			// as memory allocation will occur...
			// and you probably don't want to be copying these around
			PODQueueBase(const PODQueueBase&);
			PODQueueBase& operator = (const PODQueueBase&);
		};

		template <typename Allocator, typename T, bool POD = false>
		class PODQueue : public PODQueueBase<Allocator,T>
		{
			typedef PODQueueBase<Allocator,T> baseClass;

		public:
			PODQueue() : baseClass()
			{}

			PODQueue(Allocator& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				for (;this->m_capacity != 0 && this->m_front != this->m_back;this->m_front = (this->m_front+1) % this->m_capacity)
					this->m_data[this->m_front].~T();
			}

		protected:
			static void copy_to(T* p, const T& v)
			{
				// Placement new with copy constructor
				::new (p) T(v);
			}

			int grow()
			{
				size_t new_size = (this->m_capacity == 0 ? 8 : this->m_capacity * 2);
				T* new_data = static_cast<T*>(baseClass::allocate_i(new_size*sizeof(T),alignof<T>::value));
				if (!new_data)
					return ERROR_OUTOFMEMORY;

				// Now copy the contents of m_data into new_data
				size_t new_back = 0;

#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
					for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
						copy_to(&new_data[new_back++],this->m_data[i]);
				}
				catch (...)
				{
					while (new_back)
						new_data[--new_back].~T();

					baseClass::free_i(new_data);
					throw;
				}

				for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
					this->m_data[i].~T();

#else
				for (size_t i=this->m_front;this->m_capacity != 0 && i != this->m_back; i = (i+1) % this->m_capacity)
				{
					copy_to(&new_data[new_back++],this->m_data[i]);
					this->m_data[i].~T();
				}
#endif

				baseClass::free_i(this->m_data);
				this->m_data = new_data;
				this->m_capacity = new_size;
				this->m_front = 0;
				this->m_back = new_back;

				return 0;
			}
		};

		template <typename Allocator, typename T>
		class PODQueue<Allocator,T,true> : public PODQueueBase<Allocator,T>
		{
			typedef PODQueueBase<Allocator,T> baseClass;

		public:
			PODQueue() : baseClass()
			{}

			PODQueue(Allocator& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				this->m_capacity = this->m_front = this->m_back = 0;
			}

		protected:
			static void copy_to(T* p, const T& v)
			{
				*p = v;
			}

			int grow()
			{
				size_t new_size = (this->m_capacity == 0 ? 8 : this->m_capacity * 2);
				T* new_data = static_cast<T*>(baseClass::reallocate_i(this->m_data,new_size*sizeof(T),alignof<T>::value));
				if (!new_data)
					return ERROR_OUTOFMEMORY;
				
				this->m_capacity = new_size;
				this->m_data = new_data;
				return 0;
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Queue : public detail::PODQueue<Allocator,T,detail::is_pod<T>::value>
	{
		typedef detail::PODQueue<Allocator,T,detail::is_pod<T>::value> baseClass;

	public:
		Queue() : baseClass()
		{}

		Queue(Allocator& allocator) : baseClass(allocator)
		{}
		
		~Queue()
		{
			destroy();
		}

		int push(const T& val)
		{
			if (this->m_capacity == 0 || size() == (this->m_capacity - 1))
			{
				int err = baseClass::grow();
				if (err != 0)
					return err;
			}

			baseClass::copy_to(&this->m_data[this->m_back],val);

			this->m_back = (this->m_back + 1) % this->m_capacity;
			return 0;
		}

		T* front()
		{
			if (this->m_front == this->m_back)
				return NULL;

			return &this->m_data[this->m_front];
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

		template <typename T1>
		bool find(const T1& v) const
		{
			for (size_t pos = this->m_front;this->m_capacity != 0 && pos != this->m_back;pos = (pos+1) % this->m_capacity)
			{
				if (this->m_data[pos] == v)
					return true;
			}
			return false;
		}

		size_t size() const
		{
			if (!this->m_capacity)
				return 0;

			return (this->m_capacity - this->m_front + this->m_back) % this->m_capacity;
		}

		bool empty() const
		{
			return (this->m_front == this->m_back);
		}

	protected:
		void destroy()
		{
			baseClass::clear();
			this->free_i(this->m_data);
		}
	};
}

#endif // OOBASE_QUEUE_H_INCLUDED_
