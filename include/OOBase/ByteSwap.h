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

#ifndef OOBASE_BYTE_SWAP_H_INCLUDED_
#define OOBASE_BYTE_SWAP_H_INCLUDED_

#include "Base.h"

#if defined(_MSC_VER) && defined(_W64)
// Turn off stupid 64-bit warnings..
#pragma warning(push)
#pragma warning(disable: 4311)
#pragma warning(disable: 4312)
#endif

////////////////////////////////////////
// Byte-order (endian-ness) determination.

/// if (OOBASE_BYTE_ORDER == OOBASE_LITTLE_ENDIAN) then the current machine is little-endian
#define OOBASE_LITTLE_ENDIAN 0x0123

/// if (OOBASE_BYTE_ORDER == OOBASE_BIG_ENDIAN) then the current machine is big-endian
#define OOBASE_BIG_ENDIAN 0x3210

#if defined(DOXYGEN)
/// Defines the current byte order
#define OOBASE_BYTE_ORDER

#elif defined(BYTE_ORDER)
#   if (BYTE_ORDER == LITTLE_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_LITTLE_ENDIAN
#   elif (BYTE_ORDER == BIG_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_BIG_ENDIAN
#   else
#     error: unknown BYTE_ORDER!
#   endif /* BYTE_ORDER */
# elif defined(_BYTE_ORDER)
#   if (_BYTE_ORDER == _LITTLE_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_LITTLE_ENDIAN
#   elif (_BYTE_ORDER == _BIG_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_BIG_ENDIAN
#   else
#     error: unknown _BYTE_ORDER!
#   endif /* _BYTE_ORDER */
# elif defined(__BYTE_ORDER)
#   if (__BYTE_ORDER == __LITTLE_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_LITTLE_ENDIAN
#   elif (__BYTE_ORDER == __BIG_ENDIAN)
#     define OOBASE_BYTE_ORDER OOBASE_BIG_ENDIAN
#   else
#     error: unknown __BYTE_ORDER!
#   endif /* __BYTE_ORDER */
# else /* ! BYTE_ORDER && ! __BYTE_ORDER */
  // We weren't explicitly told, so we have to figure it out . . .
#   if defined(i386) || defined(__i386__) || defined(_M_IX86) || \
     defined(vax) || defined(__alpha) || defined(__LITTLE_ENDIAN__) ||\
     defined(ARM) || defined(_M_IA64) || \
     defined(_M_AMD64) || defined(__amd64)
    // We know these are little endian.
#     define OOBASE_BYTE_ORDER OOBASE_LITTLE_ENDIAN
#   else
    // Otherwise, we assume big endian.
#     define OOBASE_BYTE_ORDER OOBASE_BIG_ENDIAN
#   endif
# endif /* ! BYTE_ORDER && ! __BYTE_ORDER */

namespace OOBase
{
	namespace detail
	{
		template <const size_t S>
		struct byte_swapper
		{
			template <typename T>
			static T byte_swap(T val);
		};
	}

	template <typename T>
	T byte_swap(T val)
	{
		static_assert(detail::is_pod<T>::value,"Attempting to byte swap non-POD");
		return detail::byte_swapper<sizeof(T)>::byte_swap(val);
	}

	namespace detail
	{
		uint16_t byte_swap_2(uint16_t v);
		uint32_t byte_swap_4(uint32_t v);
		uint64_t byte_swap_8(uint64_t v);

		template <>
		struct byte_swapper<1>
		{
			template <typename T>
			static T byte_swap(T val)
			{
				return val;
			}
		};

		template <>
		struct byte_swapper<2>
		{
			template <typename T>
			static T byte_swap(T val)
			{
				return (T)byte_swap_2((uint16_t)val);
			}
		};

		template <>
		struct byte_swapper<4>
		{
			template <typename T>
			static T byte_swap(T val)
			{
				return (T)byte_swap_4((uint32_t)val);
			}
		};

		template <>
		struct byte_swapper<8>
		{
			template <typename T>
			static T byte_swap(T val)
			{
				return (T)byte_swap_8((uint64_t)val);
			}
		};

		// This is purely for pointers to member functions
		template <>
		struct byte_swapper<16>
		{
			template <typename T>
			static T byte_swap(const T& val)
			{
				return val;
			}
		};
	}
}

#if defined(_MSC_VER) && defined(_W64)
#pragma warning(pop)
#endif

#endif // OOBASE_BYTE_SWAP_H_INCLUDED_
