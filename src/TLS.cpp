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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/TLSSingleton.h"
#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/HashTable.h"
#include "../include/OOBase/ArenaAllocator.h"

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};

	namespace detail
	{
		char* get_error_buffer(size_t& len);
	}
}

namespace
{
	class TLSMap
	{
	public:
		TLSMap(OOBase::AllocatorInstance* allocator) : m_allocator(allocator) //, m_mapVals(*allocator)
		{
			void* TODO; // Make an AllocatorInstance version of HashTable for use here!
		}

		static TLSMap* instance(bool create = true);

		static void destroy(void* pThis);

		struct tls_val
		{
			void* m_val;
			void (*m_destructor)(void*);
		};

		OOBase::AllocatorInstance* m_allocator;

		//OOBase::HashTable<const void*,tls_val,OOBase::AllocatorInstance> m_mapVals;
		OOBase::HashTable<const void*,tls_val,OOBase::CrtAllocator> m_mapVals;

		// Special internal thread-local variables
		char m_error_buffer[512];

	private:
		TLSMap(const TLSMap&);
		TLSMap& operator = (const TLSMap&);
	};
}

#if defined(_WIN32)

namespace
{
	struct Win32TLSGlobal
	{
		Win32TLSGlobal();
		~Win32TLSGlobal();

		TLSMap* Get();
		void Set(TLSMap* inst);

	private:
		DWORD         m_key;
	};

	Win32TLSGlobal::Win32TLSGlobal() :
			m_key(TLS_OUT_OF_INDEXES)
	{
		m_key = TlsAlloc();
		if (m_key == TLS_OUT_OF_INDEXES)
			OOBase_CallCriticalFailure(GetLastError());

		if (!TlsSetValue(m_key,NULL))
			OOBase_CallCriticalFailure(GetLastError());
	}

	Win32TLSGlobal::~Win32TLSGlobal()
	{
		if (m_key != TLS_OUT_OF_INDEXES)
			TlsFree(m_key);
	}

	TLSMap* Win32TLSGlobal::Get()
	{
		return static_cast<TLSMap*>(TlsGetValue(m_key));
	}

	void Win32TLSGlobal::Set(TLSMap* inst)
	{
		if (!TlsSetValue(m_key,inst))
			OOBase_CallCriticalFailure(GetLastError());
	}

	/** \typedef TLS_GLOBAL
	 *  The TLS singleton.
	 *  The singleton is specialised by the platform specific implementation
	 *  e.g. \link Win32TLSGlobal \endlink or \link PthreadTLSGlobal \endlink
	 */
	typedef OOBase::Singleton<Win32TLSGlobal,OOBase::Module> TLS_GLOBAL;
}

template class OOBase::Singleton<Win32TLSGlobal,OOBase::Module>;

#endif // _WIN32

#if defined(HAVE_PTHREAD)

namespace
{
	struct PthreadTLSGlobal
	{
		PthreadTLSGlobal();
		~PthreadTLSGlobal();

		TLSMap* Get();
		void Set(TLSMap* inst);

	private:
		pthread_key_t m_key;

		static void thread_destruct(void*);
	};

