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

#ifndef OOBASE_ITERATOR_H_INCLUDED_
#define OOBASE_ITERATOR_H_INCLUDED_

#include "Base.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Container, typename T, typename Iter>
		class IteratorImpl : public SafeBoolean
		{
			// This is just to make Container a friend!
			template <typename T1>
			struct friend_helper
			{
			   typedef T1 type;
			};

			friend typename friend_helper<Container>::type;
			friend typename Container::const_iterator;
			friend typename Container::iterator;

		public:
			IteratorImpl(const typename Container::const_iterator& rhs) : m_cont(rhs.m_cont), m_pos(rhs.m_pos)
			{}

			IteratorImpl(const typename Container::iterator& rhs) : m_cont(rhs.m_cont), m_pos(rhs.m_pos)
			{}

			IteratorImpl& operator = (const typename Container::const_iterator& rhs)
			{
				IteratorImpl(rhs).swap(*this);
				return *this;
			}

			IteratorImpl& operator = (const typename Container::iterator& rhs)
			{
				IteratorImpl(rhs).swap(*this);
				return *this;
			}

			void swap(IteratorImpl& rhs)
			{
				OOBase::swap(m_cont,rhs.m_cont);
				OOBase::swap(m_pos,rhs.m_pos);
			}

			operator bool_type() const
			{
				return SafeBoolean::safe_bool(m_cont && *this != m_cont->end());
			}

			bool operator == (const IteratorImpl& rhs) const
			{
				return (m_cont == rhs.m_cont && m_pos == rhs.m_pos);
			}

			bool operator != (const IteratorImpl& rhs) const
			{
				return !(*this == rhs);
			}

			bool operator < (const IteratorImpl& rhs) const
			{
				return *this - rhs < 0;
			}

			bool operator <= (const IteratorImpl& rhs) const
			{
				return *this - rhs <= 0;
			}

			bool operator > (const IteratorImpl& rhs) const
			{
				return *this - rhs > 0;
			}

			bool operator >= (const IteratorImpl& rhs) const
			{
				return *this - rhs >= 0;
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
				m_cont->iterator_move(m_pos,1);
				return *this;
			}

			IteratorImpl operator ++ (int)
			{
				IteratorImpl r(*this);
				++(*this);
				return r;
			}

			IteratorImpl& operator += (ptrdiff_t n)
			{
				m_cont->iterator_move(m_pos,n);
				return *this;
			}

			IteratorImpl operator + (ptrdiff_t n) const
			{
				return IteratorImpl(*this) += n;
			}

			IteratorImpl& operator -- ()
			{
				m_cont->iterator_move(m_pos,-1);
				return *this;
			}

			IteratorImpl operator -- (int)
			{
				IteratorImpl r(*this);
				--(*this);
				return r;
			}

			IteratorImpl& operator -= (ptrdiff_t n)
			{
				m_cont->iterator_move(m_pos,-n);
				return *this;
			}

			IteratorImpl operator - (ptrdiff_t n) const
			{
				return IteratorImpl(*this) -= n;
			}

			ptrdiff_t operator - (const IteratorImpl& rhs) const
			{
				assert(rhs.m_cont == m_cont);
				return m_cont->iterator_diff(m_pos,rhs.m_pos);
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

	template <typename Container, typename T, typename Iter>
	detail::IteratorImpl<Container,T,Iter> operator + (ptrdiff_t n, const detail::IteratorImpl<Container,T,Iter>& i)
	{
		return detail::IteratorImpl<Container,T,Iter>(i) += n;
	}
}

#endif // OOBASE_ITERATOR_H_INCLUDED_
