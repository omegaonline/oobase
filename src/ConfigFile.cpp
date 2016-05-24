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

#include "../include/OOBase/ConfigFile.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/StackAllocator.h"
#include "../include/OOBase/File.h"
#include "../include/OOBase/Posix.h"
#include "../include/OOBase/Win32.h"

namespace
{
	int invalid_parameter()
	{
	#if defined(_WIN32)
		return ERROR_INVALID_PARAMETER;
	#else
		return EINVAL;
	#endif
	}

	bool is_whitespace(char c)
	{
		return (c == ' ' || c == '\r' || c == '\n' || c == '\t');
	}

	const char*& inc_p(const char*& p, const char* pe, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		if (p != pe)
			++p;

		if (error_pos && p != pe && *p == '\n')
		{
			++error_pos->line;
			error_pos->col = 1;
		}

		return p;
	}

	void skip_whitespace(const char*& p, const char* pe, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		for (;p != pe;inc_p(p,pe,error_pos))
		{
			if (*p == '#' || *p == ';')
			{
				// Comment, skip to \n
				while (p != pe && *p != '\n')
					inc_p(p,pe,error_pos);
			}
			else if (!is_whitespace(*p))
				break;
		}
	}

	const char* ident(const char*& p, const char* pe, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		for (;p != pe;inc_p(p,pe,error_pos))
		{
			if (*p < 'A' || (*p > 'Z' && *p < '_') || (*p > '_' && *p < 'a') || *p > 'z')
				break;
		}
		return p;
	}

	int parse_section(const char*& p, const char* pe, OOBase::String& section, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		skip_whitespace(p,pe,error_pos);
		if (p == pe)
			return invalid_parameter();

		const char* start = p;
		const char* end = ident(p,pe,error_pos);
		if (start == end)
			return invalid_parameter();

		skip_whitespace(p,pe,error_pos);
		if (p == pe || *p != ']')
			return invalid_parameter();
		inc_p(p,pe,error_pos);

		if (!section.assign(start,end-start))
			return OOBase::system_error();

		return 0;
	}

	int parse_entry(const char*& p, const char* pe, const OOBase::String& section, OOBase::ConfigFile::results_t& results, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		const char* start = p;
		const char* end = ident(p,pe,error_pos);
		if (start == end)
			return invalid_parameter();

		OOBase::String key(section);
		if (!key.empty() && !key.append('\\'))
			return OOBase::system_error();

		if (!key.append(start,end - start))
			return OOBase::system_error();

		skip_whitespace(p,pe,error_pos);
		if (p == pe || *p != '=')
			return invalid_parameter();
		inc_p(p,pe,error_pos);

		start = p;
		for (;p != pe;inc_p(p,pe,error_pos))
		{
			if (*p == '\n')
			{
				end = p;
				inc_p(p,pe,error_pos);
				break;
			}

			if (*p == '\r' && p != (pe-1) && p[1] == '\n')
			{
				end = p;
				inc_p(p,pe,error_pos);
				inc_p(p,pe,error_pos);
				break;
			}
			
			if (*p == '\\')
			{
				if (p == (pe-1) || (p[1] != 'n' && p[1] != 'r' && p[1] != '\\'))
					return invalid_parameter();
				inc_p(p,pe,error_pos);
			}
		}		

		OOBase::String value;
		const char* q = start;
		for (;q < end;++q)
		{
			if (*q == '\\')
			{
				if (q > start && !value.append(start,q - start))
					return OOBase::system_error();

				if (q[1] == 'r' && !value.append('\r'))
					return OOBase::system_error();
				else if (q[1] == 'n' && !value.append('\n'))
					return OOBase::system_error();
				else if (q[1] == '\\' && !value.append('\\'))
					return OOBase::system_error();

				start = ++q + 1;
			}
		}

		if (q > start && !value.append(start,q - start))
			return OOBase::system_error();

		if (!results.insert(key,value))
			return OOBase::system_error();

		return 0;
	}
}

