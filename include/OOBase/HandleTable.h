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
		HandleTable(ID start = 1) : baseClass(),
				m_next(start)
		{}

		HandleTable(Allocator& allocator, ID start = 1) : baseClass(allocator),
				m_next(start)
		{}
			
		~HandleTable()
		{}

		int force_insert(ID id, const V& value)
		{
			return baseClass::insert(id,value);
		}
			
		int insert(const V& value, ID& handle, ID min = 1, ID max = ID(-1))
		{
			ID next = m_next;
			if (next < min)
				next = min;
			
			for (ID i = 0;;++i)
			{
				if (next > max)
					next = min;
				
				handle = next++;
				if (!baseClass::exists(handle))
					break;
				
				// Check we don't loop endlessly
				if (i == max-min)
					return ERANGE;
			}			
						
			int err = baseClass::insert(handle,value);
			if (err == 0)
				m_next = next;
			
			return err;
		}
		
	private:
		ID m_next;
	};
}

#endif // OOBASE_HANDLETABLE_H_INCLUDED_
