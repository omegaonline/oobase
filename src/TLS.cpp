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
		TLSGlobal() : m_refcount(1)
		{}

		void addref() 
		{
			++m_refcount; 
		}

		void release();

		static TLSGlobal* instance();
		
		OOBase::ArenaAllocator m_allocator;

		struct tls_val
		{
			void* m_val;
			void (*m_destructor)(void*);
		};
		OOBase::HashTable<const void*,tls_val,OOBase::ThreadLocalAllocator> m_mapVals;
		
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

TLSGlobal* TLSGlobal::instance()
{
	static OOBase::Once::once_t key = ONCE_T_INIT;
	OOBase::Once::Run(&key,init);
		
	TLSGlobal* inst = NULL;
	if (s_key != TLS_OUT_OF_INDEXES)
	{
		inst = static_cast<TLSGlobal*>(TlsGetValue(s_key));
		if (!inst && OOBase::CrtAllocator::allocate_new(inst))
		{
			OOBase::DLLDestructor<OOBase::Module>::add_destructor(&term,inst);
			TlsSetValue(s_key,inst);
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

void OOBase::TLS::ThreadExit()
{
	TLSGlobal* inst = TLSGlobal::instance();
	if (inst)
	{
		OOBase::DLLDestructor<OOBase::Module>::remove_destructor(&term,inst);
		inst->release();
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

TLSGlobal* TLSGlobal::instance()
{
	static OOBase::Once::once_t key = ONCE_T_INIT;
	OOBase::Once::Run(&key,init);
		
	TLSGlobal* inst = static_cast<TLSGlobal*>(pthread_getspecific(s_key));
	if (!inst && OOBase::CrtAllocator::allocate_new(inst))
	{
		pthread_setspecific(s_key,inst);
		OOBase::DLLDestructor<OOBase::Module>::add_destructor(&term,inst);
	}
		
	return inst;
}

void TLSGlobal::release()
{
	if (--m_refcount == 0)
		OOBase::CrtAllocator::delete_free(this);
}

void OOBase::TLS::ThreadExit()
{
}

#endif

bool OOBase::TLS::Get(const void* key, void** val)
{
	TLSGlobal* inst = TLSGlobal::instance();

	OOBase::HashTable<const void*,TLSGlobal::tls_val,OOBase::ThreadLocalAllocator>::iterator i = inst->m_mapVals.find(key);
	if (i == inst->m_mapVals.end())
		return false;

	if (val)
		*val = i->value.m_val;

	return true;
}

bool OOBase::TLS::Set(const void* key, void* val, void (*destructor)(void*))
{
	TLSGlobal* inst = TLSGlobal::instance();

	TLSGlobal::tls_val v;
	v.m_val = val;
	v.m_destructor = destructor;

	return inst->m_mapVals.insert(key,v) != inst->m_mapVals.end();
}

char* OOBase::detail::get_error_buffer(size_t& len)
{
	TLSGlobal* inst = TLSGlobal::instance();
	if (!inst)
		return NULL;

	len = sizeof(inst->m_error_buffer);
	return inst->m_error_buffer;
}

void* OOBase::ThreadLocalAllocator::allocate(size_t bytes, size_t align)
{
	TLSGlobal* inst = TLSGlobal::instance();
	if (!inst)
		return NULL;

	void* p = inst->m_allocator.allocate(bytes,align);
	if (p)
		inst->addref();
	return p;
}

void* OOBase::ThreadLocalAllocator::reallocate(void* ptr, size_t bytes, size_t align)
{
	TLSGlobal* inst = TLSGlobal::instance();
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
	TLSGlobal* inst = TLSGlobal::instance();
	if (inst)
	{
		inst->m_allocator.free(ptr);
		if (ptr)
			inst->release();
	}
}
