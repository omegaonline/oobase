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

#include "./ProactorPosix.h"

#if defined(HAVE_UNISTD_H)

#include "../include/OOBase/Posix.h"

#include "./BSDSocket.h"

namespace
{
	enum ControlType
	{
		eCTStop,
		eCTBind,
		eCTWatch,
		eCTUnbind,
		eCTTimerAdd,
		eCTTimerRemove
	};

	struct ControlMessage
	{
		enum ControlType     m_type;
		OOBase::Future<int>* m_future;
		union
		{
			struct
			{
				int   m_fd;
				void* m_param;
				OOSvrBase::detail::ProactorPosix::fd_callback_t m_callback;
			} m_bind_info;
			struct
			{
				int          m_fd;
				unsigned int m_events;
			} m_watch_info;
			struct
			{
				int m_fd;
			} m_unbind_info;
			struct
			{
				void*     m_param;
				::timeval m_timeval;
				OOSvrBase::detail::ProactorPosix::timer_callback_t m_pfn;
			} m_timer_add_info;
			struct
			{
				void* m_param;
			} m_timer_remove_info;
		};
	};
}

OOSvrBase::detail::ProactorPosix::ProactorPosix() :
		m_stopped(false),
		m_read_fd(-1),
		m_write_fd(-1)
{
}

OOSvrBase::detail::ProactorPosix::~ProactorPosix()
{
	OOBase::POSIX::close(m_write_fd);
	OOBase::POSIX::close(m_read_fd);
}

int OOSvrBase::detail::ProactorPosix::init()
{
	// Create the control pipe
	int pipe_ends[2] = { -1, -1 };
	if (pipe(pipe_ends) != 0)
		return errno;

	// Set non-blocking and close-on-exec
	int err = OOBase::POSIX::set_non_blocking(pipe_ends[0],true);
	if (!err)
		err = OOBase::POSIX::set_close_on_exec(pipe_ends[0],true);
	if (!err)
		err = OOBase::POSIX::set_close_on_exec(pipe_ends[1],true);

	if (!err)
	{
		m_read_fd = pipe_ends[0];
		m_write_fd = pipe_ends[1];
	}
	else
	{
		OOBase::POSIX::close(pipe_ends[0]);
		OOBase::POSIX::close(pipe_ends[1]);
	}

	return err;
}

int OOSvrBase::detail::ProactorPosix::update_timers(OOBase::Stack<TimerItem,OOBase::LocalAllocator>& timer_set, OOBase::Timeout& timeout)
{
	// Sort the timer set, so soonest is last
	m_timers.sort();

    // Count the number of expired timers...
	for (size_t pos = m_timers.size();pos != 0; --pos)
	{
		TimerItem* item = m_timers.at(pos-1);

		// If the timer has expired, remove from set, add to stack
		if (item->m_timeout.has_expired())
		{
			int err = timer_set.push(*item);
			if (!err)
				m_timers.remove_at(pos);

			if (err)
				return err;
		}
		else
		{
			// Set current timeout to the next value, as the set is sorted
			if (item->m_timeout < timeout)
				timeout = item->m_timeout;

			break;
		}
	}

	return 0;
}

int OOSvrBase::detail::ProactorPosix::process_timerset(OOBase::Stack<TimerItem,OOBase::LocalAllocator>& timer_set)
{
	TimerItem item;
	while (timer_set.pop(&item))
	{
		OOBase::Timeout new_timeout = (*item.m_callback)(item.m_param);
		if (!new_timeout.is_infinite())
		{
			int err = start_timer(item.m_param,item.m_callback,new_timeout);
			if (err)
				return err;
		}
	}
	return 0;
}

int OOSvrBase::detail::ProactorPosix::start_timer(void* param, timer_callback_t callback, const OOBase::Timeout& timeout)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return add_timer(param,callback,timeout);

	OOBase::Future<int> future(0);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTTimerAdd;
	msg.m_future = &future;
	msg.m_timer_add_info.m_param = param;
	msg.m_timer_add_info.m_pfn = callback;
	if (!timeout.get_timeval(msg.m_timer_add_info.m_timeval))
		return EINVAL;

	ssize_t sent = OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

    return future.wait(false);
}

