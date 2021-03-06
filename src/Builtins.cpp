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

#include "config-base.h"

#include "../include/OOBase/Atomic.h"
#include "../include/OOBase/ByteSwap.h"

namespace OOBase
{
	namespace detail
	{
		unsigned int ffs(uint16_t v);
	}
}

#if defined(_MSC_VER)

#include <stdlib.h>

#define FAST_BYTESWAP_2(x) _byteswap_ushort(x)
#define FAST_BYTESWAP_4(x) _byteswap_ulong(x)
#define FAST_BYTESWAP_8(x) _byteswap_uint64(x)

#else

#if defined(HAVE___BUILTIN_BSWAP16)
#define FAST_BYTESWAP_2(x) __builtin_bswap16(x)
#endif

#if defined(HAVE___BUILTIN_BSWAP32)
#define FAST_BYTESWAP_4(x) __builtin_bswap32(x)
#endif

#if defined(HAVE___BUILTIN_BSWAP64)
#define FAST_BYTESWAP_8(x) __builtin_bswap64(x)
#endif

#endif

OOBase::uint16_t OOBase::detail::byte_swap_2(uint16_t val)
{
#if defined(FAST_BYTESWAP_2)
	return (FAST_BYTESWAP_2(val));
#else
	return (val & 0x00FF) << 8 | (val& 0xFF00) >> 8;
#endif
}

OOBase::uint32_t OOBase::detail::byte_swap_4(uint32_t val)
{
#if defined(FAST_BYTESWAP_4)
	return (FAST_BYTESWAP_4(val));
#else
	val = (val & 0x0000FFFF) << 16 | (val & 0xFFFF0000) >> 16;
	val = (val & 0x00FF00FF) << 8 | (val & 0xFF00FF00) >> 8;
	return val;
#endif
}

OOBase::uint64_t OOBase::detail::byte_swap_8(uint64_t val)
{
#if defined(FAST_BYTESWAP_8)
	return (FAST_BYTESWAP_8(val));
#else
	uint64_t hi = byte_swap_4(val);
	uint64_t lo = byte_swap_4(val >> 32);
	return (hi << 32) | lo;
#endif
}

unsigned int OOBase::detail::ffs(uint16_t v)
{
	assert(v != 0);

#if defined(HAVE___BUILTIN_FFS)
	return __builtin_ffs(v) - 1;
#elif defined(_MSC_VER)
	unsigned long i = 0;
	_BitScanForward(&i,v);
	return i;
#else
	unsigned int idx_mask = (v & (~v + 1));
	unsigned int i0 = (idx_mask & 0xAAAA) ?  1 : 0;
	unsigned int i1 = (idx_mask & 0xCCCC) ?  2 : 0;
	unsigned int i2 = (idx_mask & 0xF0F0) ?  4 : 0;
	unsigned int i3 = (idx_mask & 0xFF00) ?  8 : 0;
	return i3 | i2 | i1 | i0;
#endif
}

#if defined(_MSC_VER)

#pragma warning(push)
#pragma warning(disable : 6540)
#include <intrin.h>
#pragma warning(pop)

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

#if defined(_M_X64)
/* Define if you have atomic exchange for 64bit values */
#define ATOMIC_EXCH_64(t,v) _InterlockedExchange64((__int64 volatile*)(t),(__int64)(v))

/* Define if you have atomic compare-and-swap for 64bit values */
#define ATOMIC_INC_64(t) _InterlockedIncrement64((__int64 volatile*)(t))
#define ATOMIC_DEC_64(t) _InterlockedDecrement64((__int64 volatile*)(t))
#define ATOMIC_ADD_64(t,v) _InterlockedExchangeAdd64((__int64 volatile*)(t),(__int64)(v))
#endif

#elif defined(__clang__)

