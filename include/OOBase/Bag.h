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
		template <typename Allocator, typename T, bool POD = false>
		class BagBase : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			BagBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			BagBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

			~BagBase()
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
			bool remove_at(size_t pos, bool sorted, T* pval = NULL)
			{
				if (m_data && pos < m_size)
				{
					if (pval)
						*pval = m_data[pos];

					if (sorted)
					{
						for(--m_size;pos < m_size;++pos)
							m_data[pos] = m_data[pos+1];
					}
					else
						m_data[pos] = m_data[--m_size];

					m_data[m_size].~T();
					return true;
				}
				else
					return false;
			}

		private:
			size_t m_capacity;
		};

		template <typename Allocator, typename T>
		class BagBase<Allocator,T,true> : public Allocating<Allocator>, public NonCopyable
		{
			typedef Allocating<Allocator> baseClass;

		public:
			BagBase() : baseClass(), m_data(NULL), m_size(0), m_capacity(0)
			{}

			BagBase(AllocatorInstance& allocator) : baseClass(allocator), m_data(NULL), m_size(0), m_capacity(0)
			{}

			~BagBase()
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

			bool remove_at(size_t pos, bool sorted, T* pval = NULL)
			{
				if (m_data && pos < m_size)
				{
					if (pval)
						*pval = m_data[pos];

					--m_size;
					if (pos < m_size)
					{
						if (sorted)
							memmove(&m_data[pos],&m_data[pos+1],(m_size - pos) * sizeof(T));
						else
							m_data[pos] = m_data[m_size+1];
					}

					return true;
				}
				else
					return false;
			}

		private:
			size_t m_capacity;
		};

		template <typename T, typename Allocator>
		class BagImpl : public BagBase<Allocator,T,is_pod<T>::value>
		{
			typedef BagBase<Allocator,T,is_pod<T>::value> baseClass;

		public:
			BagImpl() : baseClass()
			{}

			BagImpl(AllocatorInstance& allocator) : baseClass(allocator)
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
			int append(const T& val)
			{
				return baseClass::insert_at(this->m_size,val);
			}

			T* at(size_t pos)
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}

			const T* at(size_t pos) const
			{
				return (pos >= this->m_size ? NULL : &this->m_data[pos]);
			}
		};

		template <typename Container, typename T, typename Iter>
		class IteratorImpl
		{
			// This is just to make Container a friend!
			template <typename T1>
			struct type_wrapper
			{
			   typedef T1 type;
			};
#if defined(_MSC_VER)
			friend typename type_wrapper<Container>::type;
#else
			friend class type_wrapper<Container>::type;
#endif

		public:
			IteratorImpl(const IteratorImpl& rhs) : m_cont(rhs.m_cont), m_pos(rhs.m_pos)
			{}

			IteratorImpl& operator = (const IteratorImpl& rhs)
			{
				if (this != &rhs)
				{
					m_cont = rhs.m_cont;
					m_pos = rhs.m_pos;
				}
				return *this;
			}

			bool operator == (const IteratorImpl& rhs) const
			{
				return (m_cont == rhs.m_cont && m_pos == rhs.m_pos);
			}

			bool operator != (const IteratorImpl& rhs) const
			{
				return !(*this == rhs);
			}

			T* operator -> () const
			{
				assert(m_cont->at(deref()) != NULL);
				return m_cont->at(deref());
			}

			T& operator * () const
			{
				assert(m_cont->at(deref()) != NULL);
				return *m_cont->at(deref());
			}

			IteratorImpl& operator ++ ()
			{
				m_cont->next(m_pos);
				return *this;
			}

			IteratorImpl operator ++ (int)
			{
				IteratorImpl r(*this);
				++(*this);
				return r;
			}

			IteratorImpl& operator -- ()
			{
				m_cont->prev(m_pos);
				return *this;
			}

			IteratorImpl operator -- (int)
			{
				IteratorImpl r(*this);
				--(*this);
				return r;
			}

		protected:
			IteratorImpl(Container* cont, Iter pos) : m_cont(cont), m_pos(pos)
			{}

#if !defined(NDEBUG)
			bool check(Container* cont) const
			{
				return (cont && cont == m_cont);
			}
#endif
			Iter deref() const
			{
				return m_pos;
			}

			Iter& deref()
			{
				return m_pos;
			}

		private:
			IteratorImpl();

			Container* m_cont;
			Iter       m_pos;
		};
	}
}

#endif // OOBASE_BAG_H_INCLUDED_
