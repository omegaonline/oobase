///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#ifndef OOBASE_FILE_H_INCLUDED_
#define OOBASE_FILE_H_INCLUDED_

#include "Win32.h"
#include "Posix.h"

namespace OOBase
{
	class File
	{
	public:
		File();
		~File();

		int open(const char* filename, bool writeable);

		size_t read(void* p, size_t size);
		size_t write(const void* p, size_t size);

		enum seek_direction
		{
			seek_begin,
			seek_current,
			seek_end
		};
		uint64_t seek(int64_t offset, enum seek_direction dir);
		uint64_t tell() const;

		uint64_t length() const;

		enum map_flags
		{
			map_read = 1,
			map_write = 2,
			map_exec = 4,
			map_shared = 8,
			map_private = 0,
		};

		void* map(unsigned int flags, uint64_t offset, size_t& length);
		bool unmap(void* p, size_t length);

		template <typename T>
		SharedPtr<T> auto_map(unsigned int flags, uint64_t offset = 0, size_t length = -1)
		{
			return reinterpret_pointer_cast<T,char>(auto_map_i(flags,offset,length));
		}

	private:
#if defined(_WIN32)
		Win32::SmartHandle m_fd;
#elif defined(HAVE_UNISTD_H)
		POSIX::SmartFD m_fd;
#else
		#error Implement platform native file handling
#endif
		SharedPtr<char> auto_map_i(unsigned int flags, uint64_t offset, size_t length);
	};
}

#endif // OOBASE_FILE_H_INCLUDED_
