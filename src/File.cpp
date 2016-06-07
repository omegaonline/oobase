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
#include "../include/OOBase/Win32.h"

#if defined(HAVE_UNISTD_H)
#include <sys/stat.h>
#include <sys/mman.h>
#endif

namespace
{
	class FileMapping : public OOBase::detail::SharedCountBase
	{
	public:
		FileMapping(OOBase::File* file, void* map, size_t length) :
			OOBase::detail::SharedCountBase(), m_file(file), m_map(map), m_length(length)
		{
		}

		void dispose();
		void destroy();

	private:
		OOBase::File* m_file;
		void* m_map;
		size_t m_length;
	};

	OOBase::uint64_t page_size_mask()
	{
		static OOBase::uint64_t s_page_size_mask = 0;
		if (!s_page_size_mask)
		{
#if defined(_WIN32)
			SYSTEM_INFO si = {0};
			GetSystemInfo(&si);
			s_page_size_mask = ~(si.dwAllocationGranularity - 1);
#else
			s_page_size_mask = ~(sysconf(_SC_PAGE_SIZE) - 1);
#endif
		}
		return s_page_size_mask;
	}
}

void FileMapping::dispose()
{
	// Unmap the buffer
	m_file->unmap(m_map,m_length);
}

void FileMapping::destroy()
{
	OOBase::CrtAllocator::delete_free(this);
}

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
	ScopedArrayPtr<wchar_t> wname;
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

	m_fd = ::CreateFileW(wname.get(),dwAccess,FILE_SHARE_READ,NULL,dwDisp,FILE_ATTRIBUTE_NORMAL,NULL);
	if (!m_fd.valid())
		return ::GetLastError();

#elif defined(HAVE_UNISTD_H)

	int flags = O_RDONLY;
	if (writeable)
		flags = O_RDWR | O_CREAT;
		
	m_fd = POSIX::open(filename,flags,0664);
	if (!m_fd.valid())
		return errno;
#endif
	return 0;
}

size_t OOBase::File::read(void* p, size_t size)
{
#if defined(_WIN32)
	size_t total = 0;
	while (size)
	{
		DWORD r = 0;
		if (!::ReadFile(m_fd,p,static_cast<DWORD>(size),&r,NULL))
			return size_t(-1);
		total += r;
		if (r <= size)
			break;
		p = static_cast<uint8_t*>(p) + r;
		size -= r;
	}
	return total;
#elif defined(HAVE_UNISTD_H)
	return POSIX::read(m_fd,p,size);
#endif
}

size_t OOBase::File::write(const void* p, size_t size)
{
#if defined(_WIN32)
	size_t total = 0;
	while (size)
	{
		DWORD w = 0;
		if (!::WriteFile(m_fd,p,static_cast<DWORD>(size),&w,NULL))
			return size_t(-1);

		total += w;
		size -= w;
		p = static_cast<const uint8_t*>(p) + w;
	}
	return total;
#elif defined(HAVE_UNISTD_H)
	return POSIX::write(m_fd,p,size);
#endif
}

OOBase::uint64_t OOBase::File::seek(int64_t offset, enum seek_direction dir)
{
#if defined(_WIN32)
	DWORD d = FILE_CURRENT;
	switch (dir)
	{
	case seek_begin:
		d = FILE_BEGIN;
		break;

	case seek_current:
		break;

	case seek_end:
		d = FILE_END;
		break;
	}
	LARGE_INTEGER off,pos;
	off.QuadPart = offset;
	pos.QuadPart = 0;
	if (!::SetFilePointerEx(m_fd,off,&pos,d))
		return uint64_t(-1);
	return pos.QuadPart;
#else
	int d = SEEK_CUR;
	switch (dir)
	{
	case seek_begin:
		d = SEEK_SET;
		break;

	case seek_current:
		break;

	case seek_end:
		d = SEEK_END;
		break;
	}
	off_t s = ::lseek(m_fd,offset,d);
	if (s == off_t(-1))
		return uint64_t(-1);
	return s;
#endif
}

