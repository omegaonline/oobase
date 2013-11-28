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
		template <typename Allocator, typename T, bool POD = false>
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
				clear();
				baseClass::free(m_data);
			}

			void clear()
			{
				while (m_size > 0)
					m_data[--m_size].~T();
			}

		protected:
			T*     m_data;
			size_t m_size;

#if defined(OOBASE_HAVE_EXCEPTIONS)
			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
				if (m_size >= m_capacity || pos < m_size)
				{
					// Copy buffer, inserting in the gap...
					size_t capacity = m_capacity;
					if (m_size+1 > m_capacity)
						capacity = (m_capacity == 0 ? 4 : m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					size_t i = 0;
					try
					{
						for (;i<m_size;++i)
							::new (&new_data[i + (i<pos ? 0 : 1)]) T(m_data[i]);

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

					for (i=0;i<m_size;++i)
						m_data[i].~T();

					baseClass::free(m_data);
					m_data = new_data;
					m_capacity = capacity;
				}
				else
					::new (&m_data[m_size]) T(value);

				++m_size;
				return 0;
			}
#else
			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
				if (m_size >= m_capacity)
				{
					size_t capacity = (m_capacity == 0 ? 4 : m_capacity*2);
					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					for (size_t i = 0;i<m_size;++i)
					{
						::new (&new_data[i + (i<pos ? 0 : 1)]) T(m_data[i]);
						m_data[i].~T();
					}

					::new (&new_data[pos]) T(value);

					baseClass::free(m_data);
					m_data = new_data;
					m_capacity = capacity;
				}
				else if (pos < m_size)
				{
					// Shuffle the contents down the buffer
					::new (&m_data[m_size]) T(m_data[m_size-1]);
					for (size_t i = m_size-1; i > pos; --i)
						m_data[i] = m_data[i-1];

					m_data[pos] = value;
				}
				else
					::new (&m_data[m_size]) T(value);

				++m_size;
				return 0;
			}
#endif
			bool remove_at(size_t pos, T* pval)
			{
				if (m_data && pos < m_size)
				{
					if (pval)
						*pval = m_data[pos];

					for(--m_size;pos < m_size;++pos)
						m_data[pos] = m_data[pos+1];

					m_data[m_size].~T();

					return true;
				}

				return false;
			}

		private:
			size_t m_capacity;
		};

		template <typename Allocator, typename T>
		class VectorBase<Allocator,T,true> : public Allocating<Allocator>, public NonCopyable
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

			void clear()
			{
				m_size = 0;
			}

		protected:
			T*     m_data;
			size_t m_size;

			template <typename T1>
			int insert_at(size_t pos, T1 value)
			{
				if (m_size >= m_capacity)
				{
					size_t capacity = (m_capacity == 0 ? 4 : m_capacity*2);

					T* new_data = static_cast<T*>(baseClass::allocate(capacity*sizeof(T),alignment_of<T>::value));
					if (!new_data)
						return ERROR_OUTOFMEMORY;

					if (pos < m_size)
						memcpy(&new_data[pos + 1],&m_data[pos],(m_size - pos) * sizeof(T));
					else
						pos = m_size;

					if (pos > 0)
						memcpy(new_data,&m_data,pos * sizeof(T));

					new_data[pos] = value;

					baseClass::free(m_data);
					m_data = new_data;
					m_capacity = capacity;
				}
				else
				{
					if (pos < m_size)
						memmove(&m_data[pos + 1],&m_data[pos],(m_size - pos) * sizeof(T));
					else
						pos = m_size;

					m_data[pos] = value;
				}
				++m_size;
				return 0;
			}

			void remove_at(size_t pos, T* pval)
			{
				if (m_data && pos < m_size)
				{
					if (pval)
						*pval = m_data[pos];

					--m_size;
					if (pos < m_size)
						memmove(&m_data[pos],&m_data[pos+1],(m_size - pos) * sizeof(T));
				}
			}

		private:
			size_t m_capacity;
		};

		template <typename T, typename Allocator>
		class VectorImpl : public VectorBase<Allocator,T,is_pod<T>::value>
		{
			typedef VectorBase<Allocator,T,is_pod<T>::value> baseClass;

		public:
			VectorImpl() : baseClass()
			{}

			VectorImpl(AllocatorInstance& allocator) : baseClass(allocator)
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
			template <typename T1>
			int push_back(T1 value)
			{
				return baseClass::insert_at(this->m_size,value);
			}

			bool pop_back(T* pval = NULL)
			{
				baseClass::remove_at(this->m_size - 1,pval);
				return !empty();
			}

			T* at(size_t pos)
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			const T* at(size_t pos) const
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			void next(size_t& pos) const
			{
				if (pos < this->m_size - 1)
					++pos;
				else
					pos = size_t(-1);
			}

			void prev(size_t& pos) const
			{
				if (pos >= this->m_size)
					pos = this->m_size-1;
				else
					--pos;
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
		int push_back(T1 value)
		{
			return baseClass::push_back(value);
		}

		bool pop_back(T* pval = NULL)
		{
			return baseClass::pop_back(pval);
		}

		template <typename T1>
		int insert(T1 value, const iterator& before)
		{
			assert(before.check(this));
			return baseClass::insert_at(before.deref(),value);
		}

		template <typename T1>
		iterator insert(T1 value, const iterator& before, int& err)
		{
			err = baseClass::insert_at(before.deref(),value);
			return (!err ? before : end());
		}

		void remove_at(iterator& iter)
		{
			assert(iter.check(this));
			baseClass::remove_at(iter.deref(),NULL);
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
			return baseClass::at(pos);
		}

		const T* at(size_t pos) const
		{
			return baseClass::at(pos);
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

		const_iterator begin() const
		{
			return const_iterator(this,0);
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

		const_iterator end() const
		{
			return const_iterator(this,size_t(-1));
		}
	};
}

#endif // OOBASE_VECTOR_H_INCLUDED_