int OOSvrBase::detail::ProactorPosix::stop_timer(void* param)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return remove_timer(param);

	OOBase::Future<int> future(0);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTTimerRemove;
	msg.m_future = &future;
	msg.m_timer_remove_info.m_param = param;

	ssize_t sent = OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

void OOSvrBase::detail::ProactorPosix::stop()
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		m_stopped = true;
	else
	{
		ControlMessage msg;
		memset(&msg,0,sizeof(msg));
		msg.m_type = eCTStop;
		msg.m_future = NULL;

		OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	}
}

int OOSvrBase::detail::ProactorPosix::bind_fd(int fd, void* param, fd_callback_t callback)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_bind_fd(fd,param,callback);

	OOBase::Future<int> future;

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTBind;
	msg.m_future = &future;
	msg.m_bind_info.m_fd = fd;
	msg.m_bind_info.m_param = param;
	msg.m_bind_info.m_callback = callback;

	ssize_t sent = OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

int OOSvrBase::detail::ProactorPosix::watch_fd(int fd, unsigned int events)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_watch_fd(fd,events);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTWatch;
	msg.m_future = NULL;
	msg.m_watch_info.m_fd = fd;
	msg.m_watch_info.m_events = events;

	ssize_t sent = OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return 0;
}

int OOSvrBase::detail::ProactorPosix::unbind_fd(int fd)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_unbind_fd(fd);

	OOBase::Future<int> future;

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTUnbind;
	msg.m_future = &future;
	msg.m_unbind_info.m_fd = fd;

	ssize_t sent = OOBase::POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

int OOSvrBase::detail::ProactorPosix::add_timer(void* param, timer_callback_t callback, const OOBase::Timeout& timeout)
{
	TimerItem ti;
	ti.m_param = param;
	ti.m_callback = callback;
	ti.m_timeout = timeout;
	return m_timers.insert(ti);
}

int OOSvrBase::detail::ProactorPosix::remove_timer(void* param)
{
	for (size_t pos = 0; pos < m_timers.size(); ++pos)
	{
		if (m_timers.at(pos)->m_param == param)
		{
			m_timers.remove_at(pos);
			return 0;
		}
	}
	return ENOENT;
}

int OOSvrBase::detail::ProactorPosix::read_control()
{
	for (;;)
	{
		ControlMessage msg;
		memset(&msg, 0, sizeof(msg));
		ssize_t recv = OOBase::POSIX::read(m_read_fd, &msg, sizeof(msg));
		if (recv == -1)
		{
			if (errno == EWOULDBLOCK || errno == EAGAIN)
				return 0;

			return errno;
		}

		if (recv != sizeof(msg))
			return EPIPE;

		int err = 0;
		switch (msg.m_type)
		{
		case eCTStop:
			m_stopped = true;
			break;

		case eCTBind:
			err = do_bind_fd(msg.m_bind_info.m_fd, msg.m_bind_info.m_param, msg.m_bind_info.m_callback);
			break;

		case eCTWatch:
			err = do_watch_fd(msg.m_watch_info.m_fd, msg.m_watch_info.m_events);
			break;

		case eCTUnbind:
			err = do_unbind_fd(msg.m_unbind_info.m_fd);
			break;

		case eCTTimerAdd:
			err = add_timer(msg.m_timer_add_info.m_param, msg.m_timer_add_info.m_pfn, OOBase::Timeout(msg.m_timer_add_info.m_timeval.tv_sec, msg.m_timer_add_info.m_timeval.tv_usec));
			break;

		case eCTTimerRemove:
			err = remove_timer(msg.m_timer_remove_info.m_param);
			break;

		default:
			err = EINVAL;
			break;
		}

		if (msg.m_future)
			msg.m_future->signal(err);
	}
}

#endif // defined(HAVE_UNISTD_H)
