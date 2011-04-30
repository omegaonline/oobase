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

#include "Memory.h"
#include "Mutex.h"
#include "Once.h"
#include "SmartPtr.h"
#include "Stack.h"

namespace OOBase
{
	namespace Once
	{
		void Run_Internal(once_t* key, pfn_once fn);
	}

	template <typename DLL>
	class DLLDestructor
	{
	public:
		typedef void (*pfn_destructor)(void*);

		static void add_destructor(pfn_destructor pfn, void* p)
		{
			DLLDestructor& inst = instance();
			Guard<SpinLock> guard(inst.m_lock);
			
			int err = inst.m_list.push(Node(pfn,p));
			if (err != 0)
				OOBase_CallCriticalFailure(err);
		}

		static void remove_destructor(pfn_destructor pfn, void* p)
		{
			DLLDestructor& inst = instance();
			Guard<SpinLock> guard(inst.m_lock);
			inst.m_list.erase(Node(pfn,p));
		}

	private:
		DLLDestructor() {}
		DLLDestructor(const DLLDestructor&);
		DLLDestructor& operator = (const DLLDestructor&);

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

		SpinLock                 m_lock;
		Stack<Node,CrtAllocator> m_list;

		~DLLDestructor()
		{
			Guard<SpinLock> guard(m_lock);

			Node node(NULL,NULL);
			while (m_list.pop(&node))
			{
				guard.release();

				try
				{
					(*node.m_pfn)(node.m_param);
				}
				catch (...)
				{}

				guard.acquire();
			}
		}

		static DLLDestructor& instance()
		{
			if (!s_instance)
			{
				static Once::once_t key = ONCE_T_INIT;
				Once::Run_Internal(&key,&init);
			}

			return *const_cast<DLLDestructor*>(s_instance);
		}

		static void init()
		{
			static DLLDestructor inst;
			s_instance = &inst;
		}

		static volatile DLLDestructor* s_instance;
	};

	template <typename DLL>
	volatile DLLDestructor<DLL>* DLLDestructor<DLL>::s_instance = NULL;
}

#endif // OOBASE_DESTRUCTOR_H_INCLUDED_
