///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef OOSVRBASE_SERVICE_H_INCLUDED_
#define OOSVRBASE_SERVICE_H_INCLUDED_

#include "../OOBase/Condition.h"

#if defined(HAVE_UNISTD_H)
#include <signal.h>
#endif

namespace OOSvrBase
{
	class Server
	{
	public:
		Server();

		virtual int wait_for_quit();
		void signal(int how);
		void quit();

	private:
#if defined(HAVE_UNISTD_H)
		sigset_t m_set;
#endif
	};

#if defined(_WIN32)
	class Service : public Server
	{
	public:
		virtual int wait_for_quit();
	};
#else
	typedef Server Service;
#endif
}

#endif // OOSVRBASE_SERVICE_H_INCLUDED_
