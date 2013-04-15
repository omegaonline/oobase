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

#ifndef OOBASE_SERVICE_H_INCLUDED_
#define OOBASE_SERVICE_H_INCLUDED_

#include "Condition.h"
#include "String.h"

#if defined(HAVE_UNISTD_H)
#include <signal.h>
#endif

namespace OOBase
{
	class Server : public NonCopyable
	{
	public:
		Server();

		int wait_for_quit();
		void signal(int how);
		void quit();

		static int create_pid_file(const OOBase::LocalString& strPidFile, bool& already);
		static int daemonize(const OOBase::LocalString& strPidFile, bool& already);

	private:
#if defined(HAVE_UNISTD_H)
		sigset_t  m_set;
#endif

#if defined(HAVE_PTHREAD)
		pthread_t m_tid;
#endif
	};
}

#endif // OOBASE_SERVICE_H_INCLUDED_
