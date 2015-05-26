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

#include "../include/OOBase/CmdArgs.h"
#include "../include/OOBase/Win32.h"
#include "../include/OOBase/UniquePtr.h"

#if defined(_WIN32)
#include <shellapi.h>
#endif

int OOBase::CmdArgs::add_option(const char* id, char short_opt, bool has_value, const char* long_opt)
{
	String strId,strLongOpt;
	if (!strId.assign(id) || !strLongOpt.assign(long_opt))
		return ERROR_OUTOFMEMORY;

	if (strId.empty())
		return EINVAL;
	
	Option opt;
	opt.m_short_opt = short_opt;
	if (long_opt)
		opt.m_long_opt = strLongOpt;
	else
		opt.m_long_opt = strId;

	opt.m_has_value = has_value;

	return m_map_opts.insert(strId,opt) ? 0 : ERROR_OUTOFMEMORY;
}

int OOBase::CmdArgs::error(results_t& results, int err, const char* key, const char* value) const
{
	String strErr,strVal;
	if (!strErr.assign(key) || !strVal.assign(value))
		return ERROR_OUTOFMEMORY;
	
	results.clear();
	if (!results.insert(strErr,strVal))
		return ERROR_OUTOFMEMORY;

	return err;
}

#if defined(_WIN32)
int OOBase::CmdArgs::parse(results_t& results, int skip) const
{
	int argc = 0;
	UniquePtr<LPWSTR,Win32::LocalAllocator> argvw(CommandLineToArgvW(GetCommandLineW(),&argc));
	if (!argvw)
		return GetLastError();

	ScopedArrayPtr<const char*> argv;
	Vector<SharedString<ThreadLocalAllocator>,ThreadLocalAllocator> arg_strings;
	if (!argv.resize(argc) || !arg_strings.resize(argc))
		return ERROR_OUTOFMEMORY;

	for (int i=0;i<argc;++i)
	{
		int err = arg_strings[i].wchar_t_to_utf8(argvw.get()[i]);
		if (err)
			return err;

		argv[i] = arg_strings[i].c_str();
	}

	return parse(argc,argv.get(),results,skip);
}
#endif

int OOBase::CmdArgs::parse(int argc, char* argv[], results_t& results, int skip) const
{
	return parse(argc,(const char**)argv,results,skip);
}

int OOBase::CmdArgs::parse(int argc, const char* argv[], results_t& results, int skip) const
{
	bool bEndOfOpts = false;
	unsigned int pos = 0;
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
			err = parse_arg(results,argv[i],++pos);
		}
	}

	return err;
}

int OOBase::CmdArgs::parse_long_option(results_t& results, const char** argv, int& arg, int argc) const
{
	String strKey,strVal;
	for (Table<String,Option>::const_iterator i = m_map_opts.begin();i; ++i)
	{
		const char* value = "true";
		if (i->second.m_long_opt == argv[arg]+2)
		{
			if (i->second.m_has_value)
			{
				if (arg >= argc-1)
					return error(results,EINVAL,"missing",argv[arg]);
					
				value = argv[++arg];
			}

			if (!strVal.assign(value) || !results.insert(i->first,strVal))
				return ERROR_OUTOFMEMORY;

			return 0;
		}

		if (strncmp(i->second.m_long_opt.c_str(),argv[arg]+2,i->second.m_long_opt.length())==0 && argv[arg][i->second.m_long_opt.length()+2]=='=')
		{
			if (i->second.m_has_value)
				value = &argv[arg][i->second.m_long_opt.length()+3];

			if (!strVal.assign(value) || !results.insert(i->first,strVal))
				return ERROR_OUTOFMEMORY;

			return 0;
		}
	}

	return error(results,ENOENT,"unknown",argv[arg]);
}

int OOBase::CmdArgs::parse_short_options(results_t& results, const char** argv, int& arg, int argc) const
{
	String strKey,strVal;
	for (const char* c = argv[arg]+1; *c!='\0'; ++c)
	{
		Table<String,Option>::const_iterator i = m_map_opts.begin();
		for (;i; ++i)
		{
			if (i->second.m_short_opt == *c)
			{
				if (i->second.m_has_value)
				{
					const char* value;
					if (c[1] == '\0')
					{
						// Next arg is the value
						if (arg >= argc-1)
						{
							int err = strVal.printf("-%c",*c);
							if (err)
								return err;

							return error(results,EINVAL,"missing",strVal.c_str());
						}
						
						value = argv[++arg];
					}
					else
						value = &c[1];

					if (!strVal.assign(value) || !results.insert(i->first,strVal))
						return ERROR_OUTOFMEMORY;

					// No more for this arg...
					return 0;
				}
				else
				{
					if (!strVal.assign("true") || !results.insert(i->first,strVal))
						return ERROR_OUTOFMEMORY;

					break;
				}
			}
		}

		if (!i)
		{
			int err = strVal.printf("-%c",*c);
			if (err)
				return err;
				
			return error(results,ENOENT,"unknown",strVal.c_str());
		}
	}

	return 0;
}

int OOBase::CmdArgs::parse_arg(results_t& results, const char* arg, unsigned int position) const
{
	String strArg;
	if (!strArg.assign(arg))
		return ERROR_OUTOFMEMORY;
	
	String strResult;
	int err = strResult.printf("$%u",position);
	if (err != 0)
		return err;

	return results.insert(strResult,strArg) ? 0 : ERROR_OUTOFMEMORY;
}
