///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#ifndef OOBASE_POSIX_H_INCLUDED_
#define OOBASE_POSIX_H_INCLUDED_

#include "ScopedArrayPtr.h"

#if defined(HAVE_UNISTD_H) || defined(DOXYGEN)

#include <pwd.h>
#include <fcntl.h>

namespace OOBase
{
	namespace POSIX
	{
		int set_non_blocking(int fd, bool set);
		int get_non_blocking(int fd, bool& set);

		int set_close_on_exec(int fd, bool set);
		int close_file_descriptors(int* except, size_t ex_count);

		int open(const char *pathname, int flags, mode_t mode = S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
		ssize_t read(int fd, void* buf, size_t count);
		ssize_t write(int fd, const void* buf, size_t count);
		int close(int fd);
		pid_t waitpid(pid_t pid, int* status, int options);

		class SmartFD : public NonCopyable
		{
		public:
			SmartFD(int fd = -1) :
					m_fd(fd)
			{}

			SmartFD& operator = (int fd)
			{
				m_fd = fd;
				return *this;
			}

			~SmartFD()
			{
				close();
			}

			int detach()
			{
				int fd = m_fd;
				m_fd = -1;
				return fd;
			}

			int close()
			{
				if (!valid())
					return 0;

				int err = POSIX::close(m_fd);
				if (!err)
					m_fd = -1;
				return err;
			}

			bool valid() const
			{
				return (m_fd != -1);
			}

			int* operator &()
			{
				return &m_fd;
			}

			operator int() const
			{
				return m_fd;
			}

			operator int&()
			{
				return m_fd;
			}

		private:
			int m_fd;
		};

		int socketpair(int type, SmartFD fds[2]);

		class pw_info : public NonCopyable, public SafeBoolean
		{
		public:
			pw_info(uid_t uid);
			pw_info(const char* uname);

			const struct passwd* operator ->() const
			{
				return m_pwd;
			}

			const struct passwd& operator *() const
			{
				return *m_pwd;
			}

			operator bool_type() const
			{
				return SafeBoolean::safe_bool(m_pwd != NULL);
			}

		private:
			static size_t get_size();

			struct passwd*    m_pwd;
			struct passwd     m_pwd2;

			ScopedArrayPtr<char,CrtAllocator,1024> m_data;
		};
	}
}

#endif

#endif // OOBASE_POSIX_H_INCLUDED_
