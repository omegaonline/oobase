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

#include "../include/OOBase/TLSSingleton.h"
#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/HashTable.h"
#include "../include/OOBase/ArenaAllocator.h"

namespace OOBase
{
	namespace detail
	{
		char* get_error_buffer(size_t& len);
	}
}

namespace
{
	class TLSGlobal : public OOBase::NonCopyable
	{
	public:
		TLSGlobal() : m_mapVals(m_allocator), m_listDestructors(m_allocator), m_refcount(1)
		{}

		~TLSGlobal();

		void addref() 
		{
			++m_refcount; 
		}

		void release();

		static TLSGlobal* instance(bool create);
		
		OOBase::ArenaAllocator m_allocator;

		OOBase::HashTable<const void*,void*,OOBase::AllocatorInstance> m_mapVals;
		struct tls_val
		{
			const void* m_key;
			void (*m_destructor)(void*);
		};
		OOBase::List<tls_val,OOBase::AllocatorInstance> m_listDestructors;
		
		// Special internal thread-local variables
		char m_error_buffer[512];

	private:
		size_t m_refcount;
	};
}

#if defined(_WIN32)

namespace
{
	static DWORD s_key = TLS_OUT_OF_INDEXES;

	void init()
	{
		s_key = TlsAlloc();
		if (s_key == TLS_OUT_OF_INDEXES)
			OOBase_CallCriticalFailure(GetLastError());
	}

	void term(void* inst)
	{
		static_cast<TLSGlobal*>(inst)->release();
	}
}

void WINAPI on_tls_callback(HANDLE, DWORD dwReason, PVOID)
{
	if (dwReason == DLL_THREAD_DETACH || dwReason == DLL_PROCESS_DETACH)
	{
		TLSGlobal* inst = static_cast<TLSGlobal*>(TlsGetValue(s_key));
		if (inst)
		{
			OOBase::DLLDestructor<OOBase::Module>::remove_destructor(&term,inst);
			inst->release();
		}
	}
}

#if (__MINGW32_MAJOR_VERSION >3) || ((__MINGW32_MAJOR_VERSION==3) && (__MINGW32_MINOR_VERSION>=18))
extern "C"
{
	PIMAGE_TLS_CALLBACK _xl_b __attribute__ ((section(".CRT$XLB"))) = &on_tls_callback;
}
#elif defined(_MSC_VER)
#ifdef _WIN64
#pragma comment (linker, "/INCLUDE:_tls_used")
#pragma comment (linker, "/INCLUDE:_xl_b")
#else
#pragma comment (linker, "/INCLUDE:__tls_used")
#pragma comment (linker, "/INCLUDE:__xl_b")
#endif
extern "C"
{
#ifdef _WIN64
#pragma const_seg (".CRT$XLB")
extern const PIMAGE_TLS_CALLBACK _xl_b;
const PIMAGE_TLS_CALLBACK _xl_b = &on_tls_callback;
#pragma const_seg ()
#else
#pragma data_seg (".CRT$XLB")
PIMAGE_TLS_CALLBACK _xl_b = &on_tls_callback;
#pragma data_seg ()
#endif
}
#endif

TLSGlobal* TLSGlobal::instance(bool create)
{
	static OOBase::Once::once_t key = ONCE_T_INIT;
	OOBase::Once::Run(&key,init);
		
	TLSGlobal* inst = NULL;
	if (s_key != TLS_OUT_OF_INDEXES)
	{
		inst = static_cast<TLSGlobal*>(TlsGetValue(s_key));
		if (!inst && create)
		{
			inst = OOBase::CrtAllocator::allocate_new<TLSGlobal>();
			if (inst)
			{
				OOBase::DLLDestructor<OOBase::Module>::add_destructor(&term,inst);
				TlsSetValue(s_key,inst);
			}
		}
	}
	return inst;
}

void TLSGlobal::release()
{
	if (--m_refcount == 0)
	{
		OOBase::CrtAllocator::delete_free(this);
		TlsSetValue(s_key,NULL);
	}
}

#elif defined(HAVE_PTHREAD)

namespace
{
	static pthread_key_t s_key;

