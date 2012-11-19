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

#ifndef OOBASE_CONFIGFILE_H_INCLUDED_
#define OOBASE_CONFIGFILE_H_INCLUDED_

#include "String.h"
#include "Table.h"

namespace OOBase
{
	namespace ConfigFile
	{
		typedef Table<String,String> results_t;

		struct error_pos_t
		{
			unsigned int line;
			unsigned int col;
		};

		int load(const char* filename, results_t& results, error_pos_t* error_pos);

#if defined(_WIN32)
		int load_registry(HKEY hRootKey, const char* key, results_t& results);
#endif
	};
}

#endif // OOBASE_CONFIGFILE_H_INCLUDED_
