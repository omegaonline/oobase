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
			err = OOBase::Win32::wchar_t_to_utf8(e,str);
			if (!err)
			{
				size_t eq = str.find('=');
				if (eq == OOBase::String::npos)
					err = tabEnv.insert(str,OOBase::String());
				else
				{
					OOBase::String strLeft,strRight;
					err = strLeft.assign(str.c_str(),eq);
					if (!err)
						err = strRight.assign(str.c_str()+eq+1);
					if (!err)
						err = tabEnv.insert(strLeft,strRight);
				}
			}
		}
		return err;
	}

	bool env_sort(const OOBase::SharedPtr<wchar_t>& s1, const OOBase::SharedPtr<wchar_t>& s2)
	{
		return (_wcsicmp(s1.get(),s2.get()) < 0);
	}
}

int OOBase::Environment::get_current(env_table_t& tabEnv)
{
	wchar_t* env = GetEnvironmentStringsW();
	if (!env)
		return GetLastError();

	int err = process_block(env,tabEnv);

	FreeEnvironmentStringsW(env);

	return err;
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

	StackAllocator<1024> allocator;
	Table<temp_wchar_t,temp_wchar_t,AllocatorInstance> wenv(allocator);

	size_t total_size = 0;
	for (size_t i=0;i<tabEnv.size();++i)
	{
		int err = Win32::utf8_to_wchar_t(tabEnv.key_at(i)->c_str(),ptr);
		if (!err)
		{
			// Include \0 and optionally '=' length
			size_t len = wcslen(ptr.get());
			total_size += len + 1;

			temp_wchar_t key(static_cast<wchar_t*>(allocator.allocate(len+1,alignment_of<wchar_t>::value)),allocator);
			if (!key)
				err = ERROR_OUTOFMEMORY;
			else
			{
				wcscpy(key.get(),ptr.get());

				err = Win32::utf8_to_wchar_t(tabEnv.at(i)->c_str(),ptr);
				if (!err)
				{
					temp_wchar_t value;

					len = wcslen(ptr.get());
					if (len)
					{
						total_size += len + 1;

						value = temp_wchar_t(static_cast<wchar_t*>(allocator.allocate(len+1,alignment_of<wchar_t>::value)),allocator);
						if (!value)
							err = ERROR_OUTOFMEMORY;
						else
							wcscpy(value.get(),ptr.get());
					}

					if (!err)
						err = wenv.insert(key,value);
				}
			}
		}
		
		if (err)
			return err;
	}

	// Sort environment block - UNICODE, no-locale, case-insensitive (from MSDN)
	wenv.sort(&env_sort);

	// And now copy into one giant block
	if (!ptr.reallocate(total_size + 2))
		return ERROR_OUTOFMEMORY;

	wchar_t* pout = ptr.get();
	for (size_t i=0;i<wenv.size();++i)
	{
		const wchar_t* p = wenv.key_at(i)->get();

		while (*p != L'\0')
			*pout++ = *p++;

		p = wenv.at(i)->get();
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

int OOBase::Environment::getenv(const char* envvar, ScopedArrayPtr<char>& strValue)
{
	ScopedArrayPtr<wchar_t> wenvvar;
	ScopedArrayPtr<wchar_t> wenv;

	int err = Win32::utf8_to_wchar_t(envvar,wenvvar);
	for (DWORD dwLen = 128;!err;)
	{
		if (!wenv.reallocate(dwLen))
			return ERROR_OUTOFMEMORY;

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
				err = Win32::wchar_t_to_utf8(wenv.get(),strValue);
			
			break;
		}

		dwLen = dwActualLen;
	}
	
	return err;
}
		
#elif defined(HAVE_UNISTD_H)

int OOBase::Environment::get_current(env_table_t& tabEnv)
{
	for (const char** env = (const char**)environ;*env != NULL;++env)
	{
		String str;
		int err = str.assign(*env);
		if (err)
			return err;

		size_t eq = str.find('=');
		if (eq == String::npos)
		{
			if (tabEnv.exists(str))
				continue;

			err = tabEnv.insert(String(),str);
		}
		else
		{
			String strK,strV;
			err = strK.assign(str.c_str(),eq);
			if (err)
				return err;

			if (tabEnv.exists(strK))
				continue;

			err = strV.assign(str.c_str()+eq+1);
			if (!err)
				err = tabEnv.insert(strK,strV);
		}

		if (err)
			return err;
	}

	return 0;
}

int OOBase::Environment::getenv(const char* envvar, ScopedArrayPtr<char>& strValue)
{
	const char* e = ::getenv(envvar);
	size_t len = strlen(e);

	if (!strValue.reallocate(len+1))
		return ERROR_OUTOFMEMORY;

	if (len)
		memcpy(strValue.get(),e,len+1);
	else
		strValue[0] = '\0';

	return 0;
}

#endif

int OOBase::Environment::substitute(env_table_t& tabEnv, const env_table_t& tabSrc)
{
	// This might be full of bugs!!

	// Substitute any ${VAR} in tabEnv with values from tabSrc
	for (size_t idx = 0; idx < tabEnv.size(); ++idx)
	{
		String* pVal = tabEnv.at(idx);

		// Loop finding ${VAR}
		for (size_t offset = 0;;)
		{
			size_t start = pVal->find("${",offset);
			if (start == String::npos)
				break;

			size_t end = pVal->find('}',start+2);
			if (end == String::npos)
				break;

			String strPrefix,strSuffix,strParam;
			int err = strPrefix.assign(pVal->c_str(),start);
			if (!err)
				err = strSuffix.assign(pVal->c_str()+end+1);
			if (!err)
				err = strParam.assign(pVal->c_str()+start+2,end-start-2);
			if (err)
				return err;

			*pVal = strPrefix;

			const String* pRepl = tabSrc.find(strParam);
			if (pRepl)
			{
				err = pVal->append(*pRepl);
				if (err)
					return err;
			}

			offset = pVal->length();

			err = pVal->append(strSuffix);
			if (err)
				return err;
		}
	}

	// Now add any values in tabSrc not in tabEnv
	for (size_t idx = 0; idx < tabSrc.size(); ++idx)
	{
		const String* key = tabSrc.key_at(idx);
		if (!tabEnv.exists(*key))
		{
			int err = tabEnv.insert(*key,*tabSrc.at(idx));
			if (err)
				return err;
		}
	}

	return 0;
}

int OOBase::Environment::get_envp(const env_table_t& tabEnv, ScopedArrayPtr<char*>& ptr)
{
	if (tabEnv.empty())
		return 0;

	// We cheat here and allocate the strings and the array in one block.

	// Count the size needed
	size_t len = (tabEnv.size()+1) * sizeof(char*);
	for (size_t idx = 0; idx < tabEnv.size(); ++idx)
		len += tabEnv.key_at(idx)->length() + tabEnv.at(idx)->length() + 2; // = and NUL

	if (!ptr.reallocate(len))
		return ERROR_OUTOFMEMORY;

	char** envp = ptr.get();
	char* char_data = reinterpret_cast<char*>(envp) + ((tabEnv.size()+1) * sizeof(char*));

	for (size_t idx = 0; idx < tabEnv.size(); ++idx)
	{
		*envp++ = char_data;

		const String* key = tabEnv.key_at(idx);
		const String* val = tabEnv.at(idx);
		memcpy(char_data,key->c_str(),key->length());
		char_data += key->length();
		*char_data++ = '=';
		memcpy(char_data,val->c_str(),val->length());
		char_data += val->length();
		*char_data++ = '\0';
	}

	// Add terminating NULL
	*envp = NULL;

	return 0;
}
