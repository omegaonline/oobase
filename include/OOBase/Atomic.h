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

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t) _InterlockedIncrement((long volatile*)(t))
#define ATOMIC_DEC_32(t) _InterlockedDecrement((long volatile*)(t))
#define ATOMIC_ADD_32(t,v) _InterlockedExchangeAdd((long volatile*)(t),(long)(v))
#define ATOMIC_SUB_32(t,v) _InterlockedExchangeAdd((long volatile*)(t),-(long)(v))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x) _InterlockedCompareExchange64((__int64 volatile*)(t),(__int64)(x),(__int64)(c))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t) _InterlockedIncrement64((__int64 volatile*)(t))
#define ATOMIC_DEC_64(t) _InterlockedDecrement64((__int64 volatile*)(t))
#define ATOMIC_ADD_64(t,v) _InterlockedExchangeAdd64((__int64 volatile*)(t),(__int64)(v))
#define ATOMIC_SUB_64(t,v) _InterlockedExchangeAdd64((__int64 volatile*)(t),-(__int64)(v))

#elif defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,c,x) __sync_val_compare_and_swap((int volatile*)(t),c,x)

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x)  __sync_val_compare_and_swap((long long*)(t),c,x)

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t) __sync_add_and_fetch((int*)(t),1)
#define ATOMIC_DEC_32(t) __sync_sub_and_fetch((int*)(t),-1)
#define ATOMIC_ADD_32(t,v) __sync_add_and_fetch((int volatile*)(t),v)
#define ATOMIC_SUB_32(t,v) __sync_sub_and_fetch((int volatile*)(t),-v)

/* Define if you have atomic inc and dec for 64bit values */
#define ATOMIC_INC_64(t) __sync_add_and_fetch((long long volatile*)(t),1)
#define ATOMIC_DEC_64(t) __sync_sub_and_fetch((long long volatile*)(t),1)
#define ATOMIC_ADD_64(t,v) __sync_add_and_fetch((long long volatile*)(t),v)
#define ATOMIC_SUB_64(t,v) __sync_sub_and_fetch((long long volatile*)(t),v)

#elif defined(_WIN32)

/* Define if you have atomic compare-and-swap for 32bit values */
#define ATOMIC_CAS_32(t,c,x) InterlockedCompareExchange((long volatile*)(t),(long)(x),(long)(c))

/* Define if you have atomic inc and dec for 32bit values */
#define ATOMIC_INC_32(t) InterlockedIncrement((LONG volatile*)(t))
#define ATOMIC_DEC_32(t) InterlockedDecrement((LONG volatile*)(t))
#define ATOMIC_ADD_32(t,v) InterlockedExchangeAdd((LONG volatile*)(t),(LONG)(v))
#define ATOMIC_SUB_32(t,v) InterlockedExchangeAdd((LONG volatile*)(t),-(LONG)(v))

#if (WINVER >= 0x0502)
/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_CAS_64(t,c,x) InterlockedCompareExchange64((LONGLONG volatile*)(t),(LONGLONG)(x),(LONGLONG)(c))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t) InterlockedIncrement64((LONGLONG volatile*)(t))
#define ATOMIC_DEC_64(t) InterlockedDecrement64((LONGLONG volatile*)(t))
#define ATOMIC_ADD_64(t,v) InterlockedExchangeAdd64((LONGLONG volatile*)(t),(LONGLONG)(v))
#define ATOMIC_SUB_64(t,v) InterlockedExchangeAdd64((LONGLONG volatile*)(t),-(LONGLONG)(v))

#endif // WINVER >= 0x0502

#endif // defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)

namespace OOBase
{
	namespace detail
	{
		template <typename T, const size_t S> 
		struct CASImpl;

		template <typename T, const size_t S> 
		struct AtomicIncImpl;

		template <typename T>
		class AtomicValImpl_Raw
		{
		public:
			AtomicValImpl_Raw(T v) : m_val(v) {}
			AtomicValImpl_Raw(const AtomicValImpl_Raw& rhs) : m_val(rhs.value()) {}

			AtomicValImpl_Raw& operator = (const AtomicValImpl_Raw& rhs)
			{
				if (this != &rhs)
				{
					Guard<SpinLock> guard(m_lock);
					m_val = rhs.value();
				}

				return *this;
			}

			AtomicValImpl_Raw& operator = (T rhs)
			{
				Guard<SpinLock> guard(m_lock);
				m_val = rhs;
				return *this;
			}

			operator T () const
			{
				return value();
			}

		protected:
			AtomicValImpl_Raw() {}

			T value() const
			{
				Guard<SpinLock> guard(m_lock);
				return m_val;
			}

			mutable SpinLock m_lock;
			T                m_val;
		};

		template <typename T, const size_t S>
		class AtomicValImpl : public AtomicValImpl_Raw<T>
		{
		public:
			AtomicValImpl(T v) : AtomicValImpl_Raw<T>(v) {}
			AtomicValImpl(const AtomicValImpl& rhs) : AtomicValImpl_Raw<T>(rhs.value()) {}

