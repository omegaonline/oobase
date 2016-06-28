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
#include "Random.h"

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

			VectorBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

			~VectorBase()
			{
				baseClass::free(m_data);
			}

			void swap(VectorBase& rhs)
			{
				baseClass::swap(rhs);
				OOBase::swap(m_data,rhs.m_data);
				OOBase::swap(m_size,rhs.m_size);
				OOBase::swap(m_capacity,rhs.m_capacity);
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

			VectorBase(const VectorBase& rhs) : baseClass(rhs), m_data(NULL), m_size(0), m_capacity(0)
			{}

			T* at(size_t pos)
			{
				return (pos >= m_size ? NULL : &m_data[pos]);
			}

			const_pointer at(size_t pos) const
			{
				return (pos >= m_size ? NULL : &m_data[pos]);
			}

			void iterator_move(size_t& pos, ptrdiff_t n) const
			{
				if (pos > m_size)
					pos = m_size;

				assert(pos + n >= 0);
				pos += n;
				if (pos >= m_size)
					pos = size_t(-1);
			}

			ptrdiff_t iterator_diff(size_t pos1, size_t pos2) const
			{
				if (pos1 > m_size)
					pos1 = m_size;

				if (pos2 > m_size)
					pos2 = m_size;

				return (pos1 - pos2);
			}
		};

		template <typename Allocator, typename T, bool POD = false>
		class VectorPODBase : public VectorBase<Allocator,T>
		{
			typedef VectorBase<Allocator,T> baseClass;

		public:
			VectorPODBase() : baseClass()
			{}

			VectorPODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			VectorPODBase(const VectorPODBase& rhs) : baseClass(rhs)
			{
				if (rhs.m_size)
				{
					if (!reserve(rhs.m_size))
						OOBase_CallCriticalFailure(system_error());
					else
					{
						for (size_t i=0;i<rhs.m_size;++i)
						{
							if (!insert_at(i,rhs.m_data[i]))
								break;
						}
					}
				}
			}

			VectorPODBase& operator = (const VectorPODBase& rhs)
			{
				this->clear();
				if (rhs.m_size)
				{
					if (!reserve(rhs.m_size))
						OOBase_CallCriticalFailure(system_error());
					else
					{
						for (size_t i=0;i<rhs.m_size;++i)
						{
							if (!insert_at(i,rhs.m_data[i]))
								break;
						}
					}
				}
				return *this;
			}

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
			bool reserve(size_t n)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return false;

					size_t i = 0;
#if defined(OOBASE_HAVE_EXCEPTIONS)
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

					for (;i<this->m_size;++i)
						this->m_data[i].~T();
#else
					for (;i<this->m_size;++i)
					{
						::new (&new_data[i]) T(this->m_data[i]);
						this->m_data[i].~T();
					}
#endif
					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return true;
			}

			bool assign(size_t n)
			{
				return assign(n,T());
			}

			bool assign(size_t n, typename call_traits<T>::param_type value)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return false;

					clear();
					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;

					for (size_t i=0; i<n; ++i)
						::new (&this->m_data[this->m_size++]) T(value);
				}
				else
				{
					while (this->m_size > n)
						this->m_data[this->m_size--].~T();

					for (size_t i=0;i<this->m_size;++i)
						this->m_data[i] = value;
				}
				return true;
			}

			bool insert_at(size_t& pos, typename call_traits<T>::param_type value)
			{
				if (pos > this->m_size)
					pos = this->m_size;

				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = this->m_capacity ? this->m_capacity * 2 : 8;
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return false;

					size_t i = 0;
#if defined(OOBASE_HAVE_EXCEPTIONS)
					try
					{
						for (;i<pos;++i)
							::new (&new_data[i]) T(this->m_data[i]);

						for (;i<this->m_size;++i)
							::new (&new_data[i+1]) T(this->m_data[i]);

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
#else
					for (;i<pos;++i)
					{
						::new (&new_data[i]) T(this->m_data[i]);
						this->m_data[i].~T();
					}

					for (;i<this->m_size;++i)
					{
						::new (&new_data[i+1]) T(this->m_data[i]);
						this->m_data[i].~T();
					}
					::new (&new_data[pos]) T(value);
#endif
					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					// Insert at end and ripple down to pos
					::new (&this->m_data[this->m_size]) T(value);

					for (size_t i = this->m_size; i > pos; --i)
						OOBase::swap(this->m_data[i],this->m_data[i-1]);
				}

				++this->m_size;
				return true;
			}

			size_t remove_at(size_t pos, size_t len)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (len > this->m_size - pos)
						len = this->m_size - pos;
					if (len)
					{
						size_t orig_len = this->m_size;
						this->m_size -= len;

						for(;pos < this->m_size;++pos)
							OOBase::swap(this->m_data[pos],this->m_data[pos+len]);

						for(;pos < orig_len;++pos)
							this->m_data[pos].~T();
					}
				}
				return pos < this->m_size ? pos : size_t(-1);
			}
		};

		template <typename Allocator, typename T>
		class VectorPODBase<Allocator,T,true> : public VectorBase<Allocator,T>
		{
			typedef VectorBase<Allocator,T> baseClass;

		public:
			VectorPODBase() : baseClass()
			{}

			VectorPODBase(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			VectorPODBase(const VectorPODBase& rhs) : baseClass(rhs)
			{
				if (rhs.m_size)
				{
					if (!reserve(rhs.m_size))
						OOBase_CallCriticalFailure(system_error());
					else
					{
						memcpy(this->m_data,rhs.m_data,rhs.m_size*sizeof(T));
						this->m_size = rhs.m_size;
					}
				}
			}

			VectorPODBase& operator = (const VectorPODBase& rhs)
			{
				this->clear();
				if (rhs.m_size)
				{
					if (!reserve(rhs.m_size))
						OOBase_CallCriticalFailure(system_error());
					else
					{
						memcpy(this->m_data,rhs.m_data,rhs.m_size*sizeof(T));
						this->m_size = rhs.m_size;
					}
				}
				return *this;
			}

		protected:
			bool assign(size_t n)
			{
				return assign(n,T());
			}

			bool assign(size_t n, typename call_traits<T>::param_type value)
			{
				if (!reserve(n))
					return false;

				for (size_t i=0; i<n; ++i)
					this->m_data[i] = value;

				this->m_size = n;
				return true;
			}

			bool reserve(size_t n)
			{
				if (n > this->m_capacity)
				{
					size_t capacity = (n/2 + 1) * 2;
					T* new_data = static_cast<T*>(baseClass::reallocate(this->m_data,capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return false;

					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return true;
			}

			bool insert_at(size_t& pos, typename call_traits<T>::param_type value)
			{
				if (pos > this->m_size)
					pos = this->m_size;

				if (this->m_size >= this->m_capacity)
				{
					if (!reserve(this->m_capacity ? this->m_capacity * 2 : 8))
						return false;
				}

				if (pos < this->m_size)
					memmove(&this->m_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));

				this->m_data[pos] = value;

				++this->m_size;
				return true;
			}

			size_t remove_at(size_t pos, size_t len)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (len > this->m_size - pos)
						len = this->m_size - pos;
					if (len)
					{
						this->m_size -= len;
						if (pos < this->m_size)
							memmove(&this->m_data[pos],&this->m_data[pos+len],(this->m_size - (pos + len)) * sizeof(T));
					}
				}
				return pos < this->m_size ? pos : size_t(-1);
			}
		};

		template <typename T, typename Allocator>
		class VectorImpl : public VectorPODBase<Allocator,T,is_pod<T>::value>
		{
			typedef VectorPODBase<Allocator,T,is_pod<T>::value> baseClass;

		public:
			VectorImpl() : baseClass()
			{}

			VectorImpl(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			VectorImpl(const VectorImpl& rhs) : baseClass(rhs)
			{}

		protected:
			typedef typename add_const<T&>::type const_reference;
			typedef typename add_const<T*>::type const_pointer;

			template <typename It>
			bool assign(It first, It last)
			{
				assert(last >= first);
				baseClass::clear();
				baseClass::reserve(last - first);
				for (It i = first; i != last; ++i)
				{
					if (!push_back(*i))
						return false;
				}
				return true;
			}

			T* data()
			{
				return this->m_data;
			}

			const_pointer data() const
			{
				return this->m_data;
			}

			bool push_back(typename call_traits<T>::param_type value)
			{
				return baseClass::insert_at(this->m_size,value);
			}

			bool pop_back()
			{
				baseClass::remove_at(this->m_size - 1,1);
				return !baseClass::empty();
			}

			void shuffle(Random& random)
			{
				if (this->m_size > 1)
				{
					for (size_t i = 0;i < this->m_size - 1; ++i)
					{
						size_t r = random.next<size_t>(i,this->m_size);
						OOBase::swap(this->m_data[i],this->m_data[r]);
					}
				}
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

		Vector() : baseClass(), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Vector(AllocatorInstance& allocator) : baseClass(allocator), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Vector(const Vector& rhs) : baseClass(rhs), m_end(NULL,size_t(-1)), m_cend(NULL,size_t(-1))
		{
			iterator(this,size_t(-1)).swap(m_end);
			const_iterator(this,size_t(-1)).swap(m_cend);
		}

		Vector& operator = (const Vector& rhs)
		{
			Vector(rhs).swap(*this);
			return *this;
		}

		template <typename It>
		bool assign(It first, It last)
		{
			return baseClass::assign(first,last);
		}

		bool assign(size_t n)
		{
			return assign(n,T());
		}

		bool assign(size_t n, typename call_traits<T>::param_type value)
		{
			return baseClass::assign(n,value);
		}

		bool reserve(size_t n)
		{
			return baseClass::reserve(n);
		}

		pointer data()
		{
			return baseClass::data();
		}

		const_pointer data() const
		{
			return baseClass::data();
		}

		bool resize(size_t new_size, typename call_traits<T>::param_type value)
		{
			bool ret = reserve(new_size);
			while (this->m_size < new_size && ret)
				ret = baseClass::push_back(value);
			while (this->m_size > new_size && ret)
				ret = baseClass::pop_back();
			return ret;
		}

		bool resize(size_t new_size)
		{
			return resize(new_size,T());
		}

		iterator push_back(typename call_traits<T>::param_type value)
		{
			return baseClass::push_back(value) ? iterator(this,this->m_size-1) : m_end;
		}

		bool pop_back()
		{
			return baseClass::pop_back();
		}

		iterator insert(typename call_traits<T>::param_type value, const iterator& before)
		{
			assert(before.check(this));
			return insert(value,before.deref());
		}

		iterator insert(typename call_traits<T>::param_type value, size_t before)
		{
			return baseClass::insert_at(before,value) ? iterator(this,before) : m_end;
		}

		iterator erase(iterator iter)
		{
			return erase(iter,1);
		}

		iterator erase(size_t pos)
		{
			return erase(pos,1);
		}

		iterator erase(iterator first, iterator last)
		{
			assert(first.check(this) && last.check(this) && last >= first);
			return erase(first.deref(),last.deref() - first.deref());
		}

		iterator erase(iterator first, size_t count)
		{
			assert(first.check(this));
			return erase(first.deref(),count);
		}

		iterator erase(size_t first, size_t count)
		{
			return iterator(this,baseClass::remove_at(first,count));
		}

		template <typename T1>
		size_t remove(const T1& value)
		{
			size_t ret = 0;
			for (size_t pos = 0;pos < this->m_size;)
			{
				if (this->m_data[pos] == value)
				{
					baseClass::remove_at(pos,1);
					++ret;
				}
				else
					++pos;
			}
			return ret;
		}

		template <typename T1>
		bool exists(const T1& value) const
		{
			return find_i(value) != size_t(-1);
		}

		template <typename T1>
		iterator find(const T1& value)
		{
			return iterator(this,find_i(value));
		}

		template <typename T1>
		const_iterator find(const T1& value) const
		{
			return const_iterator(this,find_i(value));
		}

		iterator position(size_t pos)
		{
			if (pos >= this->m_size)
				return m_end;

			return iterator(this,pos);
		}

		const_iterator position(size_t pos) const
		{
			if (pos >= this->m_size)
				return m_cend;

			return const_iterator(this,pos);
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
			return baseClass::empty() ? m_end : iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return baseClass::empty() ? m_cend : const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : m_end);
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : m_cend);
		}

		const iterator& end()
		{
			return m_end;
		}

		const const_iterator& cend() const
		{
			return m_cend;
		}

		const const_iterator& end() const
		{
			return m_cend;
		}

		void shuffle(Random& random)
		{
			baseClass::shuffle(random);
		}

	private:
		iterator m_end;
		const_iterator m_cend;

		template <typename T1>
		size_t find_i(const T1& value) const
		{
			for (size_t pos = 0;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
					return pos;
			}
			return size_t(-1);
		}
	};
}

#endif // OOBASE_VECTOR_H_INCLUDED_
