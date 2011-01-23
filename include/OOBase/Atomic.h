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

#include "Mutex.h"

#if defined(DOXYGEN)

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,v)

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t)
#define ATOMIC_DEC_32(t)
#define ATOMIC_ADD_32(t,v)
#define ATOMIC_SUB_32(t,v)

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,v)

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t)
#define ATOMIC_DEC_64(t)
#define ATOMIC_ADD_64(t,v)
#define ATOMIC_SUB_64(t,v)

#elif defined(_MSC_VER)

#include <intrin.h>

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,c,x) _InterlockedCompareExchange((long volatile*)(t),(long)(x),(long)(c))

/* Define if you have atomic exchange for 32bit values */
#define ATOMIC_EXCH_32(t,v) _InterlockedExchange((long volatile*)(t),(long)(v))

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t) _InterlockedIncrement((long volatile*)(t))
#define ATOMIC_DEC_32(t) _InterlockedDecrement((long volatile*)(t))
#define ATOMIC_ADD_32(t,v) _InterlockedExchangeAdd((long volatile*)(t),(long)(v))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x) _InterlockedCompareExchange64((__int64 volatile*)(t),(__int64)(x),(__int64)(c))

/* Define if you have atomic exchange for 64bit values */
#define ATOMIC_EXCH_64(t,v) _InterlockedExchange64((__int64 volatile*)(t),(__int64)(v))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t) _InterlockedIncrement64((__int64 volatile*)(t))
#define ATOMIC_DEC_64(t) _InterlockedDecrement64((__int64 volatile*)(t))
#define ATOMIC_ADD_64(t,v) _InterlockedExchangeAdd64((__int64 volatile*)(t),(__int64)(v))

#elif defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,c,x) __sync_val_compare_and_swap((int volatile*)(t),c,x)

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x)  __sync_val_compare_and_swap((long long*)(t),c,x)

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_ADD_32(t,v) __sync_add_and_fetch((int volatile*)(t),v)
#define ATOMIC_SUB_32(t,v) __sync_sub_and_fetch((int volatile*)(t),-v)

/* Define if you have atomic inc and dec for 64bit values */
#define ATOMIC_ADD_64(t,v) __sync_add_and_fetch((long long volatile*)(t),v)
#define ATOMIC_SUB_64(t,v) __sync_sub_and_fetch((long long volatile*)(t),v)

#elif defined(_WIN32)

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,c,x) InterlockedCompareExchange((long volatile*)(t),(long)(x),(long)(c))

/* Define if you have atomic exchange for 32bit values */
#define ATOMIC_EXCH_32(t,v) InterlockedExchange((long volatile*)(t),(long)(v))

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t) InterlockedIncrement((LONG volatile*)(t))
#define ATOMIC_DEC_32(t) InterlockedDecrement((LONG volatile*)(t))
#define ATOMIC_ADD_32(t,v) InterlockedExchangeAdd((LONG volatile*)(t),(LONG)(v))

#if (WINVER >= 0x0502)
/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x) InterlockedCompareExchange64((LONGLONG volatile*)(t),(LONGLONG)(x),(LONGLONG)(c))

/* Define if you have atomic exchange for 64bit values */
#define ATOMIC_EXCH_64(t,v) InterlockedExchange64((LONGLONG volatile*)(t),(LONGLONG)(v))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t) InterlockedIncrement64((LONGLONG volatile*)(t))
#define ATOMIC_DEC_64(t) InterlockedDecrement64((LONGLONG volatile*)(t))
#define ATOMIC_ADD_64(t,v) InterlockedExchangeAdd64((LONGLONG volatile*)(t),(LONGLONG)(v))

#endif // WINVER >= 0x0502

#endif // defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)

namespace OOBase
{
	namespace detail
	{
		template <typename T, const size_t S> 
		struct AtomicImpl;
	}
		
	template <typename T>
	class Atomic
	{
	public:
		Atomic()
		{}

		Atomic(const T& v) : m_val(v)
		{}

		Atomic(const Atomic& a) : m_val(a.m_val)
		{}

		~Atomic()
		{}

		Atomic& operator = (const Atomic& a)
		{
			if (&a != this)
				Exchange(a.m_val);
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

		static T Subtract(T& val, const T subtract)
		{
			return detail::AtomicImpl<T,sizeof(T)>::Subtract(val,subtract);
		}

		Atomic& operator -= (T val)
		{
			Subtract(m_val,val);
			return *this;
		}

		Atomic& operator += (const Atomic& rhs)
		{
			return *this += rhs.m_val;
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
		template <typename T>
		struct AtomicImpl<T,4>
		{
		#if defined(ATOMIC_CAS_32)
			static T CompareAndSwap(T& val, const T oldVal, const T newVal)
			{
				return (T)ATOMIC_CAS_32(&val,oldVal,newVal);
			}
		#endif

			static T Exchange(T& val, const T newVal)
			{
			#if defined (ATOMIC_EXCH_32)
				return (T)ATOMIC_EXCH_32(&val,newVal);
			#else
				T oldVal(val);
				while (CompareAndSwap(val,oldVal,newVal)) != oldVal)
					oldVal = val;
			
				return oldVal;
			#endif
			}

		#if defined(ATOMIC_ADD_32)
			static T Add(T& val, const T add)
			{
				return (T)ATOMIC_ADD_32(&val,add);
			}
		#endif

			static T Increment(T& val)
			{
			#if defined(ATOMIC_INC_32)
				return (T)ATOMIC_INC_32(&val);
			#else
				return Add(val,1);
			#endif
			}

			static T Subtract(T& val, const T sub)
			{
			#if defined(ATOMIC_SUB_32)
				return (T)ATOMIC_SUB_32(&val,sub);
			#else
				return Add(val,-sub);
			#endif
			}

			static T Decrement(T& val)
			{
			#if defined(ATOMIC_DEC_32)
				return (T)ATOMIC_DEC_32(&val);
			#else
				return Subtract(val,1);
			#endif
			}
		};

		template <typename T>
		struct AtomicImpl<T,8>
		{
		#if defined(ATOMIC_CAS_64)
			static T CompareAndSwap(T& val, const T oldVal, const T newVal)
			{
				return (T)ATOMIC_CAS_64(&val,oldVal,newVal);
			}
		#endif

			static T Exchange(T& val, const T newVal)
			{
			#if defined (ATOMIC_EXCH_64)
				return (T)ATOMIC_EXCH_64(&val,newVal);
			#else
				T oldVal(val);
				while (CompareAndSwap(val,oldVal,newVal) != oldVal)
					oldVal = val;
			
				return oldVal;
			#endif
			}

		#if defined(ATOMIC_ADD_64)
			static T Add(T& val, const T add)
			{
				return (T)ATOMIC_ADD_64(&val,add);
			}
		#endif

			static T Increment(T& val)
			{
			#if defined(ATOMIC_INC_64)
				return (T)ATOMIC_INC_64(&val);
			#else
				return Add(val,1);
			#endif
			}

			static T Subtract(T& val, const T sub)
			{
			#if defined(ATOMIC_SUB_64)
				return (T)ATOMIC_SUB_64(&val,sub);
			#else
				return Add(val,-sub);
			#endif
			}

			static T Decrement(T& val)
			{
			#if defined(ATOMIC_DEC_64)
				return (T)ATOMIC_DEC_64(&val);
			#else
				return Subtract(val,1);
			#endif
			}
		};
	}
}

#endif // OOBASE_ATOMIC_H_INCLUDED_
