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

#include "../config-base.h"
#include "SmartPtr.h"

#if defined(HAVE_UNISTD_H) || defined(DOXYGEN)

#include <pwd.h>

namespace OOBase
{
	namespace POSIX
	{
		int set_close_on_exec(int fd, bool set);

		class pw_info
		{
		public:
			pw_info(uid_t uid);
			pw_info(const char* uname);

			inline struct passwd* operator ->()
			{
				return m_pwd;
			}

			inline bool operator !() const
			{
				return (m_pwd == NULL);
			}

		private:
			pw_info() {};

			struct passwd* m_pwd;
			struct passwd  m_pwd2;
			size_t         m_buf_len;

			OOBase::SmartPtr<char,OOBase::HeapDestructor> m_buffer;
		};
	}
}

#endif

#endif // OOBASE_POSIX_H_INCLUDED_
