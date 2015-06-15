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

#include "../include/OOBase/Posix.h"

#include "ProactorPoll.h"
#include "BSDSocket.h"

#if defined(HAVE_UNISTD_H)

#include <sys/resource.h>

#if !defined(POLLRDHUP)
#define POLLRDHUP 0
#endif

OOBase::Proactor* OOBase::Proactor::create(int& err)
{
	detail::ProactorPoll* proactor = NULL;
	if (!OOBase::CrtAllocator::allocate_new(proactor))
		err = ERROR_OUTOFMEMORY;
	else
	{
		err = proactor->init();
		if (err)
		{
			OOBase::CrtAllocator::delete_free(proactor);
			proactor = NULL;
		}
	}
	return proactor;
}

void OOBase::Proactor::destroy(Proactor* proactor)
{
	if (proactor)
	{
		proactor->stop();
		OOBase::CrtAllocator::delete_free(static_cast<detail::ProactorPoll*>(proactor));
	}
}

OOBase::detail::ProactorPoll::ProactorPoll() :
		ProactorPosix(),
		m_poll_fds(m_allocator),
		m_items(m_allocator)
{
}

OOBase::detail::ProactorPoll::~ProactorPoll()
{
}

int OOBase::detail::ProactorPoll::init()
{
	int err = ProactorPosix::init();
	if (err)
		return err;

	// Add the control pipe to m_poll_fds
	pollfd pfd = { m_read_fd, POLLIN | POLLRDHUP, 0 };
	return m_poll_fds.push_back(pfd) ? 0 : ERROR_OUTOFMEMORY;
}

bool OOBase::detail::ProactorPoll::do_bind_fd(int fd, void* param, fd_callback_t callback)
{
	FdItem item = { param, callback, size_t(-1) };
	return m_items.insert(fd,item);
}

bool OOBase::detail::ProactorPoll::do_unbind_fd(int fd)
{
	OOBase::HashTable<int,FdItem,AllocatorInstance>::iterator i = m_items.find(fd);
	if (!i)
		return false;

	FdItem item = i->second;
	m_items.erase(i);

	if (item.m_poll_pos != size_t(-1))
	{
		// Replace current with top of stack
		pollfd* pfd = m_poll_fds.at(item.m_poll_pos);

		*pfd = m_poll_fds.back();
		m_poll_fds.pop_back();

		// Reassign index
		i = m_items.find(pfd->fd);
		if (i)
			i->second.m_poll_pos = item.m_poll_pos;
	}

	return true;
}

bool OOBase::detail::ProactorPoll::do_watch_fd(int fd, unsigned int events)
{
	OOBase::HashTable<int,FdItem,AllocatorInstance>::iterator i = m_items.find(fd);
	if (i)
	{
		short p_events = 0;
		if (events & eTXSend)
			p_events |= POLLOUT;

		if (events & eTXRecv)
			p_events |= (POLLIN | POLLRDHUP);

		if (p_events)
		{
			// See if fd already exists in the poll stack
			if (i->second.m_poll_pos != size_t(-1))
				m_poll_fds.at(i->second.m_poll_pos)->events |= p_events;
			else
			{
				// A new entry
				pollfd pfd = { fd, p_events, 0 };
				if (!m_poll_fds.push_back(pfd))
					return false;

				i->second.m_poll_pos = m_poll_fds.size()-1;
			}
		}
	}
	return true;
}

bool OOBase::detail::ProactorPoll::update_fds(FdEvent& active_fd, int poll_count, int& err)
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
			if (pfd->fd == m_read_fd)
			{
				err = read_control();
				if (err)
					return false;

				continue;
			}

			// Find the corresponding FdItem
			OOBase::HashTable<int,FdItem,AllocatorInstance>::iterator i = m_items.find(pfd->fd);
			if (i)
			{
				active_fd.m_fd = pfd->fd;
				active_fd.m_param = i->second.m_param;
				active_fd.m_callback = i->second.m_callback;
				active_fd.m_events = 0;

				// If we have errored, favour eTXRecv...
				if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL))
				{
					if (pfd->events & (POLLIN | POLLRDHUP))
						active_fd.m_events |= eTXRecv;

					if (pfd->events & POLLOUT)
						active_fd.m_events |= eTXSend;

					pfd->events = 0;
				}
				else
				{
					if ((pfd->events & (POLLIN | POLLRDHUP)) && (pfd->revents & (POLLIN | POLLRDHUP)))
					{
						// We have a viable eTXRecv, clear (POLLIN | POLLRDHUP)
						pfd->events &= ~(POLLIN | POLLRDHUP);
						active_fd.m_events |= eTXRecv;
					}

					if ((pfd->events & POLLOUT) && (pfd->revents & POLLOUT))
					{
						// We have a viable eTXSend, clear POLLOUT
						pfd->events &= ~POLLOUT;
						active_fd.m_events |= eTXSend;
					}
				}

				if (active_fd.m_events)
				{
					// We have an event
					if (!pfd->events)
					{
						// We are now longer in m_poll_fds
						i->second.m_poll_pos = size_t(-1);

						// If we have no pending events, remove from m_poll_fds
						if (pos == m_poll_fds.size()-1)
							m_poll_fds.pop_back();
						else
						{
							// Replace current with top of stack
							*pfd = m_poll_fds.back();
							m_poll_fds.pop_back();

							// Reassign index
							i = m_items.find(pfd->fd);
							if (i)
								i->second.m_poll_pos = pos;
						}
					}

					return true;
				}
			}
		}
	}

    return false;
}

int OOBase::detail::ProactorPoll::run(int& err, const Timeout& timeout)
{
	Guard<Mutex> guard(m_lock);

	while (!m_stopped && !timeout.has_expired())
	{
		TimerItem active_timer;
		FdEvent active_fd;
		bool fd_event = false;

		// Check timers and update timeout
		Timeout local_timeout(timeout);
		bool timer_event = check_timers(active_timer,local_timeout);
		if (!timer_event)
		{
			// If no timers have expired, poll for I/O
			int count = ::poll(m_poll_fds.at(0),m_poll_fds.size(),local_timeout.millisecs());
			if (count == -1)
			{
				if (errno == EINVAL)
				{
					rlimit rl = {0};
					if (getrlimit(RLIMIT_NOFILE,&rl) == -1)
						err = errno;
					else
					{
						count = ::poll(m_poll_fds.at(0),rl.rlim_cur,local_timeout.millisecs());
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
				timer_event = check_timers(active_timer,local_timeout);
			}
			else
			{
				// Socket I/O occurred
				fd_event = update_fds(active_fd,count,err);
				if (err)
					return -1;
			}
		}

		// Process any timers or I/O
		if (timer_event || fd_event)
		{
			guard.release();

			if (timer_event)
			{
				err = process_timer(active_timer);
				if (err)
					return -1;
			}
			else
			{
				(*active_fd.m_callback)(active_fd.m_fd,active_fd.m_param,active_fd.m_events);
			}

			guard.acquire();

			// Always check the control pipe if we have done something
			err = read_control();
			if (err)
				return -1;
		}
	}

    if (err)
    	return -1;

	return (timeout.has_expired() ? 0 : 1);
}

#endif
