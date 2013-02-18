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
		int Set(const void* key, void* val, void (*destructor)(void*) = NULL);
		
		void ThreadExit();

		namespace detail
		{
			AllocatorInstance* swap_allocator(AllocatorInstance* pNew = NULL);
		}
	}

	template <typename T, typename DLL>
	class TLSSingleton
	{
	public:
		static T* instance()
		{
			void* inst = NULL;
			if (!TLS::Get(&s_sentinal,&inst))
				inst = init();

			return static_cast<Instance*>(inst)->m_this;
		}

	private:
		// Prevent creation
		TLSSingleton();
		TLSSingleton(const TLSSingleton&);
		TLSSingleton& operator = (const TLSSingleton&);
		~TLSSingleton();

		static size_t s_sentinal;

		struct Instance
		{
			T*                 m_this;
			AllocatorInstance* m_allocator;
		};

		static void* init()
		{
			AllocatorInstance* alloc = TLS::detail::swap_allocator();

			Instance* i = alloc->allocate_new<Instance>();
			if (!i)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

			void* t = alloc->allocate(sizeof(T),alignof<T>::value);
			if (!t)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				i->m_this = ::new (t) T();
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				alloc->free(t);
				throw;
			}
#endif
			i->m_allocator = alloc;
									
			int err = TLS::Set(&s_sentinal,i,&destroy);
			if (err != 0)
				OOBase_CallCriticalFailure(err);

			return i;
		}

		static void destroy(void* p)
		{
			Instance* i = static_cast<Instance*>(p);
			if (i)
			{
				AllocatorInstance* curr = i->m_allocator;
				AllocatorInstance* prev = TLS::detail::swap_allocator(curr);

#if defined(OOBASE_HAVE_EXCEPTIONS)
				try
				{
#endif
					i->m_this->~T();
					curr->free(i->m_this);
#if defined(OOBASE_HAVE_EXCEPTIONS)
				}
				catch (...)
				{
					OOBase_CallCriticalFailure("Exception in TLSSingleton destructor");
				}
#endif
				i->m_this->~T();
				curr->free(i);
				TLS::detail::swap_allocator(prev);
			}
		}
	};

	template <typename T, typename DLL>
	size_t TLSSingleton<T,DLL>::s_sentinal = 1;
}

#endif // OOBASE_TLS_SINGLETON_H_INCLUDED_
