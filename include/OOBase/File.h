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

#include "Buffer.h"
#include "Win32.h"
#include "Posix.h"

namespace OOBase
{
	class File
	{
	public:
		File();
		~File();

		int open(const char* filename, bool writeable = false);

		size_t read(void* p, size_t size);
		size_t read(const RefPtr<Buffer>& buffer, int& err, size_t len = 0);

		size_t write(const void* p, size_t size);
		int write(const RefPtr<Buffer>& buffer, size_t len = 0);

		enum seek_direction
		{
			seek_begin,
			seek_current,
			seek_end
		};
		uint64_t seek(int64_t offset, enum seek_direction dir);
		uint64_t tell() const;

	private:
#if defined(_WIN32)
		Win32::SmartHandle m_fd;
#elif defined(HAVE_UNISTD_H)
		POSIX::SmartFD m_fd;
#else
		#error Implement platform native file handling
#endif
	};
}

#endif // OOBASE_FILE_H_INCLUDED_
