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

#include "SmartPtr.h"

#if defined(__unix__) || defined(DOXYGEN)

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

		int random_bytes(void* buffer, size_t len);
		int random_chars(char* buffer, size_t len);

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
				if (!is_valid())
					return 0;

				int err = POSIX::close(m_fd);
				if (!err)
					m_fd = -1;
				return err;
			}

			bool is_valid() const
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

		class pw_info : public NonCopyable
		{
		public:
			pw_info(AllocatorInstance& allocator, uid_t uid);
			pw_info(AllocatorInstance& allocator, const char* uname);

			inline struct passwd* operator ->()
			{
				return m_pwd;
			}

			inline bool operator !() const
			{
				return (m_pwd == NULL);
			}

		private:
			static const size_t get_size();

			struct passwd*    m_pwd;
			struct passwd     m_pwd2;
			TempPtr<char>     m_data;
		};
	}
}

#endif

#endif // OOBASE_POSIX_H_INCLUDED_
