///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#ifndef OOBASE_ATOMIC_H_INCLUDED_
#define OOBASE_ATOMIC_H_INCLUDED_

#include "Base.h"

namespace OOBase
{
	namespace detail
	{
		template <typename T, const size_t S>
		struct AtomicImpl;

		void atomic_memory_barrier();
	}

	template <typename T>
	class Atomic
	{
	public:
		Atomic() : m_val()
		{}

		Atomic(const T v) : m_val(v)
		{}

		Atomic(const Atomic& a) : m_val(a.m_val)
		{}

		~Atomic()
		{}

		Atomic& operator = (const Atomic& a)
		{
			if (&a != this)
				detail::AtomicImpl<T,sizeof(T)>::Exchange(m_val,a.m_val);
			return *this;
		}

		static T Exchange(T& val, const T newVal)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Exchange(val,newVal);
		}

		T Exchange(const T newVal)
		{
			return Exchange(m_val,newVal);
		}

		static T CompareAndSwap(T& val, const T oldVal, const T newVal)
		{
			return detail::AtomicImpl<T,sizeof(T)>::CompareAndSwap(val,oldVal,newVal);
		}

		T CompareAndSwap(const T oldVal, const T newVal)
		{
			return CompareAndSwap(m_val,oldVal,newVal);
		}

		operator T () const
		{
			detail::atomic_memory_barrier();

			return m_val;
		}

		static T Increment(T& val)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Increment(val);
		}

		T operator ++()
		{
			return Increment(m_val);
		}

		static T Decrement(T& val)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Decrement(val);
		}

		T operator --()
		{
			return Decrement(m_val);
		}

		T operator ++(int)
		{
			return Increment(m_val) - 1;
		}

		T operator --(int)
		{
			return Decrement(m_val) + 1;
		}

		static T Add(T& val, const T add)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Add(val,add);
		}

		Atomic& operator += (T val)
		{
			Add(m_val,val);
			return *this;
		}

		Atomic& operator += (const Atomic& rhs)
		{
			return *this += rhs.m_val;
		}

		static T Subtract(T& val, const T subtract)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Subtract(val,subtract);
		}

		Atomic& operator -= (T val)
		{
			Subtract(m_val,val);
			return *this;
		}

		Atomic& operator -= (const Atomic& rhs)
		{
			return *this -= rhs.m_val;
		}

	private:
		T m_val;
	};

	namespace detail
	{
		int32_t atomic_cas_4(int32_t volatile* val, const int32_t oldVal, const int32_t newVal);
		int32_t atomic_swap_4(int32_t volatile* val, const int32_t newVal);
		int32_t atomic_add_4(int32_t volatile* val, const int32_t add);
		int32_t atomic_inc_4(int32_t volatile* val);
		int32_t atomic_sub_4(int32_t volatile* val, const int32_t sub);
		int32_t atomic_dec_4(int32_t volatile* val);

		template <typename T>
		struct AtomicImpl<T,4>
		{
			static T CompareAndSwap(T& val, const T oldVal, const T newVal)
			{
				return (T)atomic_cas_4((int32_t volatile*)(&val),(const int32_t)oldVal,(const int32_t)newVal);
			}

			static T Exchange(T& val, const T newVal)
			{
				return (T)atomic_swap_4((int32_t volatile*)(&val),(const int32_t)newVal);
			}

			static T Add(T& val, const T add)
			{
				return (T)atomic_add_4((int32_t volatile*)(&val),(const int32_t)add);
			}

			static T Increment(T& val)
			{
				return (T)atomic_inc_4((int32_t volatile*)(&val));
			}

			static T Subtract(T& val, const T sub)
			{
				return (T)atomic_sub_4((int32_t volatile*)(&val),(const int32_t)sub);
			}

			static T Decrement(T& val)
			{
				return (T)atomic_dec_4((int32_t volatile*)(&val));
			}
		};

		int64_t atomic_cas_8(int64_t volatile* val, const int64_t oldVal, const int64_t newVal);
		int64_t atomic_swap_8(int64_t volatile* val, const int64_t newVal);
		int64_t atomic_add_8(int64_t volatile* val, const int64_t add);
		int64_t atomic_inc_8(int64_t volatile* val);
		int64_t atomic_sub_8(int64_t volatile* val, const int64_t sub);
		int64_t atomic_dec_8(int64_t volatile* val);

		template <typename T>
		struct AtomicImpl<T,8>
		{
			static T CompareAndSwap(T& val, const T oldVal, const T newVal)
			{
				return (T)atomic_cas_8((int64_t volatile*)(&val),(const int64_t)(oldVal),(const int64_t)newVal);
			}

			static T Exchange(T& val, const T newVal)
			{
				return (T)atomic_swap_8((int64_t volatile*)(&val),(const int64_t)newVal);
			}

			static T Add(T& val, const T add)
			{
				return (T)atomic_add_8((int64_t volatile*)(&val),(const int64_t)add);
			}

			static T Increment(T& val)
			{
				return (T)atomic_inc_8((int64_t volatile*)(&val));
			}

			static T Subtract(T& val, const T sub)
			{
				return (T)atomic_sub_8((int64_t volatile*)(&val),(const int64_t)sub);
			}

			static T Decrement(T& val)
			{
				return (T)atomic_dec_8((int64_t volatile*)(&val));
			}
		};
	}
}

#endif // OOBASE_ATOMIC_H_INCLUDED_
