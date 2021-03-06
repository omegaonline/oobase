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

#include "../include/OOBase/DLL.h"
#include "../include/OOBase/Singleton.h"

template class OOBase::DLLDestructor<OOBase::Module>;

#if defined(_WIN32)

OOBase::DLL::DLL() :
		m_module(NULL)
{
}

OOBase::DLL::~DLL()
{
	unload();
}

int OOBase::DLL::load(const char* full_path)
{
	ScopedArrayPtr<wchar_t> wide_path;
	int err = Win32::utf8_to_wchar_t(full_path,wide_path);
	if (err)
		return err;

	m_module = LoadLibraryW(wide_path.get());
	if (!m_module)
		return GetLastError();

	return 0;
}

bool OOBase::DLL::unload()
{
	if (!m_module)
		return true;

	if (!FreeLibrary(m_module))
		return false;

	m_module = NULL;
	return true;
}

void* OOBase::DLL::symbol(const char* sym_name)
{
	if (!m_module)
		return NULL;

	return (void*)GetProcAddress(m_module,sym_name);
}

#else

namespace
{
	struct Libtool_Helper
	{
		Libtool_Helper()
		{
			OOBase::Guard<OOBase::Mutex> guard(m_lock);

			int err = lt_dlinit();
			if (err != 0)
				OOBase_CallCriticalFailure(lt_dlerror());
		}

		~Libtool_Helper()
		{
			OOBase::Guard<OOBase::Mutex> guard(m_lock);

			lt_dlexit();
		}

		OOBase::Mutex m_lock;
	};

	typedef OOBase::Singleton<Libtool_Helper,OOBase::Module> LT_HELPER;
}

template class OOBase::Singleton<Libtool_Helper,OOBase::Module>;

OOBase::DLL::DLL() :
		m_module(NULL)
{
}

OOBase::DLL::~DLL()
{
	unload();
}

int OOBase::DLL::load(const char* full_path)
{
	OOBase::Guard<OOBase::Mutex> guard(LT_HELPER::instance().m_lock);

	lt_dladvise adv;
	int err2 = 0;
	int err = lt_dladvise_init(&adv);
	if (!err)
	{
		err = lt_dladvise_local(&adv);
		if (!err)
		{
			err = lt_dladvise_ext(&adv);
			if (!err)
			{
				m_module = lt_dlopenadvise(full_path,adv);
				if (!m_module)
					err2 = errno;
			}
		}

		lt_dladvise_destroy(&adv);
	}

	if (err2)
		return err2;

	if (err)
		OOBase_CallCriticalFailure(lt_dlerror());

	return 0;
}

bool OOBase::DLL::unload()
{
	if (!m_module)
		return true;

	OOBase::Guard<OOBase::Mutex> guard(LT_HELPER::instance().m_lock);

	if (lt_dlclose(m_module))
		return false;

	m_module = NULL;
	return true;
}

void* OOBase::DLL::symbol(const char* sym_name)
{
	if (!m_module)
		return NULL;

	OOBase::Guard<OOBase::Mutex> guard(LT_HELPER::instance().m_lock);

	return lt_dlsym(m_module,sym_name);
}

#endif

