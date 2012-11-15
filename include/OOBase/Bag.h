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

#if defined(_MSC_VER)
#define HAVE__IS_POD 1
#elif defined(__clang__)
#if __has_extension(is_pod)
#define HAVE__IS_POD 1
#endif
#elif defined(__GNUG__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
#define HAVE__IS_POD 1
#endif

#if !defined(HAVE__IS_POD)
#include <limits>
#endif

namespace OOBase
{
	namespace detail
	{
		template <typename T>
		struct is_pod
		{
#if defined(HAVE__IS_POD)
			static const bool value = __is_pod(T);
#else
			static const bool value = std::numeric_limits<T>::is_integer || std::numeric_limits<T>::is_signed;
#endif
		};

#if !defined(HAVE__IS_POD)
		template <>
		struct is_pod<void>
		{
			static const bool value = true;
		};

		template <typename T>
		struct is_pod<T&>
		{
			static const bool value = false;
		};

		template <typename T>
		struct is_pod<T*>
		{
			static const bool value = is_pod<T>::value;
		};

		template <typename T>
		struct is_pod<T const>
		{
			static const bool value = is_pod<T>::value;
		};

		template <typename T>
		struct is_pod<T volatile>
		{
			static const bool value = is_pod<T>::value;
		};
#endif

		template <typename T>
		class PODBagBase
		{
		public:
			PODBagBase() : m_data(NULL), m_size(0), m_capacity(0)
			{}

		protected:
			T*     m_data;
			size_t m_size;
			size_t m_capacity;

		private:
			// Do not allow copy constructors or assignment
			// as memory allocation will occur...
			// and you probably don't want to be copying these around
			PODBagBase(const PODBagBase&);
			PODBagBase& operator = (const PODBagBase&);
		};

		template <typename T, bool POD = false>
		class PODBag : public PODBagBase<T>
		{
		public:
			void clear()
			{
				while (this->m_size > 0)
					this->m_data[--this->m_size].~T();
			}

		protected:
			virtual ~PODBag()
			{}

			static void copy_to(T* p, const T& v)
			{
				// Placement new with copy constructor
				::new (p) T(v);
			}

			virtual void* allocate_i(size_t bytes, size_t align) = 0;
			virtual void free_i(void* ptr) = 0;

			int reserve_i(size_t capacity)
			{
				if (this->m_capacity < capacity)
				{
					T* new_data = static_cast<T*>(allocate_i(capacity*sizeof(T),detail::alignof<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

#if defined(OOBASE_HAVE_EXCEPTIONS)
					size_t i = 0;
					try
					{
						for (i=0;i<this->m_size;++i)
							::new (&new_data[i]) T(this->m_data[i]);
					}
					catch (...)
					{
						for (;i>0;--i)
							new_data[i-1].~T();

						free_i(new_data);
						throw;
					}

					for (i=0;i<this->m_size;++i)
						this->m_data[i].~T();

#else
					for (size_t i=0;i<this->m_size;++i)
					{
						::new (&new_data[i]) T(this->m_data[i]);
						this->m_data[i].~T();
					}
#endif

					free_i(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}
		};

		template <typename T>
		class PODBag<T,true> : public PODBagBase<T>
		{
		public:
			void clear()
			{
				this->m_size = 0;
			}

		protected:
			virtual ~PODBag()
			{}

			static void copy_to(T* p, const T& v)
			{
				*p = v;
			}

			virtual void* reallocate_i(void* ptr, size_t bytes, size_t align) = 0;
			virtual void free_i(void* ptr) = 0;

			int reserve_i(size_t capacity)
			{
				if (this->m_capacity < capacity)
				{
					T* new_data = static_cast<T*>(reallocate_i(this->m_data,capacity*sizeof(T),detail::alignof<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				return 0;
			}
		};

		template <typename T>
		class BagImpl : public PODBag<T,is_pod<T>::value>
		{
			typedef PODBag<T,is_pod<T>::value> baseClass;

		public:
			BagImpl() : baseClass()
			{}

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
				this->free_i(this->m_data);
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

		template <typename BaseClass, typename Allocator = CrtAllocator>
		class AllocImpl : public BaseClass
		{
		public:
			AllocImpl() : BaseClass()
			{}

			virtual ~AllocImpl()
			{}

		private:
			void* allocate_i(size_t bytes, size_t align)
			{
				return Allocator::allocate(bytes,align);
			}

			void* reallocate_i(void* ptr, size_t bytes, size_t align)
			{
				return Allocator::reallocate(ptr,bytes,align);
			}

			void free_i(void* ptr)
			{
				Allocator::free(ptr);
			}
		};

		template <typename BaseClass>
		class AllocImpl<BaseClass,AllocatorInstance> : public BaseClass
		{
		public:
			AllocImpl(AllocatorInstance& allocator) : BaseClass(), m_allocator(allocator)
			{}

			virtual ~AllocImpl()
			{}

			AllocatorInstance& get_allocator()
			{
				return m_allocator;
			}

		private:
			AllocatorInstance& m_allocator;

			void* allocate_i(size_t bytes, size_t align)
			{
				return m_allocator.allocate(bytes,align);
			}

			void* reallocate_i(void* ptr, size_t bytes, size_t align)
			{
				return m_allocator.reallocate(ptr,bytes,align);
			}

			void free_i(void* ptr)
			{
				m_allocator.free(ptr);
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Bag : public detail::AllocImpl<detail::BagImpl<T>,Allocator >
	{
		typedef detail::AllocImpl<detail::BagImpl<T>,Allocator > baseClass;

	public:
		Bag() : baseClass()
		{}

		Bag(Allocator& allocator) : baseClass(allocator)
		{}

		virtual ~Bag()
		{
			baseClass::destroy();
		}

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
