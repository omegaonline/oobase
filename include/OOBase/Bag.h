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
#if defined(OOBASE_HAVE_EXCEPTIONS)
			int insert_at(size_t pos, const T& value)
			{
				if (this->m_size >= this->m_capacity || pos < this->m_size)
				{
					// Copy buffer, inserting in the gap...
					size_t capacity = this->m_capacity;
					if (this->m_size+1 > this->m_capacity)
						capacity = (this->m_capacity == 0 ? 4 : this->m_capacity*2);

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
			int insert_at(size_t pos, const T& value)
			{
				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 4 : this->m_capacity*2);
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
			bool remove_at(size_t pos, bool sorted, T* pval = NULL)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (pval)
						*pval = this->m_data[pos];

					if (sorted)
					{
						for(--this->m_size;pos < this->m_size;++pos)
							this->m_data[pos] = this->m_data[pos+1];
					}
					else
						this->m_data[pos] = this->m_data[--this->m_size];

					this->m_data[this->m_size].~T();
					return true;
				}
				else
					return false;
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
			int insert_at(size_t pos, const T& value)
			{
				if (this->m_size >= this->m_capacity)
				{
					size_t capacity = (this->m_capacity == 0 ? 4 : this->m_capacity*2);

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

			bool remove_at(size_t pos, bool sorted, T* pval = NULL)
			{
				if (this->m_data && pos < this->m_size)
				{
					if (pval)
						*pval = this->m_data[pos];

					--this->m_size;
					if (pos < this->m_size)
					{
						if (sorted)
							memmove(&this->m_data[pos],&this->m_data[pos+1],(this->m_size - pos) * sizeof(T));
						else
							this->m_data[pos] = this->m_data[this->m_size+1];
					}

					return true;
				}
				else
					return false;
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

			void destroy()
			{
				baseClass::clear();
				this->free(this->m_data);
			}
		};

		template <class Container, class T, class Iter>
		class IteratorImpl
		{
			friend Container;

		public:
			IteratorImpl() : m_cont(NULL)
			{}

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
				return (&m_cont == &rhs.m_cont && m_pos == rhs.m_pos);
			}

			bool operator != (const IteratorImpl& rhs) const
			{
				return !(*this == rhs);
			}

			T* operator -> () const
			{
				assert(m_cont.at(*this) != NULL);
				return m_cont.at(*this);
			}

			T& operator * () const
			{
				assert(m_cont.at(*this) != NULL);
				return *m_cont.at(*this);
			}

			IteratorImpl& operator ++ ()
			{
				if (*this != m_cont.end())
					++m_pos;
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
				if (*this != m_cont.begin())
					--m_pos;
				return *this;
			}

			IteratorImpl operator -- (int)
			{
				IteratorImpl r(*this);
				--(*this);
				return r;
			}

		protected:
			IteratorImpl(Container& cont, Iter pos) : m_cont(cont), m_pos(pos)
			{}

#if !defined(NDEBUG)
			bool check(Container const* cont) const
			{
				return (cont == &m_cont);
			}
#endif
			Iter deref() const
			{
				return m_pos;
			}

		private:
			Container& m_cont;
			Iter       m_pos;
		};
	}
}

#endif // OOBASE_BAG_H_INCLUDED_
