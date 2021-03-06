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

#ifndef OOBASE_ENVIRONMENT_H_INCLUDED_
#define OOBASE_ENVIRONMENT_H_INCLUDED_

#include "String.h"
#include "Table.h"
#include "SharedPtr.h"

namespace OOBase
{
	namespace Environment
	{
		typedef Table<String,String> env_table_t;

		bool get_current(env_table_t& tabEnv);
		bool substitute(env_table_t& tabEnv, const env_table_t& tabSrc);

		bool get_envp(const env_table_t& tabEnv, SharedPtr<char*>& envp);
		bool getenv(const char* envvar, String& strVal);

#if defined(_WIN32)
		int get_user(HANDLE hToken, env_table_t& tabEnv);
		int get_block(const env_table_t& tabEnv, ScopedArrayPtr<wchar_t>& env_block);
#endif
	}
}

#endif // OOBASE_ENVIRONMENT_H_INCLUDED_
