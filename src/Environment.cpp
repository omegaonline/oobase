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

int OOBase::Environment::get_current(Set<String,LocalAllocator>& setEnv)
{
	// Keep a set of var names to use for uniqueness testing
	Set<String,LocalAllocator> setVals;

	for (const char** env = (const char**)environ;*env != NULL;++env)
	{
		String str;
		int err = str.assign(*env);
		if (err)
			return err;

		size_t eq = str.find('=');
		if (eq == String::npos)
		{
			if (setVals.exists(str))
				continue;

			err = setVals.insert(str);
		}
		else
		{
			String str2;
			err = str2.assign(str.c_str(),eq);
			if (err)
				return err;

			if (setVals.exists(str2))
				continue;

			err = setVals.insert(str2);
		}

		if (!err)
			err = setEnv.insert(str);

		if (err)
			return err;
	}

	return 0;
}

#endif
