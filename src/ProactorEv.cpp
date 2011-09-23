///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#include "ProactorEv.h"

#if defined(HAVE_EV_H) && !defined(_WIN32)

#include "../include/OOBase/Posix.h"
#include "./BSDSocket.h"

OOSvrBase::detail::ProactorEv::ProactorEv() : 
		Proactor(false),
		m_pLoop(NULL)
{ 
	m_pipe_fds[0] = -1;
	m_pipe_fds[1] = -1;
}

OOSvrBase::detail::ProactorEv::~ProactorEv()
{
	close(m_pipe_fds[0]);
	close(m_pipe_fds[1]);
	
	if (m_pLoop)
	{
		ev_io_stop(m_pLoop,&m_pipe_watcher);
		ev_loop_destroy(m_pLoop);
	}	
}

int OOSvrBase::detail::ProactorEv::init()
{
	// Early escape...
	if (m_pLoop)
		return 0;
	
	// Create a pipe
	if (pipe(m_pipe_fds) != 0)
		return errno;
	
	// Set non-blocking and close-on-exec
	int err = OOBase::BSD::set_non_blocking(m_pipe_fds[0],true);
	if (err == 0)
		err = OOBase::POSIX::set_close_on_exec(m_pipe_fds[0],true);
	if (err == 0)
		err = OOBase::POSIX::set_close_on_exec(m_pipe_fds[1],true);
	if (err != 0)
		return err;

	// Set up the pipe watcher
	ev_io_init(&m_pipe_watcher,&pipe_callback,m_pipe_fds[0],EV_READ);
	m_pipe_watcher.data = this;
	ev_set_priority(&m_pipe_watcher,EV_MINPRI);
		
	// Create an ev loop
	m_pLoop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);
	if (!m_pLoop)
		return (errno ? errno : ENOMEM);

	m_started = false;

	// Start the pipe watcher
	ev_io_start(m_pLoop,&m_pipe_watcher);
	
	// Drop loop refcount
	ev_unref(m_pLoop);
	
	return 0;
}

int OOSvrBase::detail::ProactorEv::run(int& err, const OOBase::timeval_t* timeout)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	err = init();
	if (err != 0)
		return -1;
	
	// The loop is written as a leader/follower loop:
	// Each thread polls, then queues up what needs to be processed
	// Then lets the next waiting thread poll, while it processes the outstanding queue
	
	OOBase::Queue<IOEvent,OOBase::LocalAllocator> io_queue;

	guard.release();

	for (bool bStop = false;!bStop;)
	{
		if (timeout)
			guard.acquire(*timeout);
		else
			guard.acquire();

		// Swap over the IO queue to our local one...
		m_pIOQueue = &io_queue;

		void* TODO; // Timeout timer...

		// Run the ev loop with the lock held
		ev_loop(m_pLoop,0);

		if (m_mapWatchers.empty())
		{
			 if (m_started)
				bStop = true;
		}
		else
			m_started = true;

		// Release the loop mutex... freeing the next thread to poll...
		guard.release();

		// Process our queue
		IOEvent ev;
		while (io_queue.pop(&ev))
		{
			switch (ev.m_type)
			{
			case IOEvent::Watcher:
				process_event(static_cast<Watcher*>(ev.m_watcher),ev.m_fd,ev.m_revents);
				break;
			
			case IOEvent::IOWatcher:
				process_event(static_cast<IOWatcher*>(ev.m_watcher),ev.m_fd,ev.m_revents);
				break;
			
			case IOEvent::Timer:
				{ void* TODO; }
				break;
			}
		}
	}

	return 0;
}

void OOSvrBase::detail::ProactorEv::pipe_callback(struct ev_loop*, ev_io* watcher, int)
{
	static_cast<ProactorEv*>(watcher->data)->pipe_callback();
}

void OOSvrBase::detail::ProactorEv::watcher_callback(struct ev_loop* pLoop, ev_io* watcher, int revents)
{
	// Add the event to the queue
	IOEvent ev;
	ev.m_type = IOEvent::Watcher;
	ev.m_revents = revents;
	ev.m_fd = watcher->fd;
	ev.m_watcher = watcher;
	
	// Only stop the watcher if we managed to add it to the op queue
	if (static_cast<ProactorEv*>(watcher->data)->m_pIOQueue->push(ev) == 0)
		ev_io_stop(pLoop,watcher);
		
	ev_unloop(pLoop,EVUNLOOP_ONE);
}