			AtomicValImpl& operator = (const AtomicValImpl& rhs)
			{
				if (this != &rhs)
					AtomicValImpl_Raw<T>::operator = (rhs);

				return *this;
			}

			AtomicValImpl& operator = (T rhs)
			{
				AtomicValImpl_Raw<T>::operator = (rhs);
				return *this;
			}

		private:
			AtomicValImpl();
		};

		template <typename T, const size_t S>
		class AtomicIntImpl : public AtomicValImpl_Raw<T>
		{
		public:
			AtomicIntImpl() : AtomicValImpl_Raw<T>(T(0)) {}
			AtomicIntImpl(T v) : AtomicValImpl_Raw<T>(v) {}
			AtomicIntImpl(const AtomicIntImpl& rhs) : AtomicValImpl_Raw<T>(rhs.value()) {}

			AtomicIntImpl& operator = (const AtomicIntImpl& rhs)
			{
				if (this != &rhs)
					AtomicValImpl_Raw<T>::operator = (rhs);

				return *this;
			}

			AtomicIntImpl& operator = (T rhs)
			{
				AtomicValImpl_Raw<T>::operator = (rhs);
				return *this;
			}

			AtomicIntImpl& operator += (T v)
			{
				Guard<SpinLock> guard(this->m_lock);
				this->m_val += v;
				return *this;
			}

			AtomicIntImpl& operator -= (T v)
			{
				Guard<SpinLock> guard(this->m_lock);
				this->m_val -= v;
				return *this;
			}

			T operator ++()
			{
				Guard<SpinLock> guard(this->m_lock);
				return ++this->m_val;
			}

			T operator --()
			{
				Guard<SpinLock> guard(this->m_lock);
				return --this->m_val;
			}
		};
	}

	template <typename T>
	inline T CompareAndSwap(T& val, T cmp, T exch)
	{
		return detail::CASImpl<T,sizeof(T)>::CAS(val,cmp,exch);
	}

	template <typename T> 
	inline T AtomicIncrement(T& v)
	{
		return detail::AtomicIncImpl<T,sizeof(T)>::Inc(v);
	}

	template <typename T> 
	inline T AtomicDecrement(T& v)
	{
		return detail::AtomicIncImpl<T,sizeof(T)>::Dec(v);
	}

	template <typename T>
	class AtomicVal : public detail::AtomicValImpl<T,sizeof(T)>
	{
	public:
		AtomicVal(T v) : detail::AtomicValImpl<T,sizeof(T)>(v) {}
		AtomicVal(const AtomicVal& rhs) : detail::AtomicValImpl<T,sizeof(T)>(rhs.value()) {}

		AtomicVal& operator = (const AtomicVal& rhs)
		{
			if (this != &rhs)
				detail::AtomicValImpl<T,sizeof(T)>::operator = (rhs);

			return *this;
		}

		AtomicVal& operator = (T rhs)
		{
			detail::AtomicValImpl<T,sizeof(T)>::operator = (rhs);
			return *this;
		}

	private:
		AtomicVal();
	};

	template <typename T>
	class AtomicInt : public detail::AtomicIntImpl<T,sizeof(T)>
	{
	public:
		AtomicInt() : detail::AtomicIntImpl<T,sizeof(T)>() {}
		AtomicInt(T v) : detail::AtomicIntImpl<T,sizeof(T)>(v) {}
		AtomicInt(const AtomicVal<T>& rhs) : detail::AtomicIntImpl<T,sizeof(T)>(rhs.value()) {}
		AtomicInt(const AtomicInt& rhs) : detail::AtomicIntImpl<T,sizeof(T)>(rhs.value()) {}

		AtomicInt& operator = (const AtomicInt& rhs)
		{
			if (this != &rhs)
				detail::AtomicIntImpl<T,sizeof(T)>::operator = (rhs.value());

			return *this;
		}

		AtomicInt& operator = (T rhs)
		{
			detail::AtomicIntImpl<T,sizeof(T)>::operator = (rhs);
			return *this;
		}

		T operator ++()
		{
			return detail::AtomicIntImpl<T,sizeof(T)>::operator ++();
		}

		T operator --()
		{
			return detail::AtomicIntImpl<T,sizeof(T)>::operator --();
		}

		T operator ++(int)
		{
			return ++*this - 1;
		}

		T operator --(int)
		{
			return --*this + 1;
		}
	};

#if defined(ATOMIC_CAS_32)

	namespace detail
	{
		template <typename T>
		class AtomicValImpl<T,4>
		{
		public:
			AtomicValImpl(T v) : m_val(v) {}
			AtomicValImpl(const AtomicValImpl& rhs) : m_val(rhs.value()) {}

			AtomicValImpl& operator = (T v)
			{
				for (T v1 = m_val;(v1 = ATOMIC_CAS_32(&m_val,v1,v)) != v;)
					;

				return *this;
			}

