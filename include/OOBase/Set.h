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

#include "Vector.h"

namespace OOBase
{
	template <typename T, typename Allocator = CrtAllocator>
	class Set : public detail::VectorImpl<T,Allocator>
	{
		typedef detail::VectorImpl<T,Allocator> baseClass;

	public:
		typedef T value_type;
		typedef Allocator allocator_type;
		typedef T& reference;
		typedef typename add_const<reference>::type const_reference;
		typedef T* pointer;
		typedef typename add_const<pointer>::type const_pointer;

		typedef detail::IteratorImpl<Set,value_type,size_t> iterator;
		friend class detail::IteratorImpl<Set,value_type,size_t>;
		typedef detail::IteratorImpl<const Set,const value_type,size_t> const_iterator;
		friend class detail::IteratorImpl<const Set,const value_type,size_t>;

		static const size_t npos = size_t(-1);

		Set() : baseClass(), m_sorted(true)
		{}

		Set(AllocatorInstance& allocator) : baseClass(allocator), m_sorted(true)
		{}

		template <typename T1>
		int insert(T1 value)
		{
			int err = baseClass::push_back(value);
			if (!err)
			{
				// Only clear sorted flag if we are sorted, and have inserted an
				// item that does not change the sort order
				if (m_sorted && this->m_size > 1 && !default_sort(*baseClass::at(this->m_size-2),value))
					m_sorted = false;
			}
			return err;
		}

		iterator erase(iterator iter)
		{
			assert(iter.check(this));
			return iterator(this,erase(iter.deref()));
		}

		template <typename T1>
		bool remove(T1 value)
		{
			iterator i = find(value);
			if (i == end())
				return false;

			erase(i);
			return true;
		}

		bool pop_back()
		{
			return baseClass::pop_back();
		}

		template <typename T1>
		bool exists(T1 value) const
		{
			return (find(value) != end());
		}

		template <typename T1>
		iterator find(T1 value, bool first = false)
		{
			const T* p = bsearch(value);

			// Scan for the first
			while (p && first && p > this->m_data && *(p-1) == *p)
				--p;

			return (p ? iterator(this,static_cast<size_t>(p - this->m_data)) : end());
		}

		template <typename T1>
		const_iterator find(T1 value, bool first = false) const
		{
			const T* p = bsearch(value);

			// Scan for the first
			while (p && first && p > this->m_data && *(p-1) == *p)
				--p;

			return (p ? const_iterator(this,static_cast<size_t>(p - this->m_data)) : end());
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

		iterator begin()
		{
			return iterator(this,0);
		}

		const_iterator cbegin() const
		{
			return const_iterator(this,0);
		}

		const_iterator begin() const
		{
			return cbegin();
		}

		iterator back()
		{
			return (this->m_size ? iterator(this,this->m_size-1) : end());
		}

		const_iterator back() const
		{
			return (this->m_size ? const_iterator(this,this->m_size-1) : end());
		}

		iterator end()
		{
			return iterator(this,size_t(-1));
		}

		const_iterator cend() const
		{
			return const_iterator(this,size_t(-1));
		}

		const_iterator end() const
		{
			return cend();
		}

	private:
		bool m_sorted;

		size_t erase(size_t pos)
		{
			if (m_sorted)
				return baseClass::erase(pos);

			if (this->m_data && pos < this->m_size)
			{
				this->m_data[pos] = this->m_data[--this->m_size];
				this->m_data[this->m_size].~T();
			}
			return pos < this->m_size ? pos : this->m_size;
		}

		template <typename T1>
		const T* bsearch(T1 key) const
		{
			// Always sort first
			const_cast<Set*>(this)->sort();

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

#endif // OOBASE_SET_H_INCLUDED_
