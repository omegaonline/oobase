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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Destructor.h"

const OOBase::critical_t OOBase::critical = {0};

#if !defined(_WIN32)

void* OOBase::CrtAllocator::allocate(size_t len)
{
	return ::malloc(len);
}

void* OOBase::CrtAllocator::reallocate(void* ptr, size_t len)
{
	return ::realloc(ptr,len);
}

bool OOBase::CrtAllocator::free(void* ptr)
{
	::free(ptr);
	return true;
}

#endif

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

namespace
{
	template <typename T>
	class MemoryManager
	{
	public:
		static void* allocate(size_t len)
		{
			Instance& i = instance();

			void* p = i.inst.allocate(len);
			if (p)
				++i.refcount;
			
			return p;
		}

		static void* reallocate(void* ptr, size_t len)
		{
			if (!ptr)
				return allocate(len);
			
			return instance().inst.reallocate(ptr,len);
		}

		static void free(void* ptr)
		{
			if (ptr)
			{
				Instance& i = instance();

				if (i.inst.free(ptr) && --i.refcount == 0)
					term();
			}
		}

	private:
		struct Instance
		{
			T                      inst;
			OOBase::Atomic<size_t> refcount;
		};

		static Instance* s_instance;

		static Instance& instance()
		{
			static OOBase::Once::once_t key = ONCE_T_INIT;
			OOBase::Once::Run(&key,&init);
			
			return *s_instance;
		}

		static void init()
		{
			// Use crt malloc here...
			s_instance = static_cast<Instance*>(OOBase::CrtAllocator::allocate(sizeof(Instance)));
			if (!s_instance)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

			// Placement new
			::new (s_instance) Instance();
			s_instance->refcount = 1;

			OOBase::DLLDestructor<OOBase::Module>::add_destructor(deref,NULL);
		}

		static void term()
		{
			s_instance->~Instance();
			OOBase::CrtAllocator::free(s_instance);
		}

		static void deref(void*)
		{
			s_instance->inst.check();

			if (--s_instance->refcount == 0)
				term();
		}
	};

	template <typename T>
	typename MemoryManager<T>::Instance* MemoryManager<T>::s_instance = NULL;
}


#if defined(_DEBUG)

#include "../include/OOBase/HashTable.h"

#include <stdio.h>

namespace
{
	// This class is temp!!
	class MemWatcher
	{
	public:
		void check()
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			if (!m_setEntries.empty())
				printf("****************** MEM LEAKS! *****************\n");
		}

		void* allocate(size_t len)
		{
			void* p = OOBase::CrtAllocator::allocate(len);
			if (p)
			{
				OOBase::Guard<OOBase::SpinLock> guard(m_lock);
				m_setEntries.insert(p,len);
			}

			return p;
		}

		void* reallocate(void* ptr, size_t len)
		{
			void* p = OOBase::CrtAllocator::reallocate(ptr,len);

			if (p != ptr)
			{
				OOBase::Guard<OOBase::SpinLock> guard(m_lock);

				bool e = m_setEntries.erase(ptr);
				assert(e);

				if (p)
					m_setEntries.insert(p,len);
			}

			return p;
		}

		bool free(void* ptr)
		{
			if (!ptr)
				return true;

			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			bool e = m_setEntries.erase(ptr);
			assert(e);

			guard.release();

			OOBase::CrtAllocator::free(ptr);

			return e;
		}

	private:
		OOBase::SpinLock   m_lock;
		OOBase::HashTable<void*,size_t,OOBase::CrtAllocator> m_setEntries;
	};
}

#endif // _DEBUG

void* OOBase::HeapAllocate(size_t bytes)
{
//#if defined(_DEBUG)
//	return MemoryManager<MemWatcher>::allocate(bytes);
//#else
	return MemoryManager<CrtAllocator>::allocate(bytes);
//#endif
}

void* OOBase::HeapReallocate(void* p, size_t bytes)
{
//#if defined(_DEBUG)
//	return MemoryManager<MemWatcher>::reallocate(p,bytes);
//#else
	return MemoryManager<CrtAllocator>::reallocate(p,bytes);
//#endif
}

void OOBase::HeapFree(void* p)
{
//#if defined(_DEBUG)
//	MemoryManager<MemWatcher>::free(p);
//#else
	MemoryManager<CrtAllocator>::free(p);
//#endif
}

void* OOBase::LocalAllocate(size_t bytes)
{
	return OOBase::HeapAllocate(bytes);
}

void* OOBase::LocalReallocate(void* p, size_t bytes)
{
	return OOBase::HeapReallocate(p,bytes);
}

void OOBase::LocalFree(void* p)
{
	return OOBase::HeapFree(p);
}