			AtomicValImpl& operator = (const AtomicValImpl& rhs)
			{
				if (this != &rhs)
				{
					for (T v1 = m_val;(v1 = ATOMIC_CAS_32(&m_val,v1,rhs.m_val)) != rhs.m_val;)
						;
				}

				return *this;
			}

			operator T () const
			{
				return value();
			}

		protected:
			AtomicValImpl() {}

			T value() const
			{
				return m_val;
			}

			volatile T m_val;
		};

		template <typename T> 
		struct CASImpl<T,4>
		{
			static T CAS(T& v, T cmp, T exch)
			{
				return ATOMIC_CAS_32(&v,cmp,exch);
			}
		};
	}

#endif // ATOMIC_CAS_32

#if defined(ATOMIC_CAS_64)
	namespace detail
	{
		template <typename T>
		class AtomicValImpl<T,8>
		{
		public:
			AtomicValImpl(T v) : m_val(v) {}
			AtomicValImpl(const AtomicValImpl& rhs) : m_val(rhs.value()) {}

			AtomicValImpl& operator = (T v)
			{
				for (T v1 = m_val;(v1 = ATOMIC_CAS_64(&m_val,v1,v)) != v;)
					;

				return *this;
			}

			AtomicValImpl& operator = (const AtomicValImpl& rhs)
			{
				if (this != &rhs)
				{
					for (T v1 = m_val;(v1 = ATOMIC_CAS_64(&m_val,v1,rhs.m_val)) != rhs.m_val;)
						;
				}

				return *this;
			}

			operator T () const
			{
				return value();
			}

		protected:
			AtomicValImpl();

			T value() const
			{
				return m_val;
			}

			volatile T m_val;
		};

		template <typename T> 
		struct CASImpl<T,8>
		{
			static T CAS(T& v, T cmp, T exch)
			{
				return ATOMIC_CAS_64(&v,cmp,exch);
			}
		};
	}

#endif // ATOMIC_CAS_64

#if defined(ATOMIC_INC_32)
	namespace detail
	{
		template <typename T>
		class AtomicIntImpl<T,4> : public AtomicValImpl<T,4>
		{
		public:
			AtomicIntImpl() : AtomicValImpl<T,4>(0) {}
			AtomicIntImpl(T v) : AtomicValImpl<T,4>(v) {}
			AtomicIntImpl(const AtomicIntImpl& rhs) : AtomicValImpl<T,4>(rhs.value()) {}

			AtomicIntImpl& operator = (const AtomicIntImpl& rhs)
			{
				if (this != &rhs)
					AtomicValImpl<T,4>::operator = (rhs.value());

				return *this;
			}

			AtomicIntImpl& operator += (T v)
			{
				ATOMIC_ADD_32(&this->m_val,v);
				return *this;
			}

			AtomicIntImpl& operator -= (T v)
			{
				ATOMIC_SUB_32(&this->m_val,v);
				return *this;
			}

			T operator ++()
			{
				return ATOMIC_INC_32(&this->m_val);
			}

			T operator --()
			{
				return ATOMIC_DEC_32(&this->m_val);
			}
		};

		template <typename T> 
		struct AtomicIncImpl<T,4>
		{
			static T Inc(T& v)
			{
				return ATOMIC_INC_32(&v);
			}

			static T Dec(T& v)
			{
				return ATOMIC_DEC_32(&v);
			}
		};
	}
#endif // defined(ATOMIC_INC_32)

#if defined(ATOMIC_INC_64)
	namespace detail
	{
		template <typename T>
		class AtomicIntImpl<T,8> : public AtomicValImpl<T,8>
		{
		public:
			AtomicIntImpl() : AtomicValImpl<T,8>(0) {}
			AtomicIntImpl(T v) : AtomicValImpl<T,8>(v) {}
			AtomicIntImpl(const AtomicIntImpl& rhs) : AtomicValImpl<T,8>(rhs.value()) {}

			AtomicIntImpl& operator = (const AtomicIntImpl& rhs)
			{
				if (this != &rhs)
					AtomicValImpl<T,8>::operator = (rhs.value());

				return *this;
			}

			AtomicIntImpl& operator += (T v)
			{
				ATOMIC_ADD_64(&this->m_val,v);
				return *this;
			}

			AtomicIntImpl& operator -= (T v)
			{
				ATOMIC_SUB_64(&this->m_val,v);
				return *this;
			}

			T operator ++()
			{
				return ATOMIC_INC_64(&this->m_val);
			}

			T operator --()
			{
				return ATOMIC_DEC_64(&this->m_val);
			}
		};

		template <typename T> 
		struct AtomicIncImpl<T,8>
		{
			static T Inc(T& v)
			{
				return ATOMIC_INC_32(&v);
			}

			static T Dec(T& v)
			{
				return ATOMIC_DEC_32(&v);
			}
		};
	}
#endif // defined(ATOMIC_INC_64)
}

#endif // OOBASE_ATOMIC_H_INCLUDED_
