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

#include "Iterator.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Allocator, typename T>
		class VectorBase : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			VectorBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			VectorBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

			~VectorBase()
			{
				baseClass::free(m_data);
			}

			T* data()
			{
				return m_data;
			}

			const T* data() const
			{
				return m_data;
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

			const T* at(size_t pos) const
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
				if (pos >= m_size)
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

			VectorPODBase(AllocatorInstance& allocator) : baseClass(allocator)
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
			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
				if (this->m_size >= this->m_capacity || pos < this->m_size)
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
					::new (&this->m_data[this->m_size]) T(value);

				++this->m_size;
				return 0;
			}
#else
			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
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
				else if (pos < this->m_size)
				{
					// Shuffle the contents down the buffer
					::new (&this->m_data[this->m_size]) T(this->m_data[this->m_size-1]);
					for (size_t i = this->m_size-1; i > pos; --i)
						this->m_data[i] = this->m_data[i-1];

					this->m_data[pos] = value;
				}
				else
					::new (&this->m_data[this->m_size]) T(value);

				++this->m_size;
				return 0;
			}
#endif
			bool remove_at(size_t pos, T* pval)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (pval)
						*pval = this->m_data[pos];

					for(--this->m_size;pos < this->m_size;++pos)
						this->m_data[pos] = this->m_data[pos+1];

					this->m_data[this->m_size].~T();

					return true;
				}

				return false;
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

		protected:
			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 2 : this->m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					if (pos < this->m_size)
						memcpy(&new_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));
					else
						pos = this->m_size;

					if (pos > 0)
						memcpy(new_data,&this->m_data,pos * sizeof(T));

					new_data[pos] = value;

					baseClass::free(this->m_data);
					this->m_data = new_data;
					this->m_capacity = capacity;
				}
				else
				{
					if (pos < this->m_size)
						memmove(&this->m_data[pos + 1],&this->m_data[pos],(this->m_size - pos) * sizeof(T));
					else
						pos = this->m_size;

					this->m_data[pos] = value;
				}
				++this->m_size;
				return 0;
			}

			void remove_at(size_t pos, T* pval)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (pval)
						*pval = this->m_data[pos];

					--this->m_size;
					if (pos < this->m_size)
						memmove(&this->m_data[pos],&this->m_data[pos+1],(this->m_size - pos) * sizeof(T));
				}
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

		protected:
			template <typename T1>
			int push_back(T1 value)
			{
				return this->insert_at(this->m_size,value);
			}

			bool pop_back(T* pval = NULL)
			{
				this->remove_at(this->m_size - 1,pval);
				return !this->empty();
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Vector : public detail::VectorImpl<T,Allocator>
	{
		typedef detail::VectorImpl<T,Allocator> baseClass;

		friend class detail::IteratorImpl<Vector,T,size_t>;
		friend class detail::IteratorImpl<const Vector,const T,size_t>;

	public:
		typedef detail::IteratorImpl<Vector,T,size_t> iterator;
		typedef detail::IteratorImpl<const Vector,const T,size_t> const_iterator;

		Vector() : baseClass()
		{}

		Vector(AllocatorInstance& allocator) : baseClass(allocator)
		{}

		template <typename T1>
		iterator find(T1 value)
		{
			size_t pos = 0;
			for (;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
					iterator(this,pos);
			}
			return end();
		}

		template <typename T1>
		const_iterator find(T1 value) const
		{
			size_t pos = 0;
			for (;pos < this->m_size;++pos)
			{
				if (this->m_data[pos] == value)
					return const_iterator(this,pos);
			}
			return end();
		}

		template <typename T1>
		int resize(size_t new_size, T1 value)
		{
			int err = 0;
			while (this->m_size < new_size && !err)
				err = this->push_back(value);
			while (this->m_size > new_size && !err)
				err = this->pop_back(NULL);
			return err;
		}

		int resize(size_t new_size)
		{
			return resize(new_size,T());
		}

		template <typename T1>
		int push_back(T1 value)
		{
			return this->push_back(value);
		}

		bool pop_back(T* pval = NULL)
		{
			return this->pop_back(pval);
		}

		template <typename T1>
		int insert(T1 value, const iterator& before)
		{
			assert(before.check(this));
			return this->insert_at(before.deref(),value);
		}

		template <typename T1>
		iterator insert(T1 value, const iterator& before, int& err)
		{
			err = this->insert_at(before.deref(),value);
			return (!err ? before : end());
		}

		void remove_at(iterator& iter)
		{
			assert(iter.check(this));
			this->remove_at(iter.deref(),NULL);
		}

		template <typename T1>
		bool remove(T1 value)
		{
			iterator i = find(value);
			if (i == end())
				return false;

			remove_at(i);
			return true;
		}

		T* at(size_t pos)
		{
			return this->at(pos);
		}

		const T* at(size_t pos) const
		{
			return this->at(pos);
		}

		T& operator [](size_t pos)
		{
			return *at(pos);
		}

		const T& operator [](size_t pos) const
		{
			return *at(pos);
		}

		iterator begin()
		{
			return iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : end());
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : end());
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
