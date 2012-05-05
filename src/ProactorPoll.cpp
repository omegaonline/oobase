///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#include "../include/OOBase/GlobalNew.h"
#include "./ProactorPoll.h"

#if defined(HAVE_UNISTD_H)

#include "../include/OOBase/Posix.h"

#include "./BSDSocket.h"

#include <sys/resource.h>

#if !defined(POLLRDHUP)
#define POLLRDHUP 0
#endif

OOSvrBase::Proactor* OOSvrBase::Proactor::create(int& err)
{
	detail::ProactorPoll* proactor = new (std::nothrow) detail::ProactorPoll();
	if (!proactor)
		err = ERROR_OUTOFMEMORY;
	else
	{
		err = proactor->init();
		if (err)
		{
			delete proactor;
			proactor = NULL;
		}
	}
	return proactor;
}

void OOSvrBase::Proactor::destroy(Proactor* proactor)
{
	if (proactor)
	{
		proactor->stop();
		delete proactor;
	}
}

OOSvrBase::detail::ProactorPoll::ProactorPoll() : ProactorPosix()
{
}

OOSvrBase::detail::ProactorPoll::~ProactorPoll()
{
}

int OOSvrBase::detail::ProactorPoll::init()
{
	int err = ProactorPosix::init();
	if (err)
		return err;

	// Add the control pipe to m_poll_fds
	pollfd pfd = { get_control_fd(), POLLIN | POLLRDHUP, 0 };
	return m_poll_fds.push(pfd);
}

int OOSvrBase::detail::ProactorPoll::do_bind_fd(int fd, void* param, fd_callback_t callback)
{
	FdItem item = { param, callback, size_t(-1) };
	return m_items.insert(fd,item);
}

int OOSvrBase::detail::ProactorPoll::do_unbind_fd(int fd)
{
	FdItem item;
	if (!m_items.remove(fd,&item))
		return ENOENT;

	if (item.m_poll_pos != size_t(-1))
		m_poll_fds.remove_at(item.m_poll_pos);

	return 0;
}

int OOSvrBase::detail::ProactorPoll::do_watch_fd(int fd, unsigned int events)
{
	FdItem* item = m_items.find(fd);
	if (item)
	{
		short p_events = 0;
		if (events & eTXSend)
			p_events |= POLLOUT;

		if (events & eTXRecv)
			p_events |= (POLLIN | POLLRDHUP);

		if (p_events)
		{
			// See if fd already exists in the poll stack
			if (item->m_poll_pos != size_t(-1))
				m_poll_fds.at(item->m_poll_pos)->events |= p_events;
			else
			{
				// A new entry
				pollfd pfd = { fd, p_events, 0 };
				int err = m_poll_fds.push(pfd);
				if (err)
					return err;

				item->m_poll_pos = m_poll_fds.size()-1;
			}
		}
	}
	return 0;
}

int OOSvrBase::detail::ProactorPoll::update_fds(OOBase::Stack<FdEvent,OOBase::LocalAllocator>& fd_set, int poll_count)
{
	// Check each pollfd in m_poll_fds
	for (size_t pos = 0; poll_count > 0 && pos < m_poll_fds.size(); ++pos)
	{
		pollfd* pfd = m_poll_fds.at(pos);
		if (pfd->revents)
		{
			// Make sure we don't scan the entire table...
			--poll_count;

			// Handle the control pipe first
			if (pfd->fd == get_control_fd())
			{
				int err = read_control();
				if (err)
					return err;

				continue;
			}

			// Find the corresponding FdItem
			FdItem* item = m_items.find(pfd->fd);
			if (item)
			{
				FdEvent fd_event = { pfd->fd, item, 0 };

				// If we have errored, favour eTXRecv...
				if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					if (pfd->events & (POLLIN | POLLRDHUP))
						fd_event.m_events |= eTXRecv;

					if (pfd->events & POLLOUT)
						fd_event.m_events |= eTXSend;

					pfd->events = 0;
				}
				else
				{
					if ((pfd->events & POLLIN) && (pfd->revents & (POLLIN | POLLRDHUP)))
					{
						// We have a viable eTXRecv, clear (POLLIN | POLLRDHUP)
						pfd->events &= ~(POLLIN | POLLRDHUP);
						fd_event.m_events |= eTXRecv;
					}

					if ((pfd->events & POLLOUT) && (pfd->revents & POLLOUT))
					{
						// We have a viable eTXSend, clear POLLOUT
						pfd->events &= ~POLLOUT;
						fd_event.m_events |= eTXSend;
					}
				}

				if (fd_event.m_events)
				{
					// Add to socket set
					int err = fd_set.push(fd_event);
					if (err)
						return err;
				}

				if (!pfd->events)
				{
					// We are now longer in m_poll_fds
					item->m_poll_pos = size_t(-1);

					// If we have no pending events, remove from m_poll_fds
					if (pos == m_poll_fds.size()-1)
						m_poll_fds.pop();
					else
					{
						// Replace current with top of stack
						m_poll_fds.pop(pfd);

						// Reassign index
						item = m_items.find(pfd->fd);
						if (item)
							item->m_poll_pos = pos;

						// And do this item again
						--pos;
					}
				}
			}
		}
	}

    return 0;
}

int OOSvrBase::detail::ProactorPoll::process_socketset(OOBase::Stack<FdEvent,OOBase::LocalAllocator>& fd_set)
{
	FdEvent item;
	while (fd_set.pop(&item))
		(*item.m_item->m_callback)(item.m_fd,item.m_item->m_param,item.m_events);

	return 0;
}

int OOSvrBase::detail::ProactorPoll::run(int& err, const OOBase::Timeout& timeout)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (!guard.acquire(timeout))
		return 0;

	OOBase::Stack<TimerItem,OOBase::LocalAllocator> timer_set;
	OOBase::Stack<FdEvent,OOBase::LocalAllocator> fd_set;

    do
	{
		// Check timers and update timeout
		OOBase::Timeout local_timeout(timeout);
		err = update_timers(timer_set,local_timeout);
		if (err)
			break;

		if (timer_set.empty())
		{
			// If no timers have expired, poll for I/O
			int count = poll(m_poll_fds.at(0),m_poll_fds.size(),local_timeout.millisecs());
			if (count == -1)
			{
				if (errno == EINVAL)
				{
					rlimit rl = {0};
					if (getrlimit(RLIMIT_NOFILE,&rl) == -1)
						err = errno;
					else
					{
						count = poll(m_poll_fds.at(0),rl.rlim_cur,local_timeout.millisecs());
						if (count == -1)
							err = errno;
					}
				}
				else
					err = errno;

				if (err)
					break;
			}

			if (count == 0)
			{
				// Poll timed out
				err = update_timers(timer_set,local_timeout);
				if (err)
					break;
			}
			else
			{
				// Socket I/O occurred
				err = update_fds(fd_set,count);
				if (err)
					break;
			}
		}

		// Process any timers or I/O
		if (!timer_set.empty() || !fd_set.empty())
		{
			guard.release();

			err = process_timerset(timer_set);
			if (err)
				return -1;

			err = process_socketset(fd_set);
			if (err)
				return -1;

			if (!guard.acquire(timeout))
				return 0;
		}

	} while (!stopped() && !timeout.has_expired());

    if (err)
    	return -1;

	return (stopped() ? 1 : 0);
}

#endif
