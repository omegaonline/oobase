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
				OOBase::detail::ProactorPosix::fd_callback_t m_callback;
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
				OOBase::detail::ProactorPosix::timer_callback_t m_pfn;
			} m_timer_add_info;
			struct
			{
				void* m_param;
			} m_timer_remove_info;
		};
	};
}

OOBase::detail::ProactorPosix::ProactorPosix() :
		m_stopped(false),
		m_read_fd(-1),
		m_write_fd(-1)
{
}

OOBase::detail::ProactorPosix::~ProactorPosix()
{
	POSIX::close(m_write_fd);
	POSIX::close(m_read_fd);
}

int OOBase::detail::ProactorPosix::init()
{
	// Create the control pipe
	int pipe_ends[2] = { -1, -1 };
	if (pipe(pipe_ends) != 0)
		return errno;

	// Set non-blocking and close-on-exec
	int err = POSIX::set_non_blocking(pipe_ends[0],true);
	if (!err)
		err = POSIX::set_close_on_exec(pipe_ends[0],true);
	if (!err)
		err = POSIX::set_close_on_exec(pipe_ends[1],true);

	if (!err)
	{
		m_read_fd = pipe_ends[0];
		m_write_fd = pipe_ends[1];
	}
	else
	{
		POSIX::close(pipe_ends[0]);
		POSIX::close(pipe_ends[1]);
	}

	return err;
}

bool OOBase::detail::ProactorPosix::check_timers(TimerItem& active_timer, Timeout& timeout)
{
	if (!m_timers.empty())
	{
		// Sort the timer set, so soonest is last
		m_timers.sort();

		// Count the number of expired timers...
		size_t pos = m_timers.size() - 1;

		// Check last entry
		const TimerItem* item = m_timers.at(pos);

		// If the timer has expired, remove from set
		if (item->m_timeout.has_expired())
		{
			active_timer = *item;
			m_timers.remove_at(pos);
			return true;
		}

		// Set current timeout to the next value, as the set is sorted
		if (item->m_timeout < timeout)
			timeout = item->m_timeout;
	}

	return false;
}

int OOBase::detail::ProactorPosix::process_timer(const TimerItem& active_timer)
{
	Timeout new_timeout = (*active_timer.m_callback)(active_timer.m_param);

	int err = 0;
	if (!new_timeout.is_infinite())
		err = start_timer(active_timer.m_param,active_timer.m_callback,new_timeout);

	return err;
}

int OOBase::detail::ProactorPosix::start_timer(void* param, timer_callback_t callback, const Timeout& timeout)
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return add_timer(param,callback,timeout);

	Future<int> future(0);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTTimerAdd;
	msg.m_future = &future;
	msg.m_timer_add_info.m_param = param;
	msg.m_timer_add_info.m_pfn = callback;
	if (!timeout.get_timeval(msg.m_timer_add_info.m_timeval))
		return EINVAL;

	ssize_t sent = POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

    return future.wait(false);
}

int OOBase::detail::ProactorPosix::stop_timer(void* param)
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return remove_timer(param);

	Future<int> future(0);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTTimerRemove;
	msg.m_future = &future;
	msg.m_timer_remove_info.m_param = param;

	ssize_t sent = POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

void OOBase::detail::ProactorPosix::stop()
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		m_stopped = true;
	else
	{
		ControlMessage msg;
		memset(&msg,0,sizeof(msg));
		msg.m_type = eCTStop;
		msg.m_future = NULL;

		POSIX::write(m_write_fd,&msg,sizeof(msg));
	}
}

int OOBase::detail::ProactorPosix::restart()
{
	Guard<SpinLock> guard(m_lock);
	m_stopped = false;
	return 0;
}

int OOBase::detail::ProactorPosix::bind_fd(int fd, void* param, fd_callback_t callback)
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_bind_fd(fd,param,callback);

	Future<int> future;

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTBind;
	msg.m_future = &future;
	msg.m_bind_info.m_fd = fd;
	msg.m_bind_info.m_param = param;
	msg.m_bind_info.m_callback = callback;

	ssize_t sent = POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

int OOBase::detail::ProactorPosix::watch_fd(int fd, unsigned int events)
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_watch_fd(fd,events);

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTWatch;
	msg.m_future = NULL;
	msg.m_watch_info.m_fd = fd;
	msg.m_watch_info.m_events = events;

	ssize_t sent = POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return 0;
}

int OOBase::detail::ProactorPosix::unbind_fd(int fd)
{
	Guard<SpinLock> guard(m_lock,false);
	if (guard.try_acquire())
		return do_unbind_fd(fd);

	Future<int> future;

	ControlMessage msg;
	memset(&msg,0,sizeof(msg));
	msg.m_type = eCTUnbind;
	msg.m_future = &future;
	msg.m_unbind_info.m_fd = fd;

	ssize_t sent = POSIX::write(m_write_fd,&msg,sizeof(msg));
	if (sent == -1)
		return errno;
	else if (sent != sizeof(msg))
		return EIO;

	return future.wait(false);
}

int OOBase::detail::ProactorPosix::add_timer(void* param, timer_callback_t callback, const Timeout& timeout)
{
	TimerItem ti;
	ti.m_param = param;
	ti.m_callback = callback;
	ti.m_timeout = timeout;
	return m_timers.insert(ti);
}

int OOBase::detail::ProactorPosix::remove_timer(void* param)
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

int OOBase::detail::ProactorPosix::read_control()
{
	for (;;)
	{
		ControlMessage msg;
		memset(&msg, 0, sizeof(msg));
		ssize_t recv = POSIX::read(m_read_fd, &msg, sizeof(msg));
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
			err = add_timer(msg.m_timer_add_info.m_param, msg.m_timer_add_info.m_pfn, Timeout(msg.m_timer_add_info.m_timeval.tv_sec, msg.m_timer_add_info.m_timeval.tv_usec));
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

void* OOBase::detail::ProactorPosix::allocate(size_t bytes, size_t align)
{
	return OOBase::CrtAllocator::allocate(bytes,align);
}

void OOBase::detail::ProactorPosix::free(void* /*param*/, void* ptr)
{
	OOBase::CrtAllocator::free(ptr);
}

#endif // defined(HAVE_UNISTD_H)