#if __has_extension(__sync_val_compare_and_swap)
#define ATOMIC_CAS_32(t,c,x) __sync_val_compare_and_swap((int volatile*)(t),(int)c,(int)x)
#define ATOMIC_CAS_64(t,c,x)  __sync_val_compare_and_swap((long volatile*)(t),(long)c,(long)x)
#endif

#if __has_extension(__sync_swap)
#define ATOMIC_EXCH_32(t,v) __sync_swap((int volatile*)(t),(int)v)
#define ATOMIC_EXCH_64(t,v) __sync_swap((long volatile*)(t),(long)v)
#endif

#if __has_extension(__sync_add_and_fetch)
#define ATOMIC_ADD_32(t,v) __sync_add_and_fetch((int volatile*)(t),(int)v)
#define ATOMIC_ADD_64(t,v) __sync_add_and_fetch((long volatile*)(t),(long)v)
#endif

#if __has_extension(__sync_synchronize)
#define ATOMIC_MEMORY_BARRIER() __sync_synchronize()
#endif

#endif

#if defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)
#if !defined(ATOMIC_CAS_32)
#define ATOMIC_CAS_32(t,c,x) __sync_val_compare_and_swap(t,c,x)
#endif
#if !defined(ATOMIC_CAS_64)
#define ATOMIC_CAS_64(t,c,x)  __sync_val_compare_and_swap(t,c,x)
#endif
#endif

#if defined(HAVE___SYNC_ADD_AND_FETCH)
#if !defined(ATOMIC_ADD_32)
#define ATOMIC_ADD_32(t,v) __sync_add_and_fetch(t,v)
#endif
#if !defined(ATOMIC_ADD_64)
#define ATOMIC_ADD_64(t,v) __sync_add_and_fetch(t,v)
#endif
#endif

#if defined(HAVE___SYNC_SYNCHRONIZE) && !defined(ATOMIC_MEMORY_BARRIER)
#define ATOMIC_MEMORY_BARRIER() __sync_synchronize()
#endif

#if defined(_WIN32)

/* Define if you have atomic memory barrier */
#if !defined(ATOMIC_MEMORY_BARRIER)
#define ATOMIC_MEMORY_BARRIER() MemoryBarrier();
#endif

#if !defined(ATOMIC_CAS_32)
#define ATOMIC_CAS_32(t,c,x) InterlockedCompareExchange((long volatile*)(t),(long)(x),(long)(c))
#endif

#if !defined(ATOMIC_EXCH_32)
#define ATOMIC_EXCH_32(t,v) InterlockedExchange((long volatile*)(t),(long)(v))
#endif

#if !defined(ATOMIC_INC_32)
#define ATOMIC_INC_32(t) InterlockedIncrement((long volatile*)(t))
#endif

#if !defined(ATOMIC_DEC_32)
#define ATOMIC_DEC_32(t) InterlockedDecrement((long volatile*)(t))
#endif

#if !defined(ATOMIC_ADD_32)
#define ATOMIC_ADD_32(t,v) InterlockedExchangeAdd((long volatile*)(t),(long)(v))
#endif

#if (_WIN32_WINNT >= 0x0600)
#if !defined(ATOMIC_CAS_64)
#define ATOMIC_CAS_64(t,c,x) InterlockedCompareExchange64((LONGLONG volatile*)(t),(LONGLONG)(x),(LONGLONG)(c))
#endif

#if !defined(ATOMIC_INC_64)
#define ATOMIC_INC_64(t) InterlockedIncrement64((__int64 volatile*)(t))
#endif

#if !defined(ATOMIC_DEC_64)
#define ATOMIC_DEC_64(t) InterlockedDecrement64((__int64 volatile*)(t))
#endif

#if !defined(ATOMIC_ADD_64)
#define ATOMIC_ADD_64(t,v) InterlockedExchangeAdd64((__int64 volatile*)(t),(__int64)(v))
#endif
#endif

#endif // defined(_WIN32)

void OOBase::detail::atomic_memory_barrier()
{
	ATOMIC_MEMORY_BARRIER();
}

