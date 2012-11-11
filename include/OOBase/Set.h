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

#ifndef OOBASE_SET_H_INCLUDED_
#define OOBASE_SET_H_INCLUDED_

#include "Bag.h"

namespace OOBase
{
	namespace detail
	{
		template <typename T>
		class SetImpl : public BagImpl<T>
		{
			typedef BagImpl<T> baseClass;
		public:
			static const size_t npos = size_t(-1);

			SetImpl() : baseClass(), m_sorted(true)
			{}

			int insert(const T& value)
			{
				int err = baseClass::add(value);
				if (!err)
					m_sorted = false;
				return err;
			}

			void remove_at(size_t pos)
			{
				if (m_sorted)
					baseClass::remove_at_sorted(pos);
				else
					baseClass::remove_at(pos);
			}

			bool remove(const T& value)
			{
				size_t pos = find(value,false);
				if (pos == npos)
					return false;

				remove_at(pos);

				return true;
			}

			bool pop(T* value = NULL)
			{
				return baseClass::pop(value);
			}

			template <typename V1>
			bool exists(V1 key) const
			{
				return (find(key) != npos);
			}

			template <typename T1>
			size_t find(T1 key, bool first = false) const
			{
				const T* p = bsearch(key);

				// Scan for the first
				while (p && first && p > this->m_data && *(p-1) == *p)
					--p;

				return (p ? static_cast<size_t>(p - this->m_data) : npos);
			}

			const T* at(size_t pos) const
			{
				return baseClass::at(pos);
			}

			void sort()
			{
				if (!m_sorted)
					sort(&default_sort);
			}

			void sort(bool (*less_than)(const T& v1, const T& v2))
			{
				// This is a Shell-sort because we mostly use sort() in cases where qsort() behaves badly
				// i.e. insertion at the end and then sort
				// see http://warp.povusers.org/SortComparison

				// Generate the split intervals
				// Knuth is my homeboy :)
				size_t h = 1;
				while (h <= this->m_size / 9)
					h = 3*h + 1;

				for (;h > 0; h /= 3)
				{
					for (size_t i = h; i < this->m_size; ++i)
					{
						T v = this->m_data[i];
						size_t j = i;

						while (j >= h && (*less_than)(v,this->m_data[j-h]))
						{
							this->m_data[j] = this->m_data[j-h];
							j -= h;
						}

						this->m_data[j] = v;
					}
				}

				m_sorted = (less_than == &default_sort);
			}

		private:
			mutable bool m_sorted;

			template <typename T1>
			const T* bsearch(T1 key) const
			{
				// Always sort first
				const_cast<SetImpl*>(this)->sort();

				const T* base = this->m_data;
				for (size_t span = this->m_size; span > 0; span /= 2)
				{
					const T* mid_point = base + (span / 2);
					if (*mid_point == key)
						return mid_point;

					if (*mid_point < key)
					{
						base = mid_point + 1;
						--span;
					}
				}
				return NULL;
			}

			static bool default_sort(const T& v1, const T& v2)
			{
				return (v1 < v2);
			}
		};
	}

	template <typename T, typename Allocator = CrtAllocator>
	class Set : public detail::AllocImpl<detail::SetImpl<T>,Allocator>
	{
		typedef detail::AllocImpl<detail::SetImpl<T>,Allocator> baseClass;

	public:
		Set() : baseClass()
		{
		}

		Set(Allocator& allocator) : baseClass(allocator)
		{
		}

		virtual ~Set()
		{
			baseClass::destroy();
		}
	};
}

#endif // OOBASE_SET_H_INCLUDED_
