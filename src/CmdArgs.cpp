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
		return system_error();

	if (strId.empty())
		return EINVAL;
	
	Option opt;
	opt.m_short_opt = short_opt;
	if (long_opt)
		opt.m_long_opt = strLongOpt;
	else
		opt.m_long_opt = strId;

	opt.m_has_value = has_value;

	return m_map_opts.insert(strId,opt) ? 0 : system_error();
}

int OOBase::CmdArgs::error(options_t& options, int err, const char* key, const char* value) const
{
	String strErr,strVal;
	if (!strErr.assign(key) || !strVal.assign(value))
		return system_error();
	
	options.clear();
	if (!options.insert(strErr,strVal))
		return system_error();

	return err;
}

#if defined(_WIN32)
int OOBase::CmdArgs::parse(options_t& options, arguments_t& args, int skip) const
{
	int argc = 0;
	UniquePtr<LPWSTR,Win32::LocalAllocator> argvw(CommandLineToArgvW(GetCommandLineW(),&argc));
	if (!argvw)
		return GetLastError();

	ScopedArrayPtr<const char*> argv;
	Vector<SharedString<ThreadLocalAllocator>,ThreadLocalAllocator> arg_strings;
	if (!argv.resize(argc) || !arg_strings.resize(argc))
		return system_error();

	for (int i=0;i<argc;++i)
	{
		int err = arg_strings[i].wchar_t_to_utf8(argvw.get()[i]);
		if (err)
			return err;

		argv[i] = arg_strings[i].c_str();
	}

	return parse(argc,argv.get(),options,args,skip);
}
#endif

int OOBase::CmdArgs::parse(int argc, char* argv[], options_t& options, arguments_t& args, int skip) const
{
	return parse(argc,(const char**)argv,options,args,skip);
}

int OOBase::CmdArgs::parse(int argc, const char* argv[], options_t& options, arguments_t& args, int skip) const
{
	bool bEndOfOpts = false;
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
				err = parse_long_option(options,argv,i,argc);
			}
			else
			{
				// Short options
				err = parse_short_options(options,argv,i,argc);
			}
		}
		else
		{
			// Argument
			String strVal;
			if (!strVal.assign(argv[i]))
				err = system_error();
			else
				err = args.push_back(strVal) ? 0 : system_error();
		}
	}

	return err;
}

int OOBase::CmdArgs::parse_long_option(options_t& options, const char** argv, int& arg, int argc) const
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
					return error(options,EINVAL,"missing",argv[arg]);
					
				value = argv[++arg];
			}

			if (!strVal.assign(value) || !options.insert(i->first,strVal))
				return system_error();

			return 0;
		}

		if (strncmp(i->second.m_long_opt.c_str(),argv[arg]+2,i->second.m_long_opt.length())==0 && argv[arg][i->second.m_long_opt.length()+2]=='=')
		{
			if (i->second.m_has_value)
				value = &argv[arg][i->second.m_long_opt.length()+3];

			if (!strVal.assign(value) || !options.insert(i->first,strVal))
				return system_error();

			return 0;
		}
	}

	return error(options,ENOENT,"unknown",argv[arg]);
}

int OOBase::CmdArgs::parse_short_options(options_t& options, const char** argv, int& arg, int argc) const
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

							return error(options,EINVAL,"missing",strVal.c_str());
						}
						
						value = argv[++arg];
					}
					else
						value = &c[1];

					if (!strVal.assign(value) || !options.insert(i->first,strVal))
						return system_error();

					// No more for this arg...
					return 0;
				}
				else
				{
					if (!strVal.assign("true") || !options.insert(i->first,strVal))
						return system_error();

					break;
				}
			}
		}

		if (!i)
		{
			int err = strVal.printf("-%c",*c);
			if (err)
				return err;
				
			return error(options,ENOENT,"unknown",strVal.c_str());
		}
	}

	return 0;
}
