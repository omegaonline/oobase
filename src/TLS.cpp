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

#include <map>

namespace
{
	class TLSMap
	{
	public:
		static TLSMap* instance(bool create = true);

		static void destroy(void* pThis);

		struct tls_val
		{
			void* m_val;
			void (*m_destructor)(void*);
		};

		typedef std::map<const void*,tls_val,std::less<const void*>,OOBase::CriticalAllocator<std::pair<const void*,tls_val> > > mapType;
		mapType m_mapVals;

	private:
		TLSMap() {}
		~TLSMap() {}

		TLSMap(const TLSMap&);
		TLSMap& operator = (const TLSMap&);
	};

#if defined(_WIN32)
	struct Win32TLSGlobal
	{
		Win32TLSGlobal();
		~Win32TLSGlobal();

		DWORD         m_key;
	};
#endif // _WIN32

#if defined(HAVE_PTHREAD)
	struct PthreadTLSGlobal
	{
		PthreadTLSGlobal();
		~PthreadTLSGlobal();

		pthread_key_t m_key;
	};
#endif // HAVE_PTHREAD

#if defined(_WIN32)

	Win32TLSGlobal::Win32TLSGlobal() :
			m_key(TLS_OUT_OF_INDEXES)
	{
		m_key = TlsAlloc();
		if (m_key == TLS_OUT_OF_INDEXES)
			OOBase_CallCriticalFailure(GetLastError());

		if (!TlsSetValue(m_key,0))
			OOBase_CallCriticalFailure(GetLastError());
	}

	Win32TLSGlobal::~Win32TLSGlobal()
	{
		if (m_key != TLS_OUT_OF_INDEXES)
			TlsFree(m_key);
	}
#endif // _WIN32

#if defined(HAVE_PTHREAD)

	PthreadTLSGlobal::PthreadTLSGlobal()
	{
		int err = pthread_key_create(&m_key,0);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

	PthreadTLSGlobal::~PthreadTLSGlobal()
	{
		int err = pthread_key_delete(m_key);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

#endif // HAVE_PTHREAD

	/** \typedef TLS_GLOBAL
	 *  The TLS singleton.
	 *  The singleton is specialised by the platform specific implementation
	 *  e.g. \link Win32TLSGlobal \endlink or \link PthreadTLSGlobal \endlink
	 */
#if defined(_WIN32)
	typedef OOBase::Singleton<Win32TLSGlobal> TLS_GLOBAL;
#elif defined(HAVE_PTHREAD)
	typedef OOBase::Singleton<PthreadTLSGlobal> TLS_GLOBAL;
#else
#error Fix me!
#endif

	TLSMap* TLSMap::instance(bool create)
	{
		TLSMap* inst = 0;
#if defined(_WIN32)
		inst = static_cast<TLSMap*>(TlsGetValue(TLS_GLOBAL::instance().m_key));
#elif defined(HAVE_PTHREAD)
		inst = static_cast<TLSMap*>(pthread_getspecific(TLS_GLOBAL::instance().m_key));
#else
#error Fix me!
#endif
		if (!inst && create)
		{
			OOBASE_NEW_T_CRITICAL(TLSMap,inst,TLSMap());
			
#if defined(_WIN32)
			OOBase::DLLDestructor<OOBase::Module>::add_destructor(destroy,inst);

			if (!TlsSetValue(TLS_GLOBAL::instance().m_key,inst))
				OOBase_CallCriticalFailure(GetLastError());

#elif defined(HAVE_PTHREAD)

			int err = pthread_setspecific(TLS_GLOBAL::instance().m_key,inst);
			if (err != 0)
				OOBase_CallCriticalFailure(err);
#else
#error Fix me!
#endif
		}
		return inst;
	}

	void TLSMap::destroy(void* pThis)
	{
		TLSMap* inst = static_cast<TLSMap*>(pThis);
		if (inst)
		{
			for (mapType::iterator i=inst->m_mapVals.begin(); i!=inst->m_mapVals.end(); ++i)
			{
				if (i->second.m_destructor)
					(*(i->second.m_destructor))(i->second.m_val);
			}
			inst->m_mapVals.clear();
			
			OOBASE_DELETE(TLSMap,inst);
		}

#if defined(_WIN32)
		// Now set 0 back in place...
		if (!TlsSetValue(TLS_GLOBAL::instance().m_key,0))
			OOBase_CallCriticalFailure(GetLastError());
#elif defined(HAVE_PTHREAD)
		int err = pthread_setspecific(TLS_GLOBAL::instance().m_key,0);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
#else
#error Fix me!
#endif
	}
}

bool OOBase::TLS::Get(const void* key, void** val)
{
	TLSMap* inst = TLSMap::instance();

	TLSMap::mapType::iterator i=inst->m_mapVals.find(key);
	if (i == inst->m_mapVals.end())
		return false;

	*val = i->second.m_val;
	
	return true;
}

void OOBase::TLS::Set(const void* key, void* val, void (*destructor)(void*))
{
	TLSMap::tls_val v;
	v.m_val = val;
	v.m_destructor = destructor;

	std::pair<TLSMap::mapType::iterator,bool> p = TLSMap::instance()->m_mapVals.insert(TLSMap::mapType::value_type(key,v));
	if (!p.second)
	{
		p.first->second.m_val = val;
	}
}

void OOBase::TLS::ThreadExit()
{
	TLSMap* inst = TLSMap::instance(false);
	if (inst)
	{
#if defined(_WIN32)
		OOBase::DLLDestructor<OOBase::Module>::remove_destructor(TLSMap::destroy,inst);
#endif
		TLSMap::destroy(inst);
	}
}
