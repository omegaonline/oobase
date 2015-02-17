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

#ifndef OOBASE_DESTRUCTOR_H_INCLUDED_
#define OOBASE_DESTRUCTOR_H_INCLUDED_

#include "Mutex.h"
#include "Once.h"
#include "List.h"

namespace OOBase
{
	template <typename DLL>
	class DLLDestructor : public NonCopyable
	{
	public:
		typedef void (*pfn_destructor)(void*);

		static bool add_destructor(pfn_destructor pfn, void* p)
		{
			DLLDestructor& inst = instance();
			Guard<SpinLock> guard(inst.m_lock);

			return inst.m_stack.push_back(Node(pfn,p)) != inst.m_stack.end();
		}

		static void remove_destructor(pfn_destructor pfn, void* p)
		{
			DLLDestructor& inst = instance();
			Guard<SpinLock> guard(inst.m_lock);

			inst.m_stack.remove(Node(pfn,p));
		}

	private:
		DLLDestructor() {}

		struct Node
		{
			Node(pfn_destructor pfn, void* p) : m_pfn(pfn), m_param(p)
			{}

			bool operator == (const Node& rhs) const
			{
				return (m_pfn == rhs.m_pfn && m_param == rhs.m_param);
			}

			pfn_destructor m_pfn;
			void*          m_param;
		};

		SpinLock                m_lock;
		List<Node,CrtAllocator> m_stack;

		~DLLDestructor()
		{
			Guard<SpinLock> guard(m_lock);

			for (Node node(NULL,NULL);m_stack.pop_back(&node);)
			{
				guard.release();

#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
					(*node.m_pfn)(node.m_param);
				}
				catch (...)
				{}
#else
				(*node.m_pfn)(node.m_param);
#endif
				guard.acquire();
			}
		}

		static DLLDestructor& instance()
		{
			static Once::once_t key = ONCE_T_INIT;
			Once::Run(&key,&init);

			return *s_instance;
		}

		static void init()
		{
			static DLLDestructor inst;
			s_instance = &inst;
		}

		static DLLDestructor* s_instance;
	};

	template <typename DLL>
	DLLDestructor<DLL>* DLLDestructor<DLL>::s_instance = NULL;
}

#endif // OOBASE_DESTRUCTOR_H_INCLUDED_
