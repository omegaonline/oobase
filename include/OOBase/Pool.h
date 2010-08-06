///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#ifndef OOBASE_POOL_H_INCLUDED_
#define OOBASE_POOL_H_INCLUDED_

#include "../config-base.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4345)
#endif

namespace OOBase
{
	template <typename T, size_t MAX>
	class Pool
	{
	public:
		Pool() : m_next(0)
		{
			for (size_t i=0;i<MAX;++i)
				m_pool[i].m_node_idx = i+1;
		}

		T* alloc()
		{
			if (m_next >= MAX)
			{
				T* ret = 0;
				OOBASE_NEW(ret,T());
				return ret;
			}
			else
			{
				Node& node = m_pool[m_next];
				m_next = node.m_node_idx;
				return new (node.m_placement) T();
			}
		}

		void free(T* ptr)
		{
			ptrdiff_t idx = (reinterpret_cast<Node*>(ptr) - m_pool);
			if (idx < 0 || static_cast<size_t>(idx) >= MAX)
				delete ptr;
			else
			{
				ptr->~T();
				m_pool[idx].m_node_idx = m_next;
				m_next = static_cast<size_t>(idx);
			}
		}

	private:
		Pool(const Pool&);
		Pool& operator = (const Pool&);

		union Node
		{
			char m_placement[sizeof(T)];
			size_t m_node_idx;
		};

		size_t            m_next;
		Node              m_pool[MAX];
	};
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // OOBASE_POOL_H_INCLUDED_
