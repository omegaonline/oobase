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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/ConfigFile.h"
#include "../include/OOBase/Buffer.h"

#include "../include/OOBase/Posix.h"
#include "../include/OOBase/Win32.h"

namespace
{
	bool whitespace(const char c)
	{
		return (c == ' ' || c == '\r' || c == '\n' || c == '\t');
	}

	bool comment(const char c)
	{
		return (c == '#');
	}

	int parse_section(const char* key_start, const char* end, OOBase::String& strSection, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		// Check for closing ] (And whitespace or comments in the section name)
		const char* key_end = key_start + 1;
		while (key_end < end && *key_end != ']' && !comment(*key_end) && !whitespace(*key_end))
			++key_end;

		if (*key_end != ']')
		{
			if (error_pos)
				error_pos->col += (key_end - key_start);

			return EINVAL;
		}

		// Check for trailing nonsense
		const char* p = key_end + 1;
		while (p < end && whitespace(*p))
			++p;

		if (p < end && !comment(*p))
		{
			if (error_pos)
				error_pos->col += (p - key_start);

			return EINVAL;
		}

		// Update the section name, [] clears it...
		if (key_end != key_start + 1)
		{
			int err = strSection.assign(key_start+1,key_end - key_start - 1);
			if (err)
				return err;
		}
		else
			strSection.clear();

		return 0;
	}

	int parse_line(const char* key_start, const char* end, const OOBase::String& strSection, OOBase::ConfigFile::results_t& results, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		// Check for = (and whitespace or comments)
		const char* key_end = key_start;
		while (key_end < end && *key_end != '=' && !comment(*key_end) && !whitespace(*key_end))
			++key_end;

		const char* value_start = NULL;
		if (key_end < end && whitespace(*key_end))
		{
			// Skip whitespace after key up to =
			value_start = key_end + 1;
			while (value_start < end && whitespace(*value_start))
				++value_start;

			if (value_start == end || comment(*value_start))
			{
				value_start = NULL;
			}
			else if (*value_start == '=')
			{
				++value_start;
			}
			else
			{
				if (error_pos)
					error_pos->col += (value_start - key_start);

				return EINVAL;
			}
		}
		else if (*key_end == '=')
		{
			value_start = key_end + 1;
		}
		else if (key_start == key_end)
		{
			return 0;
		}

		int err = 0;
		OOBase::String strKey = strSection;
		if (!strKey.empty())
			err = strKey.append("/",1);
		if (!err)
			err = strKey.append(key_start,key_end - key_start);
		if (err)
			return err;

		OOBase::String strValue;
		if (value_start)
		{
			// Skip leading whitespace before value
			while (value_start < end && whitespace(*value_start))
				++value_start;

			// Search for the end of the value...
			const char* value_end = value_start;
			while (value_end < end && !comment(*value_end))
				++value_end;

			if (value_end > value_start)
			{
				err = strValue.assign(value_start,value_end - value_start);
				if (err)
					return err;
			}
		}

		OOBase::String* pv = results.find(strKey);
		if (!pv)
			return results.insert(strKey,strValue);

		*pv = strValue;
		return 0;
	}

	int parse_text(OOBase::RefPtr<OOBase::Buffer>& buffer, OOBase::String& strSection, OOBase::ConfigFile::results_t& results, OOBase::ConfigFile::error_pos_t* error_pos)
	{
		// Split into lines...
		while (buffer->length() > 0)
		{
			const char* start = buffer->rd_ptr();
			const char* end = start + buffer->length();
			const char* p = start;

			// Skip leading whitespace
			while (p < end && whitespace(*p))
			{
				if (error_pos)
				{
					if (*p == '\n')
					{
						++error_pos->line;
						error_pos->col = 1;
					}
					else
						++error_pos->col;
				}
				++p;
			}

			// Find the next LF
			const char* key_start = p;
			while (p < end && *p != '\n')
				++p;

			if (*p != '\n')
			{
				// Incomplete line

				// Move rd_ptr to the start of the key, ready for next parse
				buffer->rd_ptr(key_start - start);
				break;
			}

			// Mark end of current line
			end = p + 1;

			// Trim trailing whitespace
			while (p > key_start && whitespace(*p))
				--p;

			if (p > key_start)
			{
				int err = 0;
				if (*key_start == '[')
					err = parse_section(key_start,p+1,strSection,error_pos);
				else
					err = parse_line(key_start,p+1,strSection,results,error_pos);

				if (err)
					return err;
			}

			// Move rd_ptr to the next line
			buffer->rd_ptr(end - start);

			// Update error counter
			if (error_pos)
			{
				++error_pos->line;
				error_pos->col = 1;
			}
		}

		return 0;
	}
}

int OOBase::ConfigFile::load(const char* filename, results_t& results, error_pos_t* error_pos)
{
	if (error_pos)
	{
		error_pos->line = 1;
		error_pos->col = 1;
	}

#if defined(HAVE_UNISTD_H)
	OOBase::POSIX::SmartFD f(OOBase::POSIX::open(filename,O_RDONLY));
	if (!f.is_valid())
		return errno;
#elif defined(_WIN32)
	OOBase::Win32::SmartHandle f(::CreateFileA(filename),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL));
	if (!f.is_valid())
		return ::GetLastError();
#else
#error No idea how to open a file!
#endif

	static const size_t s_read_block_size = 4096;

	OOBase::RefPtr<OOBase::Buffer> buffer = new (std::nothrow) OOBase::Buffer(s_read_block_size);
	if (!buffer)
		return ERROR_OUTOFMEMORY;

	OOBase::String strSection;
	for (;;)
	{
#if defined(HAVE_UNISTD_H)
		ssize_t r = OOBase::POSIX::read(f,buffer->wr_ptr(),buffer->space());
#elif defined(_WIN32)
		DWORD r = 0;
		if (!::ReadFile(f,buffer->wr_ptr(),static_cast<DWORD>(buffer->space()),&r,NULL);
			return GetLastError();
#endif

		if (r == 0)
		{
			// Add a trailing /n
			*buffer->wr_ptr() = '\n';
			buffer->wr_ptr(1);
		}
		else
		{
			// Update wr_ptr
			buffer->wr_ptr(r);
		}

		int err = parse_text(buffer,strSection,results,error_pos);
		if (err != 0)
			return err;

		if (r == 0)
			return 0;

		// Compact before reading again...
		buffer->compact(1);

		// And ensure we have at least s_read_block_size bytes of room
		err = buffer->space(s_read_block_size);
		if (err)
			return err;
	}
}

#if defined(_WIN32)
int OOBase::ConfigFile::load_registry(const char* key, results_t& results)
{
	return 0;
}
#endif