	PthreadTLSGlobal::PthreadTLSGlobal()
	{
		int err = pthread_key_create(&m_key,&thread_destruct);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

	PthreadTLSGlobal::~PthreadTLSGlobal()
	{
		int err = pthread_key_delete(m_key);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

	TLSMap* PthreadTLSGlobal::Get()
	{
		return static_cast<TLSMap*>(pthread_getspecific(m_key));
	}

	void PthreadTLSGlobal::Set(TLSMap* inst)
	{
		int err = pthread_setspecific(m_key,inst);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

	void PthreadTLSGlobal::thread_destruct(void* inst)
	{
		OOBase::DLLDestructor<OOBase::Module>::remove_destructor(TLSMap::destroy,inst);
		TLSMap::destroy(inst);
	}

	typedef OOBase::Singleton<PthreadTLSGlobal,OOBase::Module> TLS_GLOBAL;
}

template class OOBase::Singleton<PthreadTLSGlobal,OOBase::Module>;

#endif // HAVE_PTHREAD

namespace
{
	TLSMap* TLSMap::instance(bool create)
	{
		TLSMap* inst = TLS_GLOBAL::instance().Get();
		if (!inst && create)
		{
			OOBase::AllocatorInstance* alloc = new (OOBase::critical) OOBase::ArenaAllocator();

			void* p = alloc->allocate(sizeof(TLSMap),OOBase::alignof<TLSMap>::value);
			if (!p)
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);

#if defined(OOBASE_HAVE_EXCEPTIONS)
			try
			{
#endif
				inst = ::new (p) TLSMap(alloc);
#if defined(OOBASE_HAVE_EXCEPTIONS)
			}
			catch (...)
			{
				alloc->free(p);
				throw;
			}
#endif
			if (!inst)
			{
				delete alloc;
				OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
			}

			int err = OOBase::DLLDestructor<OOBase::Module>::add_destructor(destroy,inst);
			if (err != 0)
			{
				destroy(inst);
				OOBase_CallCriticalFailure(err);
			}

			TLS_GLOBAL::instance().Set(inst);
		}
		return inst;
	}

	void TLSMap::destroy(void* pThis)
	{
		TLSMap* inst = static_cast<TLSMap*>(pThis);
		if (inst)
		{
			tls_val val = {0};
			while (inst->m_mapVals.pop(NULL,&val))
			{
				if (val.m_destructor)
					(*val.m_destructor)(val.m_val);
			}

			OOBase::AllocatorInstance* alloc = inst->m_allocator;

			inst->~TLSMap();
			alloc->free(inst);

			delete alloc;
		}

		// Now set NULL back in place...
		TLS_GLOBAL::instance().Set(NULL);
	}
}

bool OOBase::TLS::Get(const void* key, void** val)
{
	TLSMap* inst = TLSMap::instance();

	TLSMap::tls_val v = {0};
	if (!inst->m_mapVals.find(key,v))
		return false;

	*val = v.m_val;
	return true;
}

int OOBase::TLS::Set(const void* key, void* val, void (*destructor)(void*))
{
	TLSMap* inst = TLSMap::instance();

	TLSMap::tls_val v;
	v.m_val = val;
	v.m_destructor = destructor;

	TLSMap::tls_val* pv = inst->m_mapVals.find(key);
	if (!pv)
		return inst->m_mapVals.insert(key,v);

	*pv = v;
	return true;
}

void OOBase::TLS::ThreadExit()
{
	TLSMap* inst = TLSMap::instance(false);
	if (inst)
	{
		OOBase::DLLDestructor<OOBase::Module>::remove_destructor(TLSMap::destroy,inst);
		TLSMap::destroy(inst);
	}
}

OOBase::AllocatorInstance* OOBase::TLS::detail::swap_allocator(AllocatorInstance* pNew)
{
	TLSMap* inst = TLSMap::instance();

	AllocatorInstance* curr = inst->m_allocator;
	if (pNew)
		inst->m_allocator = pNew;

	return curr;
}

char* OOBase::detail::get_error_buffer(size_t& len)
{
	TLSMap* inst = TLSMap::instance(false);
	if (!inst)
		return NULL;

	len = sizeof(inst->m_error_buffer);
	return inst->m_error_buffer;
}

void* OOBase::ThreadLocalAllocator::allocate(size_t bytes, size_t align)
{
	return TLS::detail::swap_allocator()->allocate(bytes,align);
}

void* OOBase::ThreadLocalAllocator::reallocate(void* ptr, size_t bytes, size_t align)
{
	return TLS::detail::swap_allocator()->reallocate(ptr,bytes,align);
}

void OOBase::ThreadLocalAllocator::free(void* ptr)
{
	TLS::detail::swap_allocator()->free(ptr);
}
