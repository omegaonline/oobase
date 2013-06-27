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

#include "Memory.h"

namespace OOBase
{
	namespace detail
	{
		template <typename Container, typename T, typename Iter>
		class IteratorImpl
		{
			// This is just to make Container a friend!
			template <typename T1>
			struct friend_helper
			{
			   typedef T1 type;
			};
#if defined(_MSC_VER)
			friend typename friend_helper<Container>::type;
#else
			friend class friend_helper<Container>::type;
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

#endif // OOBASE_ITERATOR_H_INCLUDED_
