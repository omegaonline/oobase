///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#ifndef OOBASE_BAG_H_INCLUDED_
#define OOBASE_BAG_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Allocator, typename T>
		class PODBagBase : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			PODBagBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			PODBagBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

		protected:
			T*     m_data;
			size_t m_size;
			size_t m_capacity;
		};

		template <typename Allocator, typename T, bool POD = false>
		class PODBag : public PODBagBase<Allocator,T>
		{
			typedef PODBagBase<Allocator,T> baseClass;

		public:
			PODBag() : baseClass()
			{}

			PODBag(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				while (this->m_size > 0)
					this->m_data[--this->m_size].~T();
			}

		protected:
			static void copy_to(T* p, const T& v)
			{
				// Placement new with copy constructor
				::new (p) T(v);
			}

			int reserve_i(size_t capacity)
			{
				if (this->m_capacity < capacity)
				{
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

#if defined(OOBASE_HAVE_EXCEPTIONS)
					size_t i = 0;
					try
					{
						for (i=0;i<this->m_size;++i)
							copy_to(&new_data[i],this->m_data[i]);
					}
					catch (...)
					{
						for (;i>0;--i)
							new_data[i-1].~T();

						baseClass::free(new_data);
						throw;
					}

					for (i=0;i<this->m_size;++i)
						this->m_data[i].~T();

#else
					for (size_t i=0;i<this->m_size;++i)
					{
						copy_to(&new_data[i],this->m_data[i]);
						this->m_data[i].~T();
					}
#endif

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}
		};

		template <typename Allocator, typename T>
		class PODBag<Allocator,T,true> : public PODBagBase<Allocator,T>
		{
			typedef PODBagBase<Allocator,T> baseClass;

		public:
			PODBag() : baseClass()
			{}

			PODBag(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			void clear()
			{
				this->m_size = 0;
			}

		protected:
			static void copy_to(T* p, const T& v)
			{
				*p = v;
			}

			int reserve_i(size_t capacity)
			{
				if (this->m_capacity < capacity)
				{
					T* new_data = static_cast<T*>(baseClass::reallocate(this->m_data,capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}
		};

		template <typename T, typename Allocator>
		class BagImpl : public PODBag<Allocator,T,is_pod<T>::value>
		{
			typedef PODBag<Allocator,T,is_pod<T>::value> baseClass;

		public:
			BagImpl() : baseClass()
			{}

			BagImpl(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			~BagImpl()
			{
				destroy();
			}

			bool empty() const
			{
				return (this->m_size == 0);
			}

			size_t size() const
			{
				return this->m_size;
			}

		protected:
			int add(const T& value)
			{
				if (this->m_size+1 > this->m_capacity)
				{
					size_t cap = (this->m_capacity == 0 ? 4 : this->m_capacity*2);
					int err = baseClass::reserve_i(cap);
					if (err != 0)
						return err;
				}

				baseClass::copy_to(&this->m_data[this->m_size],value);

				// This is exception safe as the increment happens here
				++this->m_size;
				return 0;
			}

			bool pop(T* value = NULL)
			{
				if (this->m_size == 0)
					return false;

				if (value)
					*value = this->m_data[this->m_size-1];

				this->m_data[--this->m_size].~T();
				return true;
			}

			void remove_at(size_t pos)
			{
				if (this->m_data && pos < this->m_size)
				{
					this->m_data[pos] = this->m_data[--this->m_size];
					this->m_data[this->m_size].~T();
				}
			}

			T* at(size_t pos)
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			const T* at(size_t pos) const
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			void destroy()
			{
				baseClass::clear();
				this->free(this->m_data);
			}

			void remove_at_sorted(size_t pos)
			{
				if (this->m_data && pos < this->m_size)
				{
					for(--this->m_size;pos < this->m_size;++pos)
						this->m_data[pos] = this->m_data[pos+1];

					this->m_data[this->m_size].~T();
				}
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Bag : public detail::BagImpl<T,Allocator>
	{
		typedef detail::BagImpl<T,Allocator> baseClass;

	public:
		Bag() : baseClass()
		{}

		Bag(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		int add(const T& value)
		{
			return baseClass::add(value);
		}

		bool remove(const T& value)
		{
			// This is just really useful!
			for (size_t pos = 0;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
				{
					baseClass::remove_at(pos);
					return true;
				}
			}

			return false;
		}

		bool pop(T* value = NULL)
		{
			return baseClass::pop(value);
		}

		void remove_at(size_t pos)
		{
			baseClass::remove_at(pos);
		}

		T* at(size_t pos)
		{
			return baseClass::at(pos);
		}

		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
		}
	};
}

#endif // OOBASE_BAG_H_INCLUDED_
