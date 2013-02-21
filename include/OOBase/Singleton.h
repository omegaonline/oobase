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
			if (!i)
				OOBase_CallCriticalFailure("Null instance pointer");

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
			// We do this long-hand so singleton class can friend us
			void* t = OOBase::CrtAllocator::allocate(sizeof(T),alignof<T>::value);
			if (!t)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				::new (t) T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				OOBase::CrtAllocator::free(t);
				throw;
			}
#endif
			int err = DLLDestructor<DLL>::add_destructor(&destroy,t);
			if (err)
			{
				destroy(t);
				OOBase_CallCriticalFailure(err);
			}
			
			s_instance = static_cast<T*>(t);
		}

		static void destroy(void* i)
		{
			if (i == s_instance)
			{
				// We do this long-hand so singleton class can friend us
#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					static_cast<T*>(i)->~T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
				}
				catch (...)
				{
					OOBase_CallCriticalFailure("Exception in Singleton destructor");
				}
#endif
				OOBase::CrtAllocator::free(i);
				s_instance = NULL;
			}
		}
	};

	template <typename T, typename DLL>
	T* Singleton<T,DLL>::s_instance = NULL;
}

#endif // OOBASE_SINGLETON_H_INCLUDED_