	void term(void* inst)
	{
		static_cast<TLSGlobal*>(inst)->release();
		pthread_setspecific(s_key,NULL);
	}

	void thread_destruct(void* inst)
	{
		if (inst)
		{
			OOBase::DLLDestructor<OOBase::Module>::remove_destructor(&term,inst);
			static_cast<TLSGlobal*>(inst)->release();
		}
	}

	void init()
	{
		pthread_key_create(&s_key,&thread_destruct);
	}
}

TLSGlobal* TLSGlobal::instance(bool create)
{
	static OOBase::Once::once_t key = ONCE_T_INIT;
	OOBase::Once::Run(&key,init);
		
	TLSGlobal* inst = static_cast<TLSGlobal*>(pthread_getspecific(s_key));
	if (!inst && create)
	{
		inst = OOBase::CrtAllocator::allocate_new<TLSGlobal>();
		if (inst)
		{
			pthread_setspecific(s_key,inst);
			OOBase::DLLDestructor<OOBase::Module>::add_destructor(&term,inst);
		}
	}
		
	return inst;
}

void TLSGlobal::release()
{
	if (--m_refcount == 0)
		OOBase::CrtAllocator::delete_free(this);
}

#endif

TLSGlobal::~TLSGlobal()
{
	for (tls_val val;m_listDestructors.pop_back(&val);)
	{
		OOBase::HashTable<const void*,void*,OOBase::AllocatorInstance>::iterator i = m_mapVals.find(val.m_key);
		if (i)
			(*val.m_destructor)(i->second);
	}
}

bool OOBase::TLS::Get(const void* key, void** val)
{
	TLSGlobal* inst = TLSGlobal::instance(false);
	if (!inst)
		return false;

	OOBase::HashTable<const void*,void*,OOBase::AllocatorInstance>::iterator i = inst->m_mapVals.find(key);
	if (!i)
		return false;

	if (val)
		*val = i->second;

	return true;
}

bool OOBase::TLS::Set(const void* key, void* val, void (*destructor)(void*))
{
	TLSGlobal* inst = TLSGlobal::instance(true);
	if (!inst)
		return false;

	OOBase::HashTable<const void*,void*,OOBase::AllocatorInstance>::iterator i = inst->m_mapVals.find(key);
	if (i)
	{
		if (destructor)
		{
			for (OOBase::List<TLSGlobal::tls_val,OOBase::AllocatorInstance>::iterator j=inst->m_listDestructors.begin();j;++j)
			{
				if (j->m_key == key)
				{
					j->m_destructor = destructor;
					break;
				}
			}
		}

		i->second = val;
		return true;
	}
	
	if (destructor)
	{
		TLSGlobal::tls_val v;
		v.m_key = key;
		v.m_destructor = destructor;

		if (!inst->m_listDestructors.push_back(v))
			return false;
	}

	return inst->m_mapVals.insert(key,val);
}

char* OOBase::detail::get_error_buffer(size_t& len)
{
	TLSGlobal* inst = TLSGlobal::instance(false);
	if (!inst)
		return NULL;

	len = sizeof(inst->m_error_buffer);
	return inst->m_error_buffer;
}

void* OOBase::ThreadLocalAllocator::allocate(size_t bytes, size_t align)
{
	TLSGlobal* inst = TLSGlobal::instance(true);
	if (!inst)
		return NULL;

	void* p = inst->m_allocator.allocate(bytes,align);
	if (p)
		inst->addref();
	return p;
}

void* OOBase::ThreadLocalAllocator::reallocate(void* ptr, size_t bytes, size_t align)
{
	TLSGlobal* inst = TLSGlobal::instance(true);
	if (!inst)
		return NULL;

	void* p = inst->m_allocator.reallocate(ptr,bytes,align);
	if (ptr && !bytes)
		inst->release();
	else if (p && !ptr)
		inst->addref();
	return p;
}

void OOBase::ThreadLocalAllocator::free(void* ptr)
{
	TLSGlobal* inst = TLSGlobal::instance(false);
	if (inst)
	{
		inst->m_allocator.free(ptr);
		if (ptr)
			inst->release();
	}
}