void OOSvrBase::detail::ProactorEv::process_event(Watcher* watcher, int fd, int revents)
{
	// Just call the callback
	if (watcher->m_callback)
		(*watcher->m_callback)(fd,revents,watcher->m_param);
}

void OOSvrBase::detail::ProactorEv::iowatcher_callback(struct ev_loop* pLoop, ev_io* watcher, int revents)
{
	// Add the event to the queue	
	IOEvent ev;
	ev.m_type = IOEvent::IOWatcher;
	ev.m_revents = revents;
	ev.m_fd = watcher->fd;
	ev.m_watcher = watcher;
	
	// Only stop the watcher if we managed to add it to the op queue
	if (static_cast<ProactorEv*>(watcher->data)->m_pIOQueue->push(ev) == 0)
	{
		ev_io_stop(pLoop,watcher);
		
		if (revents & EV_READ)
			static_cast<IOWatcher*>(watcher)->m_busy_recv = true;

		if (revents & EV_WRITE)
			static_cast<IOWatcher*>(watcher)->m_busy_send = true;

		// Clear received events
		int outstanding = (watcher->events & ~revents);
		
		ev_io_set(watcher,watcher->fd,outstanding);
		
		if (outstanding != 0)
			ev_io_start(pLoop,watcher);
	}
			
	ev_unloop(pLoop,EVUNLOOP_ONE);
}

namespace
{
	struct Msg
	{
		enum Opcode
		{
			Recv,
			Send,
			RecvAgain,
			SendAgain,
			New,
			Start,
			Close,
			IOClose

		} m_op_code;
		int              m_fd;
		OOSvrBase::detail::ProactorEv::IOOp m_op;
	};

	int recv_msg(int fd, Msg& msg)
	{
		int err = 0;
		ssize_t r = 0;

		do
		{
			r = ::read(fd,&msg,sizeof(msg));
		}
		while (r == -1 && errno == EINTR);

		if (r == -1)
			err = errno;

		return err;
	}

	int send_msg(int fd, const Msg& msg)
	{
		int err = 0;
		ssize_t s = 0;
		do
		{
			s = ::write(fd,&msg,sizeof(msg));
		}
		while (s == -1 && errno == EINTR);

		if (s == -1)
			err = errno;

		return err;
	}
}

bool OOSvrBase::detail::ProactorEv::copy_queue(OOBase::Queue<IOOp,OOBase::HeapAllocator>& dest, OOBase::Queue<IOOp,OOBase::HeapAllocator>& src)
{
	IOOp op;
	while (src.pop(&op))
	{
		int err = dest.push(op);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}

	return !dest.empty();
}

void OOSvrBase::detail::ProactorEv::process_recv(IOWatcher* watcher, int fd)
{
	IOOp* op = watcher->m_queueRecv.front();
	while (op)
	{
		int err = 0;
		if (watcher->m_unbound)
			err = ECONNRESET;
		else
		{
			size_t to_read = (op->m_count ? op->m_count : op->m_buffer->space());

			// We read again on EOF, as we only return error codes
			ssize_t r = 0;
			do
			{
				r = ::recv(fd,op->m_buffer->wr_ptr(),to_read,MSG_DONTWAIT);
			}
			while (r == -1 && errno == EINTR);

			if (r == 0)
			{
				err = ECONNRESET;
			}
			else if (r < 0)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK)
					err = errno;
				else
					break;
			}
			else
			{
				op->m_buffer->wr_ptr(r);
				if (op->m_count)
					op->m_count -= r;

				// If we haven't read all the buffer, restart loop...
				if (op->m_count != 0)
					continue;
			}
		}
		
		// Notify callback of status/error
		try
		{
			(*op->m_callback)(op->m_param,op->m_buffer,err);
		}
		catch (...)
		{
			if (op->m_buffer)
				op->m_buffer->release();
			throw;
		}
		
		// Done with buffer
		if (op->m_buffer)
			op->m_buffer->release();
				
		// Pop queue front and line up the next operation...
		watcher->m_queueRecv.pop();
		op = watcher->m_queueRecv.front();
	}

	Msg msg;
	msg.m_op_code = Msg::RecvAgain;
	msg.m_fd = fd;

	int err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

