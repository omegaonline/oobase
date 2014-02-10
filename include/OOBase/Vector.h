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

#ifndef OOBASE_VECTOR_H_INCLUDED_
#define OOBASE_VECTOR_H_INCLUDED_

#include "Memory.h"
#include "Iterator.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Allocator, typename T>
		class VectorBase : public Allocating<Allocator>
		{
			typedef Allocating<Allocator> baseClass;

		public:
			typedef typename add_const<T*>::type const_pointer;

			VectorBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			explicit VectorBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

			VectorBase(const VectorBase& rhs) : baseClass(rhs), m_data(NULL), m_size(0), m_capacity(0)
			{}

			~VectorBase()
			{
				baseClass::free(m_data);
			}

			VectorBase& operator = (const VectorBase& rhs)
			{
				VectorBase(rhs).swap(*this);
				return *this;
			}

			void swap(VectorBase& rhs)
			{
				Allocating<Allocator>::swap(rhs);

				OOBase::swap(m_data,rhs.m_data);
				OOBase::swap(m_size,rhs.m_size);
				OOBase::swap(m_capacity,rhs.m_size);
			}

			bool empty() const
			{
				return (m_size == 0);
			}

			size_t size() const
			{
				return m_size;
			}

		protected:
			T*     m_data;
			size_t m_size;
			size_t m_capacity;

			T* at(size_t pos)
			{
				return (pos >= m_size ? NULL : &m_data[pos]);
			}

			const_pointer at(size_t pos) const
			{
				return (pos >= m_size ? NULL : &m_data[pos]);
			}

			void next(size_t& pos) const
			{
				if (pos < m_size - 1)
					++pos;
				else
					pos = size_t(-1);
			}

			void prev(size_t& pos) const
			{
				if (pos >= m_size || pos == 0)
					pos = m_size-1;
				else
					--pos;
			}
		};

		template <typename Allocator, typename T, bool POD = false>
		class VectorPODBase : public VectorBase<Allocator,T>
		{
			typedef VectorBase<Allocator,T> baseClass;

		public:
			VectorPODBase() : baseClass()
			{}

			explicit VectorPODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			~VectorPODBase()
			{
				clear();
			}

			void clear()
			{
				while (this->m_size > 0)
					this->m_data[--this->m_size].~T();
			}

		protected:
#if defined(OOBASE_HAVE_EXCEPTIONS)
			int assign(size_t n, const T& value = T())
			{
				T* new_data = NULL;
				size_t capacity = 0;
				if (n)
				{
					capacity = (n/2 + 1) * 2;
					new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					size_t i = 0;
					try
					{
						for (;i<this->m_size;++i)
							::new (&new_data[i]) T(value);
					}
					catch (...)
					{
						while (i-- > 0)
							new_data[i].~T();

						baseClass::free(new_data);
						throw;
					}
				}

				for (size_t i=0;i<this->m_size;++i)
					this->m_data[i].~T();

				baseClass::free(this->m_data);
				this->m_data = new_data;
				this->m_size = n;
				this->m_capacity = capacity;
				return 0;
			}

			int reserve(size_t n)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					size_t i = 0;
					try
					{
						for (;i<this->m_size;++i)
							::new (&new_data[i]) T(this->m_data[i]);
					}
					catch (...)
					{
						while (i-- > 0)
							new_data[i].~T();

						baseClass::free(new_data);
						throw;
					}

					for (i=0;i<this->m_size;++i)
						this->m_data[i].~T();

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}

			int insert_at(size_t pos, const T& value)
			{
				if (pos > this->m_size)
					pos = this->m_size;

				if (this->m_size >= this->m_capacity)
				{
					// Copy buffer, inserting in the gap...
					size_t capacity = this->m_capacity;
					if (this->m_size+1 > this->m_capacity)
						capacity = (this->m_capacity == 0 ? 2 : this->m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					size_t i = 0;
					try
					{
						for (;i<this->m_size;++i)
							::new (&new_data[i + (i<pos ? 0 : 1)]) T(this->m_data[i]);

						::new (&new_data[pos]) T(value);
					}
					catch (...)
					{
						while (i-- > 0)
						{
							if (i != pos)
								new_data[i].~T();
						}
						baseClass::free(new_data);
						throw;
					}

					for (i=0;i<this->m_size;++i)
						this->m_data[i].~T();

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					::new (&this->m_data[this->m_size]) T(value);

					for (size_t i = this->m_size; i > pos; --i)
						OOBase::swap(this->m_data[i],this->m_data[i-1]);
				}

				++this->m_size;
				return 0;
			}
#else
			int assign(size_t n, const T& value = T())
			{
				T* new_data = NULL;
				size_t capacity = 0;
				if (n)
				{
					capacity = (n/2 + 1) * 2;
					new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					for (size_t i=0;i<this->m_size;++i)
						::new (&new_data[i]) T(value);
				}

				for (size_t i=0;i<this->m_size;++i)
					this->m_data[i].~T();

				baseClass::free(this->m_data);
				this->m_data = new_data;
				this->m_size = n;
				this->m_capacity = capacity;
				return 0;
			}

			int reserve(size_t n)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					for (size_t i = 0;i<this->m_size;++i)
					{
						::new (&new_data[i]) T(this->m_data[i]);
						this->m_data[i].~T();
					}

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}

			int insert_at(size_t pos, const T& value)
			{
				if (pos > this->m_size)
					pos = this->m_size;

				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 2 : this->m_capacity*2);
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					for (size_t i = 0;i<this->m_size;++i)
					{
						::new (&new_data[i + (i<pos ? 0 : 1)]) T(this->m_data[i]);
						this->m_data[i].~T();
					}

					::new (&new_data[pos]) T(value);

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					::new (&this->m_data[this->m_size]) T(value);

					for (size_t i = this->m_size; i > pos; --i)
						OOBase::swap(this->m_data[i],this->m_data[i-1]);
				}

				++this->m_size;
				return 0;
			}
