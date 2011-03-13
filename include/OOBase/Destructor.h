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
#include "SmartPtr.h"
#include "STLAllocator.h"

#include <list>

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
			inst.m_list.push_front(std::pair<pfn_destructor,void*>(pfn,p));
		}

		static void remove_destructor(pfn_destructor pfn, void* p)
		{
			DLLDestructor& inst = instance();
			Guard<SpinLock> guard(inst.m_lock);
			inst.m_list.remove(std::pair<pfn_destructor,void*>(pfn,p));
		}

	private:
		DLLDestructor() {}
		DLLDestructor(const DLLDestructor&);
		DLLDestructor& operator = (const DLLDestructor&);

		~DLLDestructor()
		{
			Guard<SpinLock> guard(m_lock);

			for (listType::iterator i=m_list.begin(); i!=m_list.end(); ++i)
			{
				try
				{
					(*(i->first))(i->second);
				}
				catch (...)
				{}
			}
		}

		SpinLock m_lock;

		typedef std::list<std::pair<pfn_destructor,void*>,STLAllocator<std::pair<pfn_destructor,void*>,HeapAllocator<CriticalFailure> > > listType;
		listType m_list;

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
	volatile DLLDestructor<DLL>* DLLDestructor<DLL>::s_instance = 0;
}

#endif // OOBASE_DESTRUCTOR_H_INCLUDED_
