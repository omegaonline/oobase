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

#ifndef OOBASE_CMDARGS_H_INCLUDED_
#define OOBASE_CMDARGS_H_INCLUDED_

#include "String.h"
#include "Table.h"

namespace OOBase
{
	class CmdArgs
	{
	public:
		int add_option(const char* id, char short_opt = 0, bool has_value = false, const char* long_opt = NULL);
		
		typedef Table<String,String> results_t;
		int parse(int argc, char* argv[], results_t& results, int skip = 1) const;
		int parse(int argc, const char* argv[], results_t& results, int skip = 1) const;

	private:
		struct Option
		{
			char   m_short_opt;
			String m_long_opt;
			bool   m_has_value;
		};

		Table<String,Option> m_map_opts;
		
		int parse_long_option(results_t& results, const char** argv, int& arg, int argc) const;
		int parse_short_options(results_t& results, const char** argv, int& arg, int argc) const;
		int parse_arg(results_t& results, const char* opt, size_t position) const;
	};
}

#endif // OOBASE_CMDARGS_H_INCLUDED_
