///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../include/OOBase/Environment.h"
#include "../include/OOBase/Win32.h"

#include <stdlib.h>

#if defined(_WIN32)

#include "../include/OOBase/StackAllocator.h"
#include "../include/OOBase/SharedPtr.h"

#include <userenv.h>

namespace
{
	int process_block(const wchar_t* env, OOBase::Environment::env_table_t& tabEnv)
	{
		int err = 0;
		for (const wchar_t* e=env;!err && e != NULL && *e != L'\0';e += wcslen(e)+1)
		{
			OOBase::String str;
			err = str.wchar_t_to_utf8(e);
			if (!err)
			{
				size_t eq = str.find('=');
				if (eq == OOBase::String::npos)
				{
					if (!tabEnv.insert(str,OOBase::String()))
						err = OOBase::system_error();
				}
				else
				{
					OOBase::String strLeft,strRight;
					err = strLeft.assign(str.c_str(),eq);
					if (!err)
						err = strRight.assign(str.c_str()+eq+1);
					if (!err)
					{
						if (!tabEnv.insert(strLeft,strRight))
							err = OOBase::system_error();
					}
				}
			}
		}
		return err;
	}
}

bool OOBase::Environment::get_current(env_table_t& tabEnv)
{
	wchar_t* env = GetEnvironmentStringsW();
	if (!env)
		return GetLastError() == ERROR_SUCCESS;

	int err = process_block(env,tabEnv);

	FreeEnvironmentStringsW(env);

	return (err == ERROR_SUCCESS);
}

int OOBase::Environment::get_user(HANDLE hToken, env_table_t& tabEnv)
{
	LPVOID lpEnv = NULL;
	if (!CreateEnvironmentBlock(&lpEnv,hToken,FALSE))
		return GetLastError();

	int err = process_block(static_cast<wchar_t*>(lpEnv),tabEnv);
		
	if (lpEnv)
		DestroyEnvironmentBlock(lpEnv);

	return err;
}

int OOBase::Environment::get_block(const env_table_t& tabEnv, ScopedArrayPtr<wchar_t>& ptr)
{
	if (tabEnv.empty())
		return 0;

	// Copy and widen to UNICODE
	typedef SharedPtr<wchar_t> temp_wchar_t;

	// Sort environment block - UNICODE, no-locale, case-insensitive (from MSDN)
	struct env_sort
	{
		bool operator ()(const OOBase::SharedPtr<wchar_t>& s1, const OOBase::SharedPtr<wchar_t>& s2) const
		{
			return (_wcsicmp(s1.get(),s2.get()) < 0);
		}
	};

	StackAllocator<1024> allocator;
	Table<temp_wchar_t,temp_wchar_t,env_sort,AllocatorInstance> wenv(env_sort(),allocator);

	size_t total_size = 0;
	for (env_table_t::const_iterator i=tabEnv.begin();i;++i)
	{
		int err = Win32::utf8_to_wchar_t(i->first.c_str(),ptr);
		if (!err)
		{
			// Include \0 and optionally '=' length
			size_t len = wcslen(ptr.get());
			total_size += len + 1;

			temp_wchar_t key = make_shared(static_cast<wchar_t*>(allocator.allocate(len+1,alignment_of<wchar_t>::value)),allocator);
			if (!key)
				return system_error();

			wcscpy(key.get(),ptr.get());

			err = Win32::utf8_to_wchar_t(i->second.c_str(),ptr);
			if (!err)
			{
				temp_wchar_t value;

				len = wcslen(ptr.get());
				if (len)
				{
					total_size += len + 1;

					value = make_shared(static_cast<wchar_t*>(allocator.allocate(len+1,alignment_of<wchar_t>::value)),allocator);
					if (!value)
						return system_error();

					wcscpy(value.get(),ptr.get());
				}

				if (!wenv.insert(key,value))
					return system_error();
			}
		}
		
		if (err)
			return err;
	}

	// And now copy into one giant block
	if (!ptr.resize(total_size + 2))
		return system_error();

	wchar_t* pout = ptr.get();
	for (Table<temp_wchar_t,temp_wchar_t,env_sort,AllocatorInstance>::iterator i=wenv.begin();i;++i)
	{
		const wchar_t* p = i->first.get();

		while (*p != L'\0')
			*pout++ = *p++;

		p = i->second.get();
		if (p && *p != L'\0')
		{
			*pout++ = L'=';

			while (*p != L'\0')
				*pout++ = *p++;
		}

		*pout++ = L'\0';
	}

	// Terminate with \0
	*pout++ = L'\0';
	*pout++ = L'\0';

	return 0;
}

