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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/CmdArgs.h"

int OOBase::CmdArgs::add_option(const char* id, char short_opt, bool has_value, const char* long_opt)
{
	String strId,strLongOpt;
	int err = strId.assign(id);
	if (err == 0)
		err = strLongOpt.assign(long_opt);

	if (err != 0)
		return err;

	assert(!strId.empty());
	
	Option opt;
	opt.m_short_opt = short_opt;
	if (long_opt)
		opt.m_long_opt = strLongOpt;
	else
		opt.m_long_opt = strId;

	opt.m_has_value = has_value;

	return m_map_opts.insert(strId,opt);
}

int OOBase::CmdArgs::parse(int argc, char* argv[], results_t& results, int skip) const
{
	return parse(argc,(const char**)argv,results,skip);
}

int OOBase::CmdArgs::parse(int argc, const char* argv[], results_t& results, int skip) const
{
	bool bEndOfOpts = false;
	int pos = 0;
	int err = 0;
	for (int i=skip; i<argc && err==0; ++i)
	{
		if (strcmp(argv[i],"--") == 0)
		{
			// -- Terminator
			bEndOfOpts = true;
			continue;
		}

		if (!bEndOfOpts && argv[i][0]=='-' && argv[i][1] != '\0')
		{
			// Options
			if (argv[i][1] == '-')
			{
				// Long option
				err = parse_long_option(results,argv,i,argc);
			}
			else
			{
				// Short options
				err = parse_short_options(results,argv,i,argc);
			}
		}
		else
		{
			// Arguments
			err = parse_arg(results,argv[i],pos++);
		}
	}

	return err;
}

int OOBase::CmdArgs::parse_long_option(results_t& results, const char** argv, int& arg, int argc) const
{
	String strVal;
	for (size_t i=0; i < m_map_opts.size(); ++i)
	{
		const Option& opt = *m_map_opts.at(i);

		const char* value = "true";
		if (opt.m_long_opt == argv[arg]+2)
		{
			if (opt.m_has_value)
			{
				if (arg >= argc-1)
				{
					results.clear();

					String strErr;
					int err = strErr.assign("missing");
					if (err == 0)
						err = strVal.assign(argv[arg]);
					if (err == 0)
						err = results.insert(strErr,strVal);
					if (err == 0)
						err = EINVAL;
					
					return err;
				}
					
				value = argv[++arg];
			}

			int err = strVal.assign(value);
			if (err != 0)
				return err;

			return results.insert(*m_map_opts.key_at(i),strVal);
		}

		if (strncmp(opt.m_long_opt.c_str(),argv[arg]+2,opt.m_long_opt.length())==0 && argv[arg][opt.m_long_opt.length()+2]=='=')
		{
			if (opt.m_has_value)
				value = &argv[arg][opt.m_long_opt.length()+3];

			int err = strVal.assign(value);
			if (err != 0)
				return err;

			return results.insert(*m_map_opts.key_at(i),strVal);
		}
	}
	
	results.clear();
	
	String strErr;
	int err = strErr.assign("unknown");
	if (err == 0)
		err = strVal.assign(argv[arg]);
	if (err == 0)
		err = results.insert(strErr,strVal);
	if (err == 0)
		err = ENOENT;
	
	return err;
}

int OOBase::CmdArgs::parse_short_options(results_t& results, const char** argv, int& arg, int argc) const
{
	String strVal;
	for (const char* c = argv[arg]+1; *c!='\0'; ++c)
	{
		size_t i;
		for (i = 0; i < m_map_opts.size(); ++i)
		{
			const Option& opt = *m_map_opts.at(i);
			if (opt.m_short_opt == *c)
			{
				if (opt.m_has_value)
				{
					const char* value;
					if (c[1] == '\0')
					{
						// Next arg is the value
						if (arg >= argc-1)
						{
							results.clear();
							
							String strErr;
							int err = strErr.assign("missing");
							if (err == 0)
								err = strVal.printf("-%c",*c);
							if (err == 0)
								err = results.insert(strErr,strVal);
							if (err == 0)
								err = EINVAL;
							
							return err;
						}
						
						value = argv[++arg];
					}
					else
						value = &c[1];

					// No more for this arg...
					int err = strVal.assign(value);
					if (err != 0)
						return err;

					return results.insert(*m_map_opts.key_at(i),strVal);
				}
				else
				{
					int err = strVal.assign("true");
					if (err == 0)
						err = results.insert(*m_map_opts.key_at(i),strVal);

					if (err != 0)
						return err;

					break;
				}
			}
		}

		if (i == m_map_opts.size())
		{
			results.clear();

			String strErr;
			int err = strErr.assign("unknown");
			if (err == 0)
				err = strVal.printf("-%c",*c);
			if (err == 0)
				err = results.insert(strErr,strVal);
			if (err == 0)
				err = ENOENT;
			
			return err;
		}
	}

	return 0;
}

int OOBase::CmdArgs::parse_arg(results_t& results, const char* arg, int position) const
{
	String strArg;
	int err = strArg.assign(arg);
	if (err != 0)
		return err;

	String strResult;
	err = strResult.printf("@%d",position);
	if (err != 0)
		return err;

	return results.insert(strResult,strArg);
}