OOBase::uint64_t OOBase::File::tell() const
{
#if defined(_WIN32)
	LARGE_INTEGER off,pos;
	off.QuadPart = 0;
	pos.QuadPart = 0;
	if (!::SetFilePointerEx(m_fd,off,&pos,FILE_CURRENT))
		return uint64_t(-1);
	return pos.QuadPart;
#else
	off_t s = ::lseek(m_fd,0,SEEK_CUR);
	if (s == off_t(-1))
		return uint64_t(-1);
	return s;
#endif
}

OOBase::uint64_t OOBase::File::length() const
{
#if defined(_WIN32)
	LARGE_INTEGER li;
	li.QuadPart = 0;
	if (!GetFileSizeEx(m_fd,&li))
		return uint64_t(-1);

	return li.QuadPart;
#else
	struct stat s = {0};
	int err = ::fstat(m_fd,&s);
	if (err)
		return uint64_t(-1);

	return s.st_size;
#endif
}

void* OOBase::File::map(bool writeable, uint64_t offset, size_t& length)
{
	uint64_t file_len = this->length();
	if (offset >= file_len)
	{
#if defined(_WIN32)
		SetLastError(ERROR_INVALID_PARAMETER);
#else
		errno = EINVAL;
#endif
		return NULL;
	}

	if (length > file_len)
		length = file_len;

	if (file_len - offset < length)
		length = file_len - offset;

	uint64_t pa_offset = offset & page_size_mask();
	size_t lead = (offset - pa_offset);

#if defined(_WIN32)
	if (!m_mapping.valid())
	{
		m_mapping = ::CreateFileMapping(m_fd,NULL,writeable ? PAGE_READWRITE : PAGE_READONLY,0,0,NULL);
		if (!m_mapping.valid())
			return NULL;
	}

	 ULARGE_INTEGER ul;
	 ul.QuadPart = pa_offset;
	 void* p = ::MapViewOfFile(m_mapping,writeable ? FILE_MAP_WRITE : FILE_MAP_READ,ul.HighPart,ul.LowPart,length + lead);
	 if (!p)
		 return NULL;

#elif defined(HAVE_UNISTD_H)
	int prot = PROT_READ;
	if (writeable)
		prot |= PROT_WRITE;
	
	void* p = mmap(NULL,length + lead,prot,MAP_PRIVATE,m_fd,pa_offset);
	if (p == MAP_FAILED)
		return NULL;
#else
#error Implement memory mapping for your platform
#endif

	return static_cast<void*>(static_cast<char*>(p) + lead);
}

bool OOBase::File::unmap(void* p, size_t length)
{
	uint64_t u_p = reinterpret_cast<uint64_t>(p);
	uint64_t u_pa = u_p & page_size_mask();
	
#if defined(_WIN32)
	
	(void)length;
	return UnmapViewOfFile(reinterpret_cast<void*>(u_pa)) == TRUE;

#elif defined(HAVE_UNISTD_H)
	
	size_t lead = u_p - u_pa;
	return (munmap(reinterpret_cast<void*>(u_pa),length + lead) != -1);

#else
#error Implement memory mapping for your platform
#endif
}

OOBase::SharedPtr<char> OOBase::File::auto_map_i(bool writeable, uint64_t offset, size_t length)
{
	if (length == size_t(-1))
		length = this->length();

	OOBase::SharedPtr<char> ret;
	void* m = map(writeable,offset,length);
	if (m)
	{
		FileMapping* fm = OOBase::CrtAllocator::allocate_new<FileMapping>(this,m,length);
		if (fm)
		{
			ret = OOBase::make_shared(reinterpret_cast<char*>(m),fm);
			if (!ret)
				OOBase::CrtAllocator::delete_free(fm);
		}

		if (!ret)
			unmap(m,length);
	}

	return ret;
}
