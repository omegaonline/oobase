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

#include "../include/OOBase/Environment.h"

#if defined(_WIN32)

#error FIX ME!

#elif defined(HAVE_UNISTD_H)

int OOBase::Environment::get_current(Table<String,String,LocalAllocator>& tabEnv)
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

#endif

int OOBase::Environment::substitute(Table<String,String,LocalAllocator>& tabEnv, const Table<String,String,LocalAllocator>& tabSrc)
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

			String strPrefix, strSuffix, strParam;
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
				err = pVal->append(pRepl->c_str(),pRepl->length());
				if (err)
					return err;
			}

			offset = pVal->length();

			err = pVal->append(strSuffix.c_str(),strSuffix.length());
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
