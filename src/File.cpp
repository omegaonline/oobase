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

#include "../include/OOBase/File.h"
#include "../include/OOBase/StackAllocator.h"
#include "../include/OOBase/Win32.h"

OOBase::File::File()
{
}

OOBase::File::~File()
{
}

int OOBase::File::open(const char* filename, bool writeable)
{
	if (!filename)
		return EINVAL;

#if defined(_WIN32)
	StackAllocator<512> allocator;
	TempPtr<wchar_t> wname(allocator);
	int err = Win32::utf8_to_wchar_t(filename,wname);
	if (err)
		return err;

	DWORD dwAccess = GENERIC_READ;
	DWORD dwDisp = OPEN_EXISTING;
	if (writeable)
	{
		dwAccess |= GENERIC_WRITE;
		dwDisp = OPEN_ALWAYS;
	}

	m_fd = ::CreateFileW(wname,dwAccess,FILE_SHARE_READ,NULL,dwDisp,FILE_ATTRIBUTE_NORMAL,NULL);
	if (!m_fd.is_valid())
		return ::GetLastError();

#elif defined(HAVE_UNISTD_H)

	int flags = O_RDONLY;
	if (writeable)
		flags = O_RDWR | O_CREAT;
		
	m_fd = POSIX::open(filename,flags,0664);
	if (!m_fd.is_valid())
		return errno;
#endif
	return 0;
}

long OOBase::File::read(void* p, unsigned long size)
{
#if defined(_WIN32)
	DWORD r = 0;
	if (!::ReadFile(m_fd,p,size,&r,NULL))
		return -1;
	return r;
#elif defined(HAVE_UNISTD_H)
	return POSIX::read(m_fd,p,size);
#endif
}

size_t OOBase::File::read(Buffer* buffer, int& err, size_t len)
{
	if (!buffer)
	{
		err = EINVAL;
		return 0;
	}

	if (!len)
		len = buffer->space();

	size_t tot = 0;
	while (len)
	{
		long r = read(buffer->wr_ptr(),len);
		if (r <= 0)
		{
			if (r != 0)
			{
#if defined(_WIN32)
				err = ::GetLastError();
#elif defined(HAVE_UNISTD_H)
				err = errno;
#endif
			}
			break;
		}

		tot += r;
		buffer->wr_ptr(r);
		len -= r;
	}

	return tot;
}

long OOBase::File::write(const void* p, unsigned long size)
{
#if defined(_WIN32)
	DWORD w = 0;
	if (!::WriteFile(m_fd,p,size,&w,NULL))
		return -1;
	return w;
#elif defined(HAVE_UNISTD_H)
	return POSIX::write(m_fd,p,size);
#endif
}

int OOBase::File::write(Buffer* buffer, size_t len)
{
	if (!buffer)
		return EINVAL;

	if (!len)
		len = buffer->length();

	while (len)
	{
		long w = write(buffer->rd_ptr(),len);
		if (w < 0)
		{
#if defined(_WIN32)
			return ::GetLastError();
#elif defined(HAVE_UNISTD_H)
			return errno;
#endif
		}
		
		buffer->rd_ptr(w);
		len -= w;
	}

	return 0;
}
