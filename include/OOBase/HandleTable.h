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

#ifndef OOBASE_HANDLETABLE_H_INCLUDED_
#define OOBASE_HANDLETABLE_H_INCLUDED_

#include "HashTable.h"

namespace OOBase
{
	namespace detail
	{
		// Numerically increasing values do not need a complex hash
		template <typename T>
		struct HandleHash
		{
			static size_t hash(T v)
			{
				return v;
			}
		};
	}

	template <typename ID, typename V, typename Allocator = CrtAllocator>
	class HandleTable : public HashTable<ID,V,Allocator,detail::HandleHash<ID> >
	{
		typedef HashTable<ID,V,Allocator,detail::HandleHash<ID> > baseClass;

	public:
		HandleTable() : baseClass(),
				m_next(0)
		{}

		HandleTable(AllocatorInstance& allocator) : baseClass(allocator),
				m_next(0)
		{}

		~HandleTable()
		{}

		template <typename V1>
		int force_insert(ID id, V1 value)
		{
			return baseClass::insert(id,value);
		}

		template <typename V1>
		int insert(V1 value, ID& handle, ID min = 1, ID max = ID(-1))
		{
			return insert_i(value,handle,min,max,false);
		}

		template <typename V1>
		typename baseClass::iterator insert(V1 value, int& err, ID min = 1, ID max = ID(-1))
		{
			ID handle = 0;
			err = insert_i(value,handle,min,max,false);
			if (err)
				return baseClass::end();

			return baseClass::iterator(this,handle);
		}

		template <typename V1>
		int replace(ID handle, V1 value, ID min = 1, ID max = ID(-1))
		{
			return insert_i(value,handle,min,max,true);
		}

	private:
		ID m_next;

		template <typename V1>
		int insert_i(V1 value, ID& handle, ID min, ID max, bool replace)
		{
			ID next = m_next;
			if (next < min)
				next = min;

			for (int twice = 0; twice < 2; ++twice)
			{
				for (handle = next+1; handle != next;)
				{
					if (handle++ == max)
						handle = min;

					ID pos = handle & (this->m_size-1);
					if (this->m_data[pos].m_in_use != 2)
					{
						baseClass::Node::inplace_copy(&this->m_data[pos],handle,value);
						++this->m_count;
						m_next = handle;
						return 0;
					}

					if (this->m_data[pos].m_data.key == handle)
					{
						if (!replace)
							return EEXIST;

						baseClass::Node::inplace_copy(&this->m_data[pos],handle,value);
						m_next = handle;
						return 0;
					}
				}

				int err = baseClass::clone();
				if (err)
					return err;
			}
			return ERANGE;
		}
	};
}

#endif // OOBASE_HANDLETABLE_H_INCLUDED_
