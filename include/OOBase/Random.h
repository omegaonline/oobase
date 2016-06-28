///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2016 Rick Taylor
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

///////////////////////////////////////////////////////////////////////////////////
//
// Sections below are covered by the following license:

/*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#ifndef OOBASE_RANDOM_H_INCLUDED_
#define OOBASE_RANDOM_H_INCLUDED_

#include "Base.h"

namespace OOBase
{
	namespace detail
	{
		static inline uint64_t rotl(const uint64_t x, int k) 
		{
			return (x << k) | (x >> (64 - k));
		}

		struct splitmix64 : public NonCopyable
		{
			/* This is a fixed-increment version of Java 8's SplittableRandom generator
			   See http://dx.doi.org/10.1145/2714064.2660195 and 
			   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

			   It is a very fast generator passing BigCrush, and it can be useful if
			   for some reason you absolutely want 64 bits of state; otherwise, we
			   rather suggest to use a xoroshiro128+ (for moderately parallel
			   computations) or xorshift1024* (for massively parallel computations)
			   generator. */

			splitmix64(const uint64_t x) : m_x(x)
			{}

			uint64_t next() 
			{
				uint64_t z = (m_x += 0x9E3779B97F4A7C15ULL);
				z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
				z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
				return z ^ (z >> 31);
			}

		private:
			uint64_t m_x; /* The state can be seeded with any value. */
		};

		struct xoroshiro128plus : public NonCopyable
		{
			/* This is the successor to xorshift128+. It is the fastest full-period
			   generator passing BigCrush without systematic failures, but due to the
			   relatively short period it is acceptable only for applications with a
			   mild amount of parallelism; otherwise, use a xorshift1024* generator.

			   Beside passing BigCrush, this generator passes the PractRand test suite
			   up to (and included) 16TB, with the exception of binary rank tests,
			   which fail due to the lowest bit being an LFSR; all other bits pass all
			   tests. We suggest to use a sign test to extract a random Boolean value.
   
			   Note that the generator uses a simulated rotate operation, which most C
			   compilers will turn into a single instruction. In Java, you can use
			   Long.rotateLeft(). In languages that do not make low-level rotation
			   instructions accessible xorshift128+ could be faster.

			   The state must be seeded so that it is not everywhere zero. If you have
			   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
			   output to fill s. */

			xoroshiro128plus();

			xoroshiro128plus(const uint64_t s0, const uint64_t s1) :
				m_s0(s0), m_s1(s1)
			{}

			uint64_t next() 
			{
				const uint64_t result = m_s0 + m_s1;

				m_s1 ^= m_s0;
				m_s0 = rotl(m_s0, 55) ^ m_s1 ^ (m_s1 << 14); // a, b
				m_s1 = rotl(m_s1, 36); // c

				return result;
			}

			void swap(xoroshiro128plus& rhs)
			{
				OOBase::swap(m_s0,rhs.m_s0);
				OOBase::swap(m_s1,rhs.m_s1);
			}

		private:
			uint64_t m_s0;
			uint64_t m_s1;
		};
	}

	class Random : public NonCopyable
	{
	public:
		Random()
		{}

		template <typename T>
		Random(const T seed) : m_generator(static_cast<uint64_t>(seed))
		{}

		template <typename T>
		T next()
		{
			static_assert(sizeof(T) <= sizeof(uint64_t),"sizeof(T) <= sizeof(uint64_t)");
			// Use the high bits
			return static_cast<T>(m_generator.next() >> ((sizeof(uint64_t) - sizeof(T)) * 8));
		}

		bool next()
		{
			return static_cast<int64_t>(m_generator.next()) >= 0;
		}

		template <typename T>
		T next(const T max) // [0,max)
		{
			uint64_t r_max = uint64_t(-1) / max * max;
			uint64_t r;
			do {
				r = m_generator.next();
			} while (r >= r_max);
			return static_cast<T>(r % max);
		}

		template <typename T>
		T next(const T min, const T max) // [min,max)
		{
			return next(max-min) + min;
		}

	private:
		detail::xoroshiro128plus m_generator;
	};

	int random_bytes(void* buffer, size_t len);
	int random_chars(char* buffer, size_t len);
}

#endif // OOBASE_RANDOM_H_INCLUDED_
