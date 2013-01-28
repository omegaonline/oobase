///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#ifndef OOSVRBASE_PROACTOR_POLL_H_INCLUDED_
#define OOSVRBASE_PROACTOR_POLL_H_INCLUDED_

#include "./ProactorPosix.h"

#if defined(HAVE_UNISTD_H) && !defined(_WIN32)

#include "../include/OOBase/HashTable.h"

#include <poll.h>

namespace OOBase
{
	namespace detail
	{
		class ProactorPoll : public ProactorPosix
		{
		public:
			ProactorPoll();
			virtual ~ProactorPoll();

			int init();

			int run(int& err, const Timeout& timeout = Timeout());

		private:
			struct FdItem
			{
				void*         m_param;
				fd_callback_t m_callback;
				size_t        m_poll_pos;
			};

			struct FdEvent
			{
				int          m_fd;
				FdItem*      m_item;
				unsigned int m_events;
			};

			Bag<pollfd>                   m_poll_fds;
			OOBase::HashTable<int,FdItem> m_items;

			int do_bind_fd(int fd, void* param, fd_callback_t callback);
			int do_unbind_fd(int fd);
			int do_watch_fd(int fd, unsigned int events);

			bool update_fds(FdEvent& active_fd, int poll_count, int& err);
		};
	}
}

#endif // defined(HAVE_UNISTD_H) && !defined(_WIN32)

#endif // OOSVRBASE_PROACTOR_POLL_H_INCLUDED_