void OOSvrBase::detail::ProactorEv::process_send(IOWatcher* watcher, int fd)
{
	IOOp* op = watcher->m_queueSend.front();
	while (op)
	{
		int err = 0;
		if (op->m_count == 0)
		{
			// Single send
			if (!op->m_buffer)
				watcher->m_unbound = true;
			
			if (watcher->m_unbound)
				err = ECONNRESET;
			else
			{
				// We send again on EOF, as we only return error codes
				ssize_t s = 0;
				do
				{
					s = ::send(fd,op->m_buffer->rd_ptr(),op->m_buffer->length(),MSG_DONTWAIT
#if defined(MSG_NOSIGNAL)
							| MSG_NOSIGNAL
#endif
						);
				} while (s == -1 && errno == EINTR);
				
				if (s == -1)
				{
					if (errno != EAGAIN && errno != EWOULDBLOCK)
						err = errno;
					else
						break;
				}
				else
				{
					op->m_buffer->rd_ptr(s);
					
					// If we haven't sent all the buffer, restart loop...
					if (op->m_buffer->length() != 0)
						continue;
				}
			}
			
			// Notify callback of status/error
			if (op->m_callback)
			{
				try
				{
					(*op->m_callback)(op->m_param,op->m_buffer,err);
				}
				catch (...)
				{
					if (op->m_buffer)
						op->m_buffer->release();
					throw;
				}
			}
			
			// Done with buffer
			if (op->m_buffer)
				op->m_buffer->release();
		}
		else
		{
			// Sendv
			if (watcher->m_unbound)
				err = ECONNRESET;
			else
			{
				struct iovec* iovec_bufs = static_cast<struct iovec*>(OOBase::LocalAllocate(op->m_count * sizeof(struct iovec)));
				if (!iovec_bufs)
					err = ENOMEM;
				else
				{
					size_t total = 0;
					for (size_t i=0;i<op->m_count;++i)
					{
						iovec_bufs[i].iov_len = op->m_buffers[i]->length();
						iovec_bufs[i].iov_base = const_cast<char*>(op->m_buffers[i]->rd_ptr());
						
						total += iovec_bufs[i].iov_len;
					}
					
					// We send again on EOF, as we only return error codes
					ssize_t s = 0;
					do
					{
						void* TODO; // Use sendmsg()
						s = ::writev(fd,iovec_bufs,op->m_count);
					} while (s == -1 && errno == EINTR);
							
					// Done with iovec now
					OOBase::LocalFree(iovec_bufs);
					
					if (s == -1)
					{
						if (errno != EAGAIN && errno != EWOULDBLOCK)
							err = errno;
						else
							break;
					}
					else
					{
						total -= s;
						
						for (size_t i=0;s>0 && i<op->m_count;++i)
						{
							size_t len = op->m_buffers[i]->length();
							if (len > static_cast<size_t>(s))
								len = s;
							
							op->m_buffers[i]->rd_ptr(len);
							s -= len;
						}
						
						// If we haven't sent all the buffers, restart loop...
						if (total != 0)
							continue;
					}
				}
			}
			
			// Notify callback of status/error
			if (op->m_callback_v)
			{
				try
				{
					(*op->m_callback_v)(op->m_param,op->m_buffers,op->m_count,err);
				}
				catch (...)
				{
					for (size_t i=0;i<op->m_count;++i)
						op->m_buffers[i]->release();

					delete [] op->m_buffers;
					throw;
				}
			}
			
			for (size_t i=0;i<op->m_count;++i)
				op->m_buffers[i]->release();
			
			delete [] op->m_buffers;
		}
		
		// Pop queue front and line up the next operation...
		watcher->m_queueSend.pop();
		op = watcher->m_queueSend.front();
	}

	Msg msg;
	msg.m_op_code = Msg::SendAgain;
	msg.m_fd = fd;

	int err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

void OOSvrBase::detail::ProactorEv::process_event(IOWatcher* watcher, int fd, int revents)
{
	if ((revents & EV_READ))
		process_recv(watcher,fd);
	
	if (revents & EV_WRITE)
		process_send(watcher,fd);
	
	// Check to see if the watcher should be closed
	if (watcher->m_unbound)
	{
		Msg msg;
		msg.m_op_code = Msg::IOClose;
		msg.m_fd = fd;
		
		int err = send_msg(m_pipe_fds[1],msg);
		if (err != 0)
			OOBase_CallCriticalFailure(err);
	}
}

void OOSvrBase::detail::ProactorEv::pipe_callback()
{
	// A local map of watchers, so we start each only once...
	OOBase::HashTable<int,ev_io*,OOBase::LocalAllocator> mapStart;
	
	for (Msg msg;;)	
	{
		int err = recv_msg(m_pipe_fds[0],msg);
		if (err == EAGAIN || err == EWOULDBLOCK)
			break;
		else if (err != 0)
			OOBase_CallCriticalFailure(err);
				
		if (msg.m_op_code == Msg::New)
		{
			err = m_mapWatchers.insert(msg.m_fd,static_cast<ev_io*>(msg.m_op.m_param));
			if (err != 0)
				OOBase_CallCriticalFailure(err);
		}
		else
		{
			// Process the message
			ev_io* watcher = NULL;
			if (m_mapWatchers.find(msg.m_fd,watcher))
			{
				assert(msg.m_fd == watcher->fd);
						
				int events = 0;
				switch (msg.m_op_code)
				{
				case Msg::Recv:
					if ((err = static_cast<IOWatcher*>(watcher)->m_queueRecvPending.push(msg.m_op)) != 0)
						OOBase_CallCriticalFailure(err);
					
					// We fall through if we are already not busy...
					if (static_cast<IOWatcher*>(watcher)->m_busy_recv)
						break;

				case Msg::RecvAgain:
					static_cast<IOWatcher*>(watcher)->m_busy_recv = false;

					if (copy_queue(static_cast<IOWatcher*>(watcher)->m_queueRecv,static_cast<IOWatcher*>(watcher)->m_queueRecvPending))
						events = EV_READ;
					break;

				case Msg::Send:
					if ((err = static_cast<IOWatcher*>(watcher)->m_queueSendPending.push(msg.m_op)) != 0)
						OOBase_CallCriticalFailure(err);
					
					// We fall through if we are already not busy...
					if (static_cast<IOWatcher*>(watcher)->m_busy_send)
						break;

				case Msg::SendAgain:
					static_cast<IOWatcher*>(watcher)->m_busy_send = false;

					if (copy_queue(static_cast<IOWatcher*>(watcher)->m_queueSend,static_cast<IOWatcher*>(watcher)->m_queueSendPending))
						events = EV_WRITE;
					break;

				case Msg::Close:
					if (ev_is_active(watcher))
						ev_io_stop(m_pLoop,watcher);

					close(watcher->fd);
					delete static_cast<Watcher*>(watcher);
					m_mapWatchers.erase(watcher->fd);
					break;

				case Msg::IOClose:
					if (ev_is_active(watcher))
						ev_io_stop(m_pLoop,watcher);

					close(watcher->fd);
					delete static_cast<IOWatcher*>(watcher);
					m_mapWatchers.erase(watcher->fd);
					break;

				default:
					break;
				}
							
				if (events != 0)
				{
					if (!(watcher->events & events))
					{
						if (ev_is_active(watcher))
							ev_io_stop(m_pLoop,watcher);
						
						ev_io_set(watcher,watcher->fd,watcher->events | events);
					}
					
					// Make sure we start the watcher again
					msg.m_op_code = Msg::Start;
				}
				
				if (msg.m_op_code == Msg::Start)
				{
					err = mapStart.insert(watcher->fd,watcher);
					if (err != 0 && err != EEXIST)
						OOBase_CallCriticalFailure(err);
				}
			}
		}
	}
	
	// Start the watchers
	ev_io* watcher = NULL;
	while (mapStart.pop(NULL,&watcher))
		ev_io_start(m_pLoop,watcher);
}

int OOSvrBase::detail::ProactorEv::recv(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
{
	int err = init();
	if (err != 0)
		return err;

	// Must have a callback function
	assert(callback);
	
	if (bytes == 0)
	{
		// I imagine you might want to increase space()?
		assert(buffer->space() > 0);
	}
	else
	{
		// Make sure we have room
		err = buffer->space(bytes);
		if (err != 0)
			return err;
	}
	
	Msg msg;
	msg.m_op_code = Msg::Recv;
	msg.m_fd = fd;
	msg.m_op.m_count = bytes;
	msg.m_op.m_buffer = buffer;
	msg.m_op.m_buffer->addref();
	msg.m_op.m_param = param;
	msg.m_op.m_callback = callback;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::send(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
{
	int err = init();
	if (err != 0)
		return err;
	
	// I imagine you might want to increase length()?
	assert(buffer->length() > 0);
	
	Msg msg;
	msg.m_op_code = Msg::Send;
	msg.m_fd = fd;
	msg.m_op.m_count = 0;
	msg.m_op.m_buffer = buffer;
	msg.m_op.m_buffer->addref();
	msg.m_op.m_param = param;
	msg.m_op.m_callback = callback;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::send_v(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
{
	int err = init();
	if (err != 0)
		return err;
	
	if (count == 0)
		return EINVAL;
	
	Msg msg;
	msg.m_op_code = Msg::Send;
	msg.m_fd = fd;
	msg.m_op.m_count = count;
	msg.m_op.m_param = param;
	msg.m_op.m_callback_v = callback;
	msg.m_op.m_buffers = new (std::nothrow) OOBase::Buffer*[count];
	if (!msg.m_op.m_buffers)
		return ENOMEM;
	
	for (size_t i=0;i<count;++i)
	{
		msg.m_op.m_buffers[i] = buffers[i];
		msg.m_op.m_buffers[i]->addref();
	}
		
	err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
	{
		for (size_t i=0;i<count;++i)
			msg.m_op.m_buffers[i]->release();
		
		delete [] msg.m_op.m_buffers;
	}

	return err;
}

int OOSvrBase::detail::ProactorEv::new_watcher(int fd, int events, void* param, void (*callback)(int fd, int events, void* param))
{
	int err = init();
	if (err != 0)
		return err;
	
	Watcher* watcher = new (std::nothrow) Watcher;
	if (!watcher)
		return ENOMEM;
	
	ev_io_init(watcher,&watcher_callback,fd,events);
	watcher->data = this;
	watcher->m_callback = callback;
	watcher->m_param = param;
	
	Msg msg;
	msg.m_op_code = Msg::New;
	msg.m_fd = fd;
	msg.m_op.m_param = static_cast<ev_io*>(watcher);
	
	err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
		delete watcher;
	
	return err;
}

int OOSvrBase::detail::ProactorEv::close_watcher(int fd)
{	
	Msg msg;
	msg.m_op_code = Msg::Close;
	msg.m_fd = fd;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::start_watcher(int fd)
{
	Msg msg;
	msg.m_op_code = Msg::Start;
	msg.m_fd = fd;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::bind_fd(int fd)
{
	int err = init();
	if (err != 0)
		return err;
	
	IOWatcher* watcher = new (std::nothrow) IOWatcher;
	if (!watcher)
		return ENOMEM;
	
	ev_io_init(watcher,&iowatcher_callback,fd,0);
	watcher->data = this;
	watcher->m_unbound = false;
	watcher->m_busy_recv = false;
	watcher->m_busy_send = false;

	Msg msg;
	msg.m_op_code = Msg::New;
	msg.m_fd = fd;
	msg.m_op.m_param = static_cast<ev_io*>(watcher);
	
	err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
		delete watcher;
	
	return err;
}

int OOSvrBase::detail::ProactorEv::unbind_fd(int fd)
{
	Msg msg;
	msg.m_op_code = Msg::Send;
	msg.m_fd = fd;
	msg.m_op.m_count = 0;
	msg.m_op.m_callback = NULL;
	msg.m_op.m_buffer = NULL; // <-- This is the important bit! The NULL buffer means close
		
	return send_msg(m_pipe_fds[1],msg);
}
	
#endif // defined(HAVE_EV_H)
