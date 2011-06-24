///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef OOSVRBASE_CMDARGS_H_INCLUDED_
#define OOSVRBASE_CMDARGS_H_INCLUDED_

#include "../OOBase/Memory.h"
#include "../OOBase/String.h"
#include "../OOBase/Table.h"

namespace OOSvrBase
{
	class CmdArgs
	{
	public:
		int add_option(const char* id, char short_opt = 0, bool has_value = false, const char* long_opt = NULL);
		
		typedef OOBase::Table<OOBase::String,OOBase::String> results_t;
		int parse(int argc, char* argv[], results_t& results, int skip = 1) const;
		int parse(int argc, const char* argv[], results_t& results, int skip = 1) const;

	private:
		struct Option
		{
			char           m_short_opt;
			OOBase::String m_long_opt;
			bool           m_has_value;
		};

		OOBase::Table<OOBase::String,Option> m_map_opts;
		
		int parse_long_option(const char* name, results_t& results, const char** argv, int& arg, int argc) const;
		int parse_short_options(const char* name, results_t& results, const char** argv, int& arg, int argc) const;
		int parse_arg(results_t& results, const char* opt, int position) const;
	};
}

#endif // OOSVRBASE_CMDARGS_H_INCLUDED_
