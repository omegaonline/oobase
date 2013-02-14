///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor, Jamal Natour
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

#include "../include/OOBase/Posix.h"
#include "../include/OOBase/String.h"

#if defined (HAVE_UNISTD_H)

#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>

int OOBase::POSIX::open(const char *pathname, int flags, mode_t mode)
{
#if defined(O_CLOEXEC)
	flags |= O_CLOEXEC;
#endif

	int fd = -1;
	do
	{
		fd = ::open(pathname,flags,mode);
	}
	while (fd == -1 && errno == EINTR);

#if !defined(O_CLOEXEC)
	if (fd != -1)
	{
		int err = POSIX::set_close_on_exec(fd,true);
		if (err)
		{
			POSIX::close(fd);
			fd = -1;
			return err;
		}
	}
#endif

	return fd;
}

ssize_t OOBase::POSIX::read(int fd, void* buf, size_t count)
{
	ssize_t r = -1;
	do
	{
		r = ::read(fd,buf,count);
	}
	while (r == -1 && errno == EINTR);

	return r;
}

ssize_t OOBase::POSIX::write(int fd, const void* buf, size_t count)
{
	ssize_t w = -1;
	do
	{
		w = ::write(fd,buf,count);
	}
	while (w == -1 && errno == EINTR);

	return w;
}

int OOBase::POSIX::close(int fd)
{
	// Don't attempt to spin on EINTR, as the handle might be closed and reused before we call close again
	if (::close(fd) == -1 && errno != EINTR)
		return -1;

	return 0;
}

pid_t OOBase::POSIX::waitpid(pid_t pid, int* status, int options)
{
	pid_t ret = -1;
	do
	{
		ret = ::waitpid(pid,status,options);
	}
	while (ret == -1 && errno == EINTR);

	return ret;
}

int OOBase::POSIX::set_non_blocking(int sock, bool set)
{
	int flags = fcntl(sock,F_GETFL);
	if (flags == -1)
		return errno;

	flags = (set ? flags | O_NONBLOCK : flags & ~O_NONBLOCK);
	if (fcntl(sock,F_SETFL,flags) == -1)
		return errno;

	return 0;
}

int OOBase::POSIX::get_non_blocking(int sock, bool& set)
{
	int flags = fcntl(sock,F_GETFL);
	if (flags == -1)
		return errno;

	set = ((flags & O_NONBLOCK) != 0);
	return 0;
}

int OOBase::POSIX::set_close_on_exec(int sock, bool set)
{
	int flags = fcntl(sock,F_GETFD);
	if (flags == -1)
		return errno;

	flags = (set ? flags | FD_CLOEXEC : flags & ~FD_CLOEXEC);
	if (fcntl(sock,F_GETFD,flags) == -1)
		return errno;

	return 0;
}

int OOBase::POSIX::close_file_descriptors(int* except, size_t ex_count)
{
	int mx = getdtablesize();
	if (mx == -1)
	{
#if defined(_POSIX_OPEN_MAX) && (_POSIX_OPEN_MAX >= 0)
#if (_POSIX_OPEN_MAX == 0)
		/* value available at runtime only */
		mx = sysconf(_SC_OPEN_MAX);
#else
		/* value available at compile time */
		mx = _POSIX_OPEN_MAX;
#endif
#endif
	}

	if (mx > 0)
	{
		for (int fd = 0; fd < mx; ++fd)
		{
			bool do_close = true;
			for (size_t e = 0; e < ex_count; ++e)
			{
				if (fd == except[e])
				{
					do_close = false;
					break;
				}
			}

			if (do_close && POSIX::close(fd) != 0 && errno != EBADF)
				return errno;
		}
	}
	else
	{
		/* based on lsof style walk of proc filesystem so should
		 * work on anything with a proc filesystem i.e. a OSx/BSD */
		/* walk proc, closing all descriptors from stderr onwards for our pid */

		StackAllocator<128> allocator;
		TempPtr<char> str(allocator);
		int err = OOBase::temp_printf(str,"/proc/%u/fd/",getpid());
		if (err != 0)
			return err;

		DIR* pdir = opendir(str);
		if (!pdir)
			return errno;

		/* skips ./ and ../ entries in addition to skipping to the passed fd offset */
		for (dirent* pfile; (pfile = readdir(pdir));)
		{
			if (*pfile->d_name != '.')
			{
				int fd = atoi(pfile->d_name);
				bool do_close = true;
				for (size_t e = 0; e < ex_count; ++e)
				{
					if (fd == except[e])
					{
						do_close = false;
						break;
					}
				}

				if (do_close && POSIX::close(fd) != 0 && errno != EBADF)
					return errno;
			}
		}

		closedir(pdir);
	}

	return 0;
}

int OOBase::POSIX::random_bytes(void* buffer, size_t len)
{
	OOBase::POSIX::SmartFD fd(OOBase::POSIX::open("/dev/urandom",O_RDONLY));
	if (!fd.is_valid())
		fd = OOBase::POSIX::open("/dev/random",O_RDONLY);

	if (!fd.is_valid())
		return errno;

	ssize_t r = OOBase::POSIX::read(fd,buffer,len);
	if (r == -1)
		return errno;

	if (static_cast<size_t>(r) != len)
		return EIO;

	return 0;
}

int OOBase::POSIX::random_chars(char* buffer, size_t len)
{
	static const char c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYUZabcdefghijklmnopqrstuvwxyz0123456789";

	if (len)
		buffer[--len] = '\0';

	while (len)
	{
		unsigned char buf[128] = {0};
		int err = random_bytes(buf,sizeof(buf));
		if (err)
			return err;

		for (size_t i = 0;i < sizeof(buf) && len;)
			buffer[--len] = c[buf[i++] % (sizeof(c)-1)];
	}

	return 0;
}

OOBase::POSIX::pw_info::pw_info(AllocatorInstance& allocator, uid_t uid) : m_pwd(NULL), m_data(allocator)
{
	size_t size = get_size();
	if (m_data.reallocate(size))
		::getpwuid_r(uid,&m_pwd2,m_data,size,&m_pwd);
}

OOBase::POSIX::pw_info::pw_info(AllocatorInstance& allocator, const char* uname) : m_pwd(NULL), m_data(allocator)
{
	size_t size = get_size();
	if (m_data.reallocate(size))
		::getpwnam_r(uname,&m_pwd2,m_data,size,&m_pwd);
}

const size_t OOBase::POSIX::pw_info::get_size()
{
#if defined(_SC_GETPW_R_SIZE_MAX)
	long buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
#else
	long buf_len = 0;
#endif

	// _SC_GETPW_R_SIZE_MAX is defined on Mac OS X. However,
	// sysconf(_SC_GETPW_R_SIZE_MAX) returns an error. Therefore, the
	// constant is used as below when error was returned.
	return (buf_len <= 0 ? 1024 : buf_len);
}

#endif
