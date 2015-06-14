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

#ifndef OOBASE_TLS_SINGLETON_H_INCLUDED_
#define OOBASE_TLS_SINGLETON_H_INCLUDED_

#include "Memory.h"

namespace OOBase
{
	namespace TLS
	{
		bool Get(const void* key, void** val);
		bool Set(const void* key, void* val, void (*destructor)(void*) = NULL);
	}

	template <typename T, typename DLL = OOBase::Module>
	class TLSSingleton : public NonCopyable
	{
	public:
		static T* instance_ptr()
		{
			void* inst = NULL;
			if (!TLS::Get(&s_sentinal,&inst))
				inst = init();

			return static_cast<T*>(inst);
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
		TLSSingleton();
		~TLSSingleton();

		static size_t s_sentinal;

		static void* init()
		{
			// We do this long-hand so T can friend us
			void* t = ThreadLocalAllocator::allocate(sizeof(T),alignment_of<T>::value);
			if (!t)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

			// Add destructor before calling constructor
			if (!TLS::Set(&s_sentinal,t,&destroy))
			{
				ThreadLocalAllocator::free(t);
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
			}

#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				::new (t) T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				CrtAllocator::free(t);
				throw;
			}
#endif		
			return t;
		}

		static void destroy(void* p)
		{
			if (p)
				ThreadLocalAllocator::delete_free(static_cast<T*>(p));
		}
	};

	template <typename T, typename DLL>
	size_t TLSSingleton<T,DLL>::s_sentinal = 1;
}

#endif // OOBASE_TLS_SINGLETON_H_INCLUDED_