int OOBase::ConfigFile::load(const char* filename, results_t& results, error_pos_t* error_pos)
{
	if (error_pos)
	{
		error_pos->line = 1;
		error_pos->col = 1;
	}

	File file;
	int err = file.open(filename,false);
	if (err)
		return err;

	uint64_t len = file.length();
	if (len == 0)
		return 0;

	if (len > size_t(-1))
		return invalid_parameter();

	SharedPtr<const char> data = file.auto_map<const char>(false);
	if (!data)
		return system_error();

	const char* p = data.get();
	const char* pe = p + file.length();

	OOBase::String section;
	while (!err && p != pe)
	{
		skip_whitespace(p,pe,error_pos);
		if (p == pe)
			break;

		if (*p == '[')
			err = parse_section(inc_p(p,pe,error_pos),pe,section,error_pos);
		else
			err = parse_entry(p,pe,section,results,error_pos);
	}

	return err;
}

#if defined(_WIN32)
int OOBase::ConfigFile::load_registry(HKEY hRootKey, const char* key_name, results_t& results)
{
	ScopedArrayPtr<wchar_t> wszKey;
	LONG lRes = Win32::utf8_to_wchar_t(key_name,wszKey);
	if (lRes != ERROR_SUCCESS)
		return lRes;

	// Read from registry
	HKEY hKey = 0;
	lRes = RegOpenKeyExW(hRootKey,wszKey.get(),0,KEY_READ,&hKey);
	if (lRes != ERROR_SUCCESS)
		return lRes;

	// Loop pulling out registry values
	for (DWORD dwIndex=0;; ++dwIndex)
	{
		wchar_t szName[256] = {0};
		DWORD dwNameLen = sizeof(szName)/sizeof(szName[0]);
		DWORD dwType = 0;
		DWORD dwValLen = 0;
		lRes = RegEnumValueW(hKey,dwIndex,szName,&dwNameLen,NULL,&dwType,NULL,&dwValLen);
		if (lRes == ERROR_NO_MORE_ITEMS)
		{
			lRes = 0;
			break;
		}
		else if (lRes != ERROR_SUCCESS)
			break;

		// Skip anything starting with #
		if (dwNameLen >= 1 && szName[0] == L'#')
			continue;

		String value,key;

		szName[dwNameLen-1] = L'\0';
		lRes = key.wchar_t_to_utf8(szName);
		if (lRes)
			break;

		if (dwType == REG_DWORD)
		{
			DWORD dwVal = 0;
			DWORD dwLen = sizeof(dwVal);
			dwNameLen = sizeof(szName)/sizeof(szName[0]);
			lRes = RegEnumValueW(hKey,dwIndex,szName,&dwNameLen,NULL,NULL,(LPBYTE)&dwVal,&dwLen);
			if (lRes != ERROR_SUCCESS)
				break;

			lRes = value.printf("%d",static_cast<int>(dwVal));
			if (lRes)
				break;
		}
		else if (dwType == REG_SZ || dwType == REG_EXPAND_SZ)
		{
			if (!wszKey.resize(dwValLen/sizeof(wchar_t)))
			{
				lRes = system_error();
				break;
			}

			dwNameLen = sizeof(szName)/sizeof(szName[0]);
			lRes = RegEnumValueW(hKey,dwIndex,szName,&dwNameLen,NULL,NULL,(LPBYTE)wszKey.get(),&dwValLen);
			if (lRes != ERROR_SUCCESS)
				break;

			if (dwType == REG_EXPAND_SZ)
			{
				ScopedArrayPtr<wchar_t> ptrEnv;
				for (;;)
				{
					DWORD dwNewLen = ExpandEnvironmentStringsW(wszKey.get(),ptrEnv.get(),static_cast<DWORD>(ptrEnv.count()));
					if (!dwNewLen)
						lRes = ::GetLastError();

					if (dwNewLen < ptrEnv.count())
						break;

					if (!ptrEnv.resize(dwNewLen + 1))
					{
						lRes = system_error();
						break;
					}
				}

				if (lRes == ERROR_SUCCESS)
					lRes = value.wchar_t_to_utf8(ptrEnv.get());
			}
			else
				lRes = value.wchar_t_to_utf8(wszKey.get());

			if (lRes)
				break;
		}
		else
		{
			continue;
		}

		if (!key.empty())
		{
			results_t::iterator i = results.find(key);
			if (!i)
			{
				if (!results.insert(key,value))
				{
					lRes = system_error();
					break;
				}
			}
			else
				i->second = value;
		}
	}

	RegCloseKey(hKey);

	return lRes;
}
#endif