bool OOBase::Environment::getenv(const char* envvar, String& strValue)
{
	ScopedArrayPtr<wchar_t> wenvvar;
	ScopedArrayPtr<wchar_t> wenv;

	int err = Win32::utf8_to_wchar_t(envvar,wenvvar);
	for (DWORD dwLen = 128;!err;)
	{
		if (!wenv.resize(dwLen))
			return false;

		DWORD dwActualLen = GetEnvironmentVariableW(wenvvar.get(),wenv.get(),dwLen);
		if (dwActualLen < dwLen)
		{
			if (dwActualLen == 0)
			{
				err = GetLastError();
				if (err == ERROR_ENVVAR_NOT_FOUND)
					err = 0;
			}
			else if (dwActualLen > 1)
				err = strValue.wchar_t_to_utf8(wenv.get());
			
			break;
		}

		dwLen = dwActualLen;
	}
	
	return err == 0;
}
		
#elif defined(HAVE_UNISTD_H)

bool OOBase::Environment::get_current(env_table_t& tabEnv)
{
	for (const char** env = (const char**)environ;*env != NULL;++env)
	{
		String str;
		if (!str.assign(*env))
			return false;

		size_t eq = str.find('=');
		if (eq == String::npos)
		{
			if (tabEnv.exists(str))
				continue;

			if (!tabEnv.insert(String(),str))
				return false;
		}
		else
		{
			String strK,strV;
			if (!strK.assign(str.c_str(),eq))
				return false;

			if (tabEnv.exists(strK))
				continue;

			if (!strV.assign(str.c_str()+eq+1) || !tabEnv.insert(strK,strV))
				return false;
		}
	}

	return true;
}

bool OOBase::Environment::getenv(const char* envvar, String& strValue)
{
	return strValue.assign(::getenv(envvar));
}

#endif

bool OOBase::Environment::substitute(env_table_t& tabEnv, const env_table_t& tabSrc)
{
	// This might be full of bugs!!

	// Substitute any ${VAR} in tabEnv with values from tabSrc
	for (env_table_t::iterator i = tabEnv.begin(); i; ++i)
	{
		// Loop finding ${VAR}
		for (size_t offset = 0;;)
		{
			size_t start = i->second.find("${",offset);
			if (start == String::npos)
				break;

			size_t end = i->second.find('}',start+2);
			if (end == String::npos)
				break;

			String strPrefix,strSuffix,strParam;
			if (!strPrefix.assign(i->second.c_str(),start) ||
					!strSuffix.assign(i->second.c_str()+end+1) ||
					!strParam.assign(i->second.c_str()+start+2,end-start-2))
			{
				return false;
			}

			i->second = strPrefix;

			env_table_t::const_iterator j = tabSrc.find(strParam);
			if (j)
			{
				if (!i->second.append(j->second))
					return false;
			}

			offset = i->second.length();

			if (!i->second.append(strSuffix))
				return false;
		}
	}

	// Now add any values in tabSrc not in tabEnv
	for (env_table_t::const_iterator i = tabSrc.begin(); i; ++i)
	{
		if (!tabEnv.exists(i->first) && !tabEnv.insert(*i))
			return false;
	}

	return true;
}

bool OOBase::Environment::get_envp(const env_table_t& tabEnv, SharedPtr<char*>& ptr)
{
	if (tabEnv.empty())
		return true;

	// We cheat here and allocate the strings and the array in one block.

	// Count the size needed
	size_t len = (tabEnv.size()+1) * sizeof(char*);
	for (env_table_t::const_iterator i = tabEnv.cbegin(); i != tabEnv.cend(); ++i)
		len += i->first.length() + i->second.length() + 2; // = and NUL

	ptr = make_shared<char*,CrtAllocator>(static_cast<char**>(CrtAllocator::allocate(len,16)));
	if (!ptr)
		return false;

	char** envp = ptr.get();
	char* char_data = reinterpret_cast<char*>(envp) + ((tabEnv.size()+1) * sizeof(char*));

	for (env_table_t::const_iterator i = tabEnv.cbegin(); i != tabEnv.cend(); ++i)
	{
		*envp++ = char_data;

		memcpy(char_data,i->first.c_str(),i->first.length());
		char_data += i->first.length();
		*char_data++ = '=';
		memcpy(char_data,i->second.c_str(),i->second.length());
		char_data += i->second.length();
		*char_data++ = '\0';
	}

	// Add terminating NULL
	*envp = NULL;

	return true;
}
