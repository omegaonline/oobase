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

#include "../include/OOSvrBase/CmdArgs.h"
#include "../include/OOSvrBase/Logger.h"

int OOSvrBase::CmdArgs::add_option(const char* id, char short_opt, bool has_value, const char* long_opt)
{
	OOBase::String strId,strLongOpt;
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

int OOSvrBase::CmdArgs::parse(int argc, char* argv[], results_t& results, int skip) const
{
	return parse(argc,(const char**)argv,results,skip);
}

int OOSvrBase::CmdArgs::parse(int argc, const char* argv[], results_t& results, int skip) const
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
				err = parse_long_option(argv[0],results,argv,i,argc);
			}
			else
			{
				// Short options
				err = parse_short_options(argv[0],results,argv,i,argc);
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

int OOSvrBase::CmdArgs::parse_long_option(const char* name, results_t& results, const char** argv, int& arg, int argc) const
{
	for (size_t i=0; i < m_map_opts.size(); ++i)
	{
		const Option& opt = *m_map_opts.at(i);

		const char* value = "true";
		if (opt.m_long_opt == argv[arg]+2)
		{
			if (opt.m_has_value)
			{
				if (arg >= argc-1)
					LOG_ERROR_RETURN(("%s - Missing argument for option %s",name,argv[arg]),EINVAL);
					
				value = argv[++arg];
			}

			OOBase::String strVal;
			int err = strVal.assign(value);
			if (err != 0)
				return err;

			return results.insert(*m_map_opts.key_at(i),strVal);
		}

		if (strncmp(opt.m_long_opt.c_str(),argv[arg]+2,opt.m_long_opt.length())==0 && argv[arg][opt.m_long_opt.length()+2]=='=')
		{
			if (opt.m_has_value)
				value = &argv[arg][opt.m_long_opt.length()+3];

			OOBase::String strVal;
			int err = strVal.assign(value);
			if (err != 0)
				return err;

			return results.insert(*m_map_opts.key_at(i),strVal);
		}
	}

	LOG_ERROR_RETURN(("%s - Unrecognised option %s",name,argv[arg]),EINVAL);
}

int OOSvrBase::CmdArgs::parse_short_options(const char* name, results_t& results, const char** argv, int& arg, int argc) const
{
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
							LOG_ERROR_RETURN(("%s - Missing argument for option -%s",name,c),EINVAL);
						
						value = argv[++arg];
					}
					else
						value = &c[1];

					// No more for this arg...
					OOBase::String strVal;
					int err = strVal.assign(value);
					if (err != 0)
						return err;

					return results.insert(*m_map_opts.key_at(i),strVal);
				}
				else
				{
					OOBase::String strVal;
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
			LOG_ERROR_RETURN(("%s - Unrecognised option -%c",name,c),EINVAL);
	}

	return 0;
}

int OOSvrBase::CmdArgs::parse_arg(results_t& results, const char* arg, int position) const
{
	OOBase::String strArg;
	int err = strArg.assign(arg);
	if (err != 0)
		return err;

	OOBase::String strResult;
	err = strResult.printf("@%d",position);
	if (err != 0)
		return err;

	return results.insert(strResult,strArg);
}
