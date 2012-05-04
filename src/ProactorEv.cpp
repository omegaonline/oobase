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

#if defined(HAVE_EV_H) && defined(USE_LIB_EV)

#include "../include/OOBase/Posix.h"
#include "./BSDSocket.h"

OOSvrBase::Proactor* OOSvrBase::Proactor::create(int& err)
{
	detail::ProactorEv* proactor = new (OOBase::critical) detail::ProactorEv();
	err = proactor->init();
	if (err)
	{
		delete proactor;
		proactor = NULL;
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

OOSvrBase::detail::ProactorEv::ProactorEv() : 
		Proactor(),
		m_pLoop(NULL),
		m_outstanding(1)
{ 
	m_pipe_fds[0] = -1;
	m_pipe_fds[1] = -1;
}

OOSvrBase::detail::ProactorEv::~ProactorEv()
{
	if (m_pLoop)
	{
		ev_ref(m_pLoop);
		ev_io_stop(m_pLoop,&m_pipe_watcher);
		ev_loop_destroy(m_pLoop);
	}

	OOBase::POSIX::close(m_pipe_fds[0]);
	OOBase::POSIX::close(m_pipe_fds[1]);
}

int OOSvrBase::detail::ProactorEv::init()
{
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

	// Start the pipe watcher
	ev_io_start(m_pLoop,&m_pipe_watcher);
	
	return 0;
}

void OOSvrBase::detail::ProactorEv::io_start(struct ev_io* io)
{
	if (!ev_is_active(io))
		ev_io_start(m_pLoop,io);
}

void OOSvrBase::detail::ProactorEv::io_stop(struct ev_io* io)
{
	if (ev_is_active(io))
		ev_io_stop(m_pLoop,io);
}

int OOSvrBase::detail::ProactorEv::run(int& err, const OOBase::Timeout& timeout)
{
	// The loop is written as a leader/follower loop:
	// Each thread polls, then queues up what needs to be processed
	// Then lets the next waiting thread poll, while it processes the outstanding queue
	
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);
	OOBase::Queue<IOEvent,OOBase::LocalAllocator> io_queue;

	for (bool bStop = false;!bStop && !timeout.has_expired();)
	{
		if (!guard.acquire(timeout))
			break;
	
		// Swap over the IO queue to our local one...
		m_pIOQueue = &io_queue;

		void* TODO; // Timeout timer...

		// Run the ev loop with the lock held
		ev_loop(m_pLoop,0);

		bStop = (m_outstanding == 0);

		int send_fd = m_pipe_fds[1];

		// Release the loop mutex... freeing the next thread to poll...
		guard.release();

		// Process our queue
		IOEvent ev;
		while (io_queue.pop(&ev))
		{
			switch (ev.m_type)
			{
			case IOEvent::Acceptor:
				process_accept(send_fd,static_cast<AcceptWatcher*>(ev.m_watcher));
				break;
			
			case IOEvent::IOWatcher:
				process_event(send_fd,static_cast<IOWatcher*>(ev.m_watcher),ev.m_fd,ev.m_revents);
				break;
			
			case IOEvent::Timer:
				{ void* TODO; }
				break;
			}
		}
	}

	return (m_outstanding == 0 ? 1 : 0);
}

void OOSvrBase::detail::ProactorEv::pipe_callback(struct ev_loop*, struct ev_io* watcher, int)
{
	static_cast<ProactorEv*>(watcher->data)->pipe_callback();
}

void OOSvrBase::detail::ProactorEv::acceptor_callback(struct ev_loop* pLoop, struct ev_io* watcher, int revents)
{
	ProactorEv* pThis = static_cast<ProactorEv*>(watcher->data);

	// Add the event to the queue
	IOEvent ev;
	ev.m_type = IOEvent::Acceptor;
	ev.m_revents = revents;
	ev.m_fd = watcher->fd;
	ev.m_watcher = watcher;
	
	// Only stop the watcher if we managed to add it to the op queue
	if (pThis->m_pIOQueue->push(ev) == 0)
	{
		pThis->io_stop(watcher);
		
		// Inc the ref count
		++static_cast<AcceptWatcher*>(watcher)->m_refcount;

		ev_unloop(pLoop,EVUNLOOP_ONE);
	}
}

void OOSvrBase::detail::ProactorEv::iowatcher_callback(struct ev_loop* pLoop, struct ev_io* watcher, int revents)
{
	ProactorEv* pThis = static_cast<ProactorEv*>(watcher->data);

	// Add the event to the queue	
	IOEvent ev;
	ev.m_type = IOEvent::IOWatcher;
	ev.m_revents = revents;
	ev.m_fd = watcher->fd;
	ev.m_watcher = watcher;
	
	// Only stop the watcher if we managed to add it to the op queue
	if ((revents & (EV_READ | EV_WRITE)) && pThis->m_pIOQueue->push(ev) == 0)
	{
		pThis->io_stop(watcher);
		
		if (revents & EV_READ)
		{
			static_cast<IOWatcher*>(watcher)->m_busy_recv = true;
			++static_cast<IOWatcher*>(watcher)->m_refcount;
		}

		if (revents & EV_WRITE)
		{
			static_cast<IOWatcher*>(watcher)->m_busy_send = true;
			++static_cast<IOWatcher*>(watcher)->m_refcount;
		}

		// Clear received events
		int outstanding = (watcher->events & ~revents);
		
		ev_io_set(watcher,watcher->fd,outstanding);
		
		if (outstanding != 0)
			pThis->io_start(watcher);

		ev_unloop(pLoop,EVUNLOOP_ONE);
	}
}

namespace
{
	struct Msg
	{
		enum Opcode
		{
			Stop,

			Recv,
			Send,
			RecvAgain,
			SendAgain,
			IOClose,

			Accept,
			AcceptAgain,
			AcceptorClose,

		} m_op_code;
		void*                               m_handle;
		OOSvrBase::detail::ProactorEv::IOOp m_op;
	};

	int recv_msg(int fd, Msg& msg)
	{
		ssize_t r = OOBase::POSIX::read(fd,&msg,sizeof(msg));
		if (r == -1)
			return errno;

		if (r != sizeof(msg))
			return EIO;

		return 0;
	}

	int send_msg(int fd, const Msg& msg)
	{
		ssize_t s = OOBase::POSIX::write(fd,&msg,sizeof(msg));
		if (s == -1)
			return errno;

		if (s != sizeof(msg))
			return EIO;

		return 0;
	}
}

void OOSvrBase::detail::ProactorEv::stop()
{
	Msg msg = { Msg::Stop, NULL, {0} };

	int err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

bool OOSvrBase::detail::ProactorEv::copy_queue(OOBase::Queue<IOOp>& dest, OOBase::Queue<IOOp>& src)
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

void OOSvrBase::detail::ProactorEv::process_accept(int send_fd, AcceptWatcher* watcher)
{
	// Just call the callback
	bool bAgain = false;
	if (watcher->m_callback)
		bAgain = (*watcher->m_callback)(watcher->m_param,watcher->fd);

	Msg msg = { Msg::AcceptAgain, watcher, {0} };
	msg.m_op_code = bAgain ? Msg::AcceptAgain : Msg::AcceptorClose;

	int err = send_msg(send_fd,msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

void OOSvrBase::detail::ProactorEv::process_recv(int send_fd, IOWatcher* watcher, int fd)
{
	IOOp* op = watcher->m_queueRecv.front();
	while (op)
	{
		int err = 0;

		size_t to_read = (op->m_count ? op->m_count : op->m_buffer->space());

		// We read again on EOF, as we only return error codes
		ssize_t r = 0;
		do
		{
			r = ::recv(fd,op->m_buffer->wr_ptr(),to_read,0);
		}
		while (r == -1 && errno == EINTR);

		if (r < 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
				err = errno;
			else
				break;
		}
		else if (r > 0)
		{
			op->m_buffer->wr_ptr(r);
			if (op->m_count)
				op->m_count -= r;

			// If we haven't read all the buffer, restart loop...
			if (op->m_count != 0)
				continue;
		}
		
		// Make sure the buffer is released
		OOBase::RefPtr<OOBase::Buffer> buf = op->m_buffer;

		// Notify callback of status/error
		if (op->m_recv_callback)
			(*op->m_recv_callback)(op->m_param,op->m_buffer,err);
		
		// Pop queue front and line up the next operation...
		watcher->m_queueRecv.pop();
		op = watcher->m_queueRecv.front();

		// Reduce refcount - this will never hit zero
		--watcher->m_refcount;
	}

	Msg msg = { Msg::RecvAgain, watcher, {0} };

	int err = send_msg(send_fd,msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

int OOSvrBase::detail::ProactorEv::get_fd(void* handle)
{
	if (!handle)
		return -1;
	else
		return static_cast<struct ev_io*>(handle)->fd;
}

bool OOSvrBase::detail::ProactorEv::deref(AcceptWatcher* watcher)
{
	bool ret = (--watcher->m_refcount == 0);
	if (ret)
	{
		if (ev_is_active(watcher))
			io_stop(watcher);

		OOBase::POSIX::close(watcher->fd);
		delete watcher;
		--m_outstanding;
	}

	return ret;
}

bool OOSvrBase::detail::ProactorEv::deref(IOWatcher* watcher)
{
	bool ret = (--watcher->m_refcount == 0);
	if (ret)
	{
		if (ev_is_active(watcher))
			io_stop(watcher);

		OOBase::POSIX::close(watcher->fd);
		delete watcher;
		--m_outstanding;
	}

	return ret;
}

void OOSvrBase::detail::ProactorEv::process_send(int send_fd, IOWatcher* watcher, int fd)
{
	IOOp* op = watcher->m_queueSend.front();
	while (op)
	{
		int err = 0;
		if (op->m_count == 0)
		{
			// Single send
			
			// We send again on EOF, as we only return error codes
			ssize_t sent = 0;
			do
			{
				sent = ::send(fd,op->m_buffer->rd_ptr(),op->m_buffer->length(),0
#if defined(MSG_NOSIGNAL)
						| MSG_NOSIGNAL
#endif
					);
			} while (sent == -1 && errno == EINTR);

			if (sent == -1)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK)
					err = errno;
				else
					break;
			}
			else
			{
				op->m_buffer->rd_ptr(sent);
				
				// If we haven't sent all the buffer, restart loop...
				if (op->m_buffer->length() != 0)
					continue;
			}
			
			// Make sure the buffer is released
			OOBase::RefPtr<OOBase::Buffer> buf = op->m_buffer;

			// Notify callback of status/error
			if (op->m_send_callback)
				(*op->m_send_callback)(op->m_param,err);
		}
		else
		{
			// Sendv

			// Find first valid buffer
			size_t first_buffer = 0;
			for (size_t i = 0;i<op->m_count;++i)
			{
				if (op->m_buffers[i]->length() > 0)
					break;

				++first_buffer;
			}

			if (first_buffer < op->m_count)
			{
				struct iovec static_bufs[4];
				OOBase::SmartPtr<struct iovec,OOBase::LocalAllocator> ptrBufs;

				struct msghdr msg = {0};
				msg.msg_iov = static_bufs;
				msg.msg_iovlen = op->m_count - first_buffer;

				if (msg.msg_iovlen > sizeof(static_bufs)/sizeof(static_bufs[0]))
				{
					if (!ptrBufs.allocate(msg.msg_iovlen * sizeof(struct iovec)))
					{
						msg.msg_iov = NULL;
						err = ENOMEM;
					}
					else
						msg.msg_iov = ptrBufs;
				}

				if (msg.msg_iov)
				{
					for (size_t i=0;i<msg.msg_iovlen;++i)
					{
						msg.msg_iov[i].iov_len = op->m_buffers[i+first_buffer]->length();
						msg.msg_iov[i].iov_base = const_cast<char*>(op->m_buffers[i+first_buffer]->rd_ptr());
					}

					bool bBreak = false;
					do
					{
						// We send again on EOF, as we only return error codes
						ssize_t sent = 0;
						do
						{
							sent = ::sendmsg(fd,&msg,0
#if defined(MSG_NOSIGNAL)
								| MSG_NOSIGNAL
#endif
							);
						} while (sent == -1 && errno == EINTR);

						if (sent == -1)
						{
							if (errno != EAGAIN && errno != EWOULDBLOCK)
								err = errno;
							else
							{
								bBreak = true;
								break;
							}
						}
						else
						{
							// Update buffers...
							do
							{
								if (static_cast<size_t>(sent) >= msg.msg_iov->iov_len)
								{
									op->m_buffers[first_buffer]->rd_ptr(msg.msg_iov->iov_len);
									++first_buffer;
									sent -= msg.msg_iov->iov_len;
									++msg.msg_iov;
									if (--msg.msg_iovlen == 0)
										break;
								}
								else
								{
									op->m_buffers[first_buffer]->rd_ptr(sent);
									msg.msg_iov->iov_len -= sent;
									msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + sent;
									sent = 0;
								}
							}
							while (sent > 0);
						}
					}
					while (msg.msg_iovlen);

					if (bBreak)
						break;
				}
			}
			
			// Notify callback of status/error
			if (op->m_send_callback)
				(*op->m_send_callback)(op->m_param,err);

			for (size_t i=0;i<op->m_count;++i)
				op->m_buffers[i]->release();
			
			delete [] op->m_buffers;
		}
		
		// Pop queue front and line up the next operation...
		watcher->m_queueSend.pop();
		op = watcher->m_queueSend.front();

		// Reduce refcount - this will never hit zero
		--watcher->m_refcount;
	}

	Msg msg = { Msg::SendAgain, watcher, {0} };

	int err = send_msg(send_fd,msg);
	if (err != 0)
		OOBase_CallCriticalFailure(err);
}

void OOSvrBase::detail::ProactorEv::process_event(int send_fd, IOWatcher* watcher, int fd, int revents)
{
	if ((revents & EV_READ))
		process_recv(send_fd,watcher,fd);
	
	if (revents & EV_WRITE)
		process_send(send_fd,watcher,fd);
}

void OOSvrBase::detail::ProactorEv::pipe_callback()
{
	for (Msg msg;;)	
	{
		int err = recv_msg(m_pipe_fds[0],msg);
		if (err == EAGAIN || err == EWOULDBLOCK)
			break;
		else if (err != 0)
			OOBase_CallCriticalFailure(err);
				
		// Process the message
		struct ev_io* watcher = static_cast<struct ev_io*>(msg.m_handle);

		int events = 0;
		switch (msg.m_op_code)
		{
		case Msg::Stop:
			// Drop loop refcount
			ev_unref(m_pLoop);
			--m_outstanding;
			break;

		case Msg::Recv:
			if (watcher)
			{
				IOWatcher* io_watcher = static_cast<IOWatcher*>(watcher);

				if ((err = io_watcher->m_queueRecvPending.push(msg.m_op)) != 0)
					OOBase_CallCriticalFailure(err);

				// Inc the ref count
				++io_watcher->m_refcount;

				if (!io_watcher->m_busy_recv)
				{
					if (copy_queue(io_watcher->m_queueRecv,io_watcher->m_queueRecvPending))
						events = EV_READ;
				}
			}
			break;

		case Msg::RecvAgain:
			if (watcher)
			{
				IOWatcher* io_watcher = static_cast<IOWatcher*>(watcher);

				io_watcher->m_busy_recv = false;

				if (copy_queue(io_watcher->m_queueRecv,io_watcher->m_queueRecvPending))
					events = EV_READ;

				deref(io_watcher);
			}
			break;

		case Msg::Send:
			if (watcher)
			{
				IOWatcher* io_watcher = static_cast<IOWatcher*>(watcher);

				if ((err = io_watcher->m_queueSendPending.push(msg.m_op)) != 0)
					OOBase_CallCriticalFailure(err);

				// Inc the ref count
				++io_watcher->m_refcount;

				if (!io_watcher->m_busy_send)
				{
					if (copy_queue(io_watcher->m_queueSend,io_watcher->m_queueSendPending))
						events = EV_WRITE;
				}
			}
			break;

		case Msg::SendAgain:
			if (watcher)
			{
				IOWatcher* io_watcher = static_cast<IOWatcher*>(watcher);

				io_watcher->m_busy_send = false;

				if (copy_queue(io_watcher->m_queueSend,io_watcher->m_queueSendPending))
					events = EV_WRITE;

				deref(io_watcher);
			}
			break;

		case Msg::IOClose:
			if (watcher)
				deref(static_cast<IOWatcher*>(watcher));
			break;

		case Msg::Accept:
			if (watcher)
				io_start(watcher);
			break;

		case Msg::AcceptAgain:
			if (watcher && !deref(static_cast<AcceptWatcher*>(watcher)))
				io_start(watcher);
			break;

		case Msg::AcceptorClose:
			if (watcher)
				deref(static_cast<AcceptWatcher*>(watcher));
			break;

		default:
			break;
		}

		if (watcher && events != 0 && (watcher->events & events) != events)
		{
			if (ev_is_active(watcher))
				io_stop(watcher);

			ev_io_set(watcher,watcher->fd,watcher->events | events);

			// Make sure we start the watcher again
			io_start(watcher);
		}
	}
}

int OOSvrBase::detail::ProactorEv::recv(void* handle, void* param, AsyncSocket::recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
{
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
		int err = buffer->space(bytes);
		if (err != 0)
			return err;
	}
	
	Msg msg = { Msg::Recv, handle, {0} };
	msg.m_op.m_count = bytes;
	msg.m_op.m_buffer = buffer;
	msg.m_op.m_buffer->addref();
	msg.m_op.m_param = param;
	msg.m_op.m_recv_callback = callback;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::send(void* handle, void* param, AsyncSocket::send_callback_t callback, OOBase::Buffer* buffer)
{
	// I imagine you might want to increase length()?
	assert(buffer->length() > 0);
	
	Msg msg = { Msg::Send, handle, {0} };
	msg.m_op.m_count = 0;
	msg.m_op.m_buffer = buffer;
	msg.m_op.m_buffer->addref();
	msg.m_op.m_param = param;
	msg.m_op.m_send_callback = callback;
	
	return send_msg(m_pipe_fds[1],msg);
}

int OOSvrBase::detail::ProactorEv::send_v(void* handle, void* param, AsyncSocket::send_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	if (count == 0)
		return EINVAL;
	
	Msg msg = { Msg::Send, handle, {0} };
	msg.m_op.m_count = count;
	msg.m_op.m_param = param;
	msg.m_op.m_send_callback = callback;
	msg.m_op.m_buffers = new (std::nothrow) OOBase::Buffer*[count];
	if (!msg.m_op.m_buffers)
		return ENOMEM;
	
	for (size_t i=0;i<count;++i)
	{
		msg.m_op.m_buffers[i] = buffers[i];
		msg.m_op.m_buffers[i]->addref();
	}
		
	int err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
	{
		for (size_t i=0;i<count;++i)
			msg.m_op.m_buffers[i]->release();
		
		delete [] msg.m_op.m_buffers;
	}

	return err;
}

void* OOSvrBase::detail::ProactorEv::new_acceptor(int fd, void* param, bool (*callback)(void* param, int fd), int& err)
{
	AcceptWatcher* watcher = new (std::nothrow) AcceptWatcher;
	if (!watcher)
	{
		err = ENOMEM;
		return NULL;
	}
	
	ev_io_init(watcher,&acceptor_callback,fd,EV_READ);
	watcher->data = this;
	watcher->m_callback = callback;
	watcher->m_param = param;
	watcher->m_refcount = 1;

	Msg msg = { Msg::Accept, watcher, {0} };

	err = send_msg(m_pipe_fds[1],msg);
	if (err != 0)
	{
		delete watcher;
		watcher = NULL;
	}
	else
		++m_outstanding;

	return watcher;
}

int OOSvrBase::detail::ProactorEv::close_acceptor(void* handle)
{	
	Msg msg = { Msg::AcceptorClose, handle, {0} };
	
	return send_msg(m_pipe_fds[1],msg);
}

void* OOSvrBase::detail::ProactorEv::bind(int fd, int& err)
{
	IOWatcher* watcher = new (std::nothrow) IOWatcher;
	if (!watcher)
	{
		err = ENOMEM;
		return NULL;
	}
	
	ev_io_init(watcher,&iowatcher_callback,fd,0);
	watcher->data = this;
	watcher->m_refcount = 1;
	watcher->m_busy_recv = false;
	watcher->m_busy_send = false;

	++m_outstanding;

	return watcher;
}

int OOSvrBase::detail::ProactorEv::unbind(void* handle)
{
	Msg msg = { Msg::IOClose, handle, {0} };

	return send_msg(m_pipe_fds[1],msg);
}
	
#endif // defined(HAVE_EV_H)
