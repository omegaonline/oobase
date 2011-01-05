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

#include "../include/config-base.h"

#if defined(_DEBUG)

#include "../include/OOBase/Singleton.h"
//#include "../include/OOBase/internal/Win32Impl.h"

#include <set>
#include <string>

namespace
{
	class MemWatcher
	{
	public:
		void add(void* p)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			m_setEntries.insert(p);
		}

		void remove(void* p)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			size_t c = m_setEntries.erase(p);

			assert(c == 1);
		}

		static MemWatcher& instance();

	private:
		OOBase::SpinLock   m_lock;
		std::set<void*>    m_setEntries;

		static volatile MemWatcher* s_instance;

		static BOOL __stdcall init(INIT_ONCE*,void*,void**);
		static void init2();
	};
}

volatile MemWatcher* MemWatcher::s_instance = 0;

MemWatcher& MemWatcher::instance()
{
	if (!s_instance)
	{
#if defined(_WIN32)
		static INIT_ONCE key = {0};
		if (!OOBase::Win32::InitOnceExecuteOnce_Internal(&key,&init))
			OOBase_CallCriticalFailure(GetLastError());
#elif defined(HAVE_PTHREAD)
		static pthread_once_t key = PTHREAD_ONCE_INIT;
		int err = pthread_once(key,&init2);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
#else
#error Fix me!
#endif
	}

	return *const_cast<MemWatcher*>(s_instance);
}

BOOL MemWatcher::init(INIT_ONCE*,void*,void**)
{
	init2();
	return TRUE;
}

void MemWatcher::init2()
{
	// Use crt malloc here...
	// This will leak...

	void* TODO;

	MemWatcher* instance = new (std::nothrow) MemWatcher();
	s_instance = instance;
}

#endif // _DEBUG

// flags: 0 - C++ object - align to size, no reallocation
//        1 - Buffer - align 32, reallocation
//        2 - Stack-local buffer - align 32, no reallocation
void* OOBase::Allocate(size_t len, int flags, const char* file, unsigned int line)
{
	/*if (flags == 2)
	{
#if defined(_MSC_VER)
		return _malloca(len);
#else
		flags = 1;
#endif
	}*/
	
	void* p = ::malloc(len);

	//std::ostringstream os;
	//os << "Alloc(" << flags << ") " << p << " " << len << " " << file << " " << line << std::endl;
	//OutputDebugString(os.str().c_str());

#if defined(_DEBUG)
	MemWatcher::instance().add(p);
#endif

	return p;
}

void OOBase::Free(void* mem, int flags)
{
	//std::ostringstream os;
	//os << "Free(" << flags << ")  " << mem << std::endl;
	//OutputDebugString(os.str().c_str());

#if defined(_DEBUG)
	MemWatcher::instance().remove(mem);
#endif
	
	/*if (flags == 2)
	{
#if defined(_MSC_VER)
		_freea(mem);
		return;
#else
		flags = 1;
#endif
	}*/
	
	::free(mem);
}