OOBase::int32_t OOBase::detail::atomic_cas_4(int32_t volatile* val, const int32_t oldVal, const int32_t newVal)
{
#if defined(ATOMIC_CAS_32)
	return ATOMIC_CAS_32(val,oldVal,newVal);
#else
	#error define ATOMIC_CAS_32
#endif
}

OOBase::int32_t OOBase::detail::atomic_swap_4(int32_t volatile* val, const int32_t newVal)
{
#if defined (ATOMIC_EXCH_32)
	return ATOMIC_EXCH_32(val,newVal);
#else
	int32_t oldVal = *val;
	while (atomic_cas_4(val,oldVal,newVal) != oldVal)
		oldVal = *val;

	return oldVal;
#endif
}

OOBase::int32_t OOBase::detail::atomic_add_4(int32_t volatile* val, const int32_t add)
{
#if defined(ATOMIC_ADD_32)
	return ATOMIC_ADD_32(val,add);
#else
	int32_t oldVal = *val;
	int32_t newVal = *val + add;
	while (atomic_cas_4(val,oldVal,newVal) != oldVal)
	{
		oldVal = *val;
		newVal = *val + add;
	}
	return newVal;
#endif
}

OOBase::int32_t OOBase::detail::atomic_inc_4(int32_t volatile* val)
{
#if defined(ATOMIC_INC_32)
	return ATOMIC_INC_32(val);
#else
	return atomic_add_4(val,1);
#endif
}

OOBase::int32_t OOBase::detail::atomic_sub_4(int32_t volatile* val, const int32_t sub)
{
#if defined(ATOMIC_SUB_32)
	return ATOMIC_SUB_32(val,sub);
#else
	return atomic_add_4(val,-sub);
#endif
}

OOBase::int32_t OOBase::detail::atomic_dec_4(int32_t volatile* val)
{
#if defined(ATOMIC_DEC_32)
	return ATOMIC_DEC_32(val);
#else
	return atomic_sub_4(val,1);
#endif
}

#if defined (ATOMIC_CAS_64)
OOBase::int64_t OOBase::detail::atomic_cas_8(int64_t volatile* val, const int64_t oldVal, const int64_t newVal)
{
	return ATOMIC_CAS_64(val,oldVal,newVal);
}

OOBase::int64_t OOBase::detail::atomic_swap_8(int64_t volatile* val, const int64_t newVal)
{
#if defined (ATOMIC_EXCH_64)
	return ATOMIC_EXCH_64(val,newVal);
#else
	int64_t oldVal = *val;
	while (atomic_cas_8(val,oldVal,newVal) != oldVal)
		oldVal = *val;

	return oldVal;
#endif
}

OOBase::int64_t OOBase::detail::atomic_add_8(int64_t volatile* val, const int64_t add)
{
#if defined(ATOMIC_ADD_64)
	return ATOMIC_ADD_64(val,add);
#else
	int64_t oldVal = *val;
	int64_t newVal = *val + add;
	while (atomic_cas_8(val,oldVal,newVal) != oldVal)
	{
		oldVal = *val;
		newVal = *val + add;
	}
	return newVal;
#endif
}

OOBase::int64_t OOBase::detail::atomic_inc_8(int64_t volatile* val)
{
#if defined(ATOMIC_INC_64)
	return ATOMIC_INC_64(val);
#else
	return atomic_add_8(val,1);
#endif
}

OOBase::int64_t OOBase::detail::atomic_sub_8(int64_t volatile* val, const int64_t sub)
{
#if defined(ATOMIC_SUB_64)
	return ATOMIC_SUB_64(val,sub);
#else
	return atomic_add_8(val,-sub);
#endif
}

OOBase::int64_t OOBase::detail::atomic_dec_8(int64_t volatile* val)
{
#if defined(ATOMIC_DEC_64)
	return ATOMIC_DEC_64(val);
#else
	return atomic_sub_8(val,1);
#endif
}
#endif // ATOMIC_CAS_64