#endif
			size_t remove_at(size_t pos)
			{
				if (this->m_data && pos < this->m_size)
				{
					for(--this->m_size;pos < this->m_size;++pos)
						OOBase::swap(this->m_data[pos],this->m_data[pos+1]);

					this->m_data[this->m_size].~T();
				}
				return pos < this->m_size ? pos : this->m_size;
			}
		};

		template <typename Allocator, typename T>
		class VectorPODBase<Allocator,T,true> : public VectorBase<Allocator,T>
		{
			typedef VectorBase<Allocator,T> baseClass;

		public:
			VectorPODBase() : baseClass()
			{}

			explicit VectorPODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

		protected:
			int assign(size_t n, const T& value = T())
			{
				T* new_data = NULL;
				size_t capacity = 0;
				if (n)
				{
					capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					for (size_t i=0; i<this->m_size; ++i)
						new_data[i] = value;
				}
				baseClass::free(this->m_data);
				this->m_data = new_data;
				this->m_size = n;
				this->m_capacity = capacity;
				return 0;
			}

			int reserve(size_t n)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					memcpy(new_data,this->m_data,this->m_size * sizeof(T));

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}

			int insert_at(size_t pos, const T& value)
			{
				if (pos > this->m_size)
					pos = this->m_size;

				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 2 : this->m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					if (pos > 0)
						memcpy(new_data,this->m_data,pos * sizeof(T));

					if (pos < this->m_size)
						memcpy(&new_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));

					new_data[pos] = value;

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					if (pos < this->m_size)
						memmove(&this->m_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));

					this->m_data[pos] = value;
				}
				++this->m_size;
				return 0;
			}

			size_t remove_at(size_t pos)
			{
				if (this->m_data && pos < this->m_size)
				{
					--this->m_size;
					if (pos < this->m_size)
						memmove(&this->m_data[pos],&this->m_data[pos+1],(this->m_size - pos) * sizeof(T));
				}
				return pos < this->m_size ? pos : this->m_size;
			}
		};

		template <typename T, typename Allocator>
		class VectorImpl : public VectorPODBase<Allocator,T,is_pod<T>::value>
		{
			typedef VectorPODBase<Allocator,T,is_pod<T>::value> baseClass;

		public:
			VectorImpl() : baseClass()
			{}

			explicit VectorImpl(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			VectorImpl(const VectorImpl& rhs) : baseClass(rhs)
			{
				baseClass::clear();
				for (size_t i=0;i<rhs.m_size;++i)
				{
					int err = push_back(rhs.m_data[i]);
					if (err)
						OOBase_CallCriticalFailure(err);
				}
			}

			VectorImpl& operator = (const VectorImpl& rhs)
			{
				VectorImpl(rhs).swap(*this);
				return *this;
			}

		protected:
			typedef typename add_const<T&>::type const_reference;
			typedef typename add_const<T*>::type const_pointer;

			template <typename It>
			int assign(It first, It last)
			{
				baseClass::clear();
				for (It i = first; i != last; ++i)
				{
					int err = push_back(*i);
					if (err)
						return err;
				}
				return 0;
			}

			T* data()
			{
				return this->m_data;
			}

			const_pointer data() const
			{
				return this->m_data;
			}

			T& front()
			{
				assert(baseClass::at(0));
				return *baseClass::at(0);
			}

			const_reference front() const
			{
				assert(baseClass::at(0));
				return *baseClass::at(0);
			}

			T& back()
			{
				assert(baseClass::at(this->m_size-1));
				return *baseClass::at(this->m_size-1);
			}

			const_reference back() const
			{
				assert(baseClass::at(this->m_size-1));
				return *baseClass::at(this->m_size-1);
			}

			int push_back(const T& value)
			{
				return baseClass::insert_at(this->m_size,value);
			}

			bool pop_back()
			{
				baseClass::remove_at(this->m_size - 1);
				return !baseClass::empty();
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Vector : public detail::VectorImpl<T,Allocator>
	{
		typedef detail::VectorImpl<T,Allocator> baseClass;

	public:
		typedef T value_type;
		typedef Allocator allocator_type;
		typedef T& reference;
		typedef typename add_const<reference>::type const_reference;
		typedef T* pointer;
		typedef typename add_const<pointer>::type const_pointer;

		typedef detail::IteratorImpl<Vector,value_type,size_t> iterator;
		friend class detail::IteratorImpl<Vector,value_type,size_t>;

		typedef detail::IteratorImpl<const Vector,const value_type,size_t> const_iterator;
		friend class detail::IteratorImpl<const Vector,const value_type,size_t>;

		Vector() : baseClass()
		{}

		explicit Vector(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		template <typename It>
		int assign(It first, It last)
		{
			return baseClass::assign(first,last);
		}

		int assign(size_t n, const T& value = T())
		{
			return baseClass::assign(n,value);
		}

		int reserve(size_t n)
		{
			return baseClass::reserve(n);
		}

		T* data()
		{
			return baseClass::data();
		}

		const_pointer data() const
		{
			return baseClass::data();
		}

		T& front()
		{
			return baseClass::front();
		}

		const_reference front() const
		{
			return baseClass::front();
		}

		T& back()
		{
			return baseClass::back();
		}

		const_reference back() const
		{
			return baseClass::back();
		}

		int resize(size_t new_size, const T& value)
		{
			int err = reserve(new_size);
			if (err)
				return err;

			while (this->m_size < new_size && !err)
				err = baseClass::push_back(value);
			while (this->m_size > new_size && !err)
				err = baseClass::pop_back();
			return err;
		}

		int resize(size_t new_size)
		{
			return resize(new_size,T());
		}

		int push_back(const T& value)
		{
			return baseClass::push_back(value);
		}

		bool pop_back()
		{
			return baseClass::pop_back();
		}

		int insert(const T& value, const iterator& before)
		{
			assert(before.check(this));
			return baseClass::insert_at(before.deref(),value);
		}

		iterator insert(const T& value, const iterator& before, int& err)
		{
			err = baseClass::insert_at(before.deref(),value);
			return (!err ? before : end());
		}

		iterator erase(iterator iter)
		{
			assert(iter.check(this));
			return iterator(this,baseClass::remove_at(iter.deref()));
		}

		size_t erase(const T& value)
		{
			size_t ret = 0;
			for (size_t pos = 0;pos < this->m_size;)
			{
				if (this->m_data[pos] == value)
				{
					baseClass::remove_at(pos);
					++ret;
				}
				else
					++pos;
			}
			return ret;
		}

		pointer at(size_t pos)
		{
			return baseClass::at(pos);
		}

		const_pointer at(size_t pos) const
		{
			return baseClass::at(pos);
		}

		reference operator [](size_t pos)
		{
			assert(at(pos));
			return *at(pos);
		}

		const_reference operator [](size_t pos) const
		{
			assert(at(pos));
			return *at(pos);
		}

		iterator begin()
		{
			return iterator(this,baseClass::empty() ? size_t(-1) : 0);
		}

		const_iterator cbegin() const
		{
			return const_iterator(this,baseClass::empty() ? size_t(-1) : 0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator end()
		{
			return iterator(this,size_t(-1));
		}

		const_iterator cend() const
		{
			return const_iterator(this,size_t(-1));
		}

		const_iterator end() const
		{
			return cend();
		}
	};
}

#endif // OOBASE_VECTOR_H_INCLUDED_
