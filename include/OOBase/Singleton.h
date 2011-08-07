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

#ifndef OOBASE_SINGLETON_H_INCLUDED_
#define OOBASE_SINGLETON_H_INCLUDED_

#include "Memory.h"
#include "Destructor.h"

namespace OOBase
{
	template <typename T, typename DLL>
	class Singleton
	{
	public:
		static T* instance_ptr()
		{
			static Once::once_t key = ONCE_T_INIT;
			Once::Run(&key,init);

			return s_instance;
		}

		static T& instance()
		{
			T* i = instance_ptr();
			assert(i);
			return *i;
		}

	private:
		// Prevent creation
		Singleton();
		Singleton(const Singleton&);
		Singleton& operator = (const Singleton&);
		~Singleton();

		static T* s_instance;

		static void init()
		{
			void* p = OOBase::HeapAllocate(sizeof(T));
			if (!p)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
			
			s_instance = ::new (p) T();
			
			DLLDestructor<DLL>::add_destructor(&destroy,NULL);
		}

		static void destroy(void*)
		{
			T* p = s_instance;
			s_instance = NULL;
			
			if (p)
			{
				p->~T();
				OOBase::HeapFree(p);
			}
		}
	};

	template <typename T, typename DLL>
	T* Singleton<T,DLL>::s_instance;
}

#endif // OOBASE_SINGLETON_H_INCLUDED_
