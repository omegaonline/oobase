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

#include "../include/OOSvrBase/Proactor.h"

#include "../include/OOSvrBase/internal/ProactorImpl.h"

#if defined(HAVE_EV_H)
#include "../include/OOSvrBase/internal/ProactorEv.h"
#elif defined(_WIN32)
#include "../include/OOSvrBase/internal/ProactorWin32.h"
#else
#error No suitable proactor implementation!
#endif

#if defined(_WIN32)
#define ETIMEDOUT WSAETIMEDOUT
#else
#define ERROR_OUTOFMEMORY ENOMEM
#endif

OOSvrBase::Proactor::Proactor() :
		m_impl(0)
{
#if defined(HAVE_EV_H)
	OOBASE_NEW(m_impl,Ev::ProactorImpl());
#elif defined(_WIN32)
	OOBASE_NEW(m_impl,Win32::ProactorImpl());
#endif

	if (!m_impl)
		OOBase_OutOfMemory();
}

OOSvrBase::Proactor::~Proactor()
{
	delete m_impl;
}

OOBase::Socket* OOSvrBase::Proactor::accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa)
{
	return m_impl->accept_local(handler,path,perr,psa);
}

OOBase::Socket* OOSvrBase::Proactor::accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr)
{
	return m_impl->accept_remote(handler,address,port,perr);
}

OOSvrBase::detail::AsyncQueued::AsyncQueued(bool sender, AsyncIOHelper* helper) :
		m_sender(sender),
		m_helper(helper)
{
}

OOSvrBase::detail::AsyncQueued::~AsyncQueued()
{
	close();
}

int OOSvrBase::detail::AsyncQueued::async_op(OOBase::Buffer* buffer, size_t len)
{
	assert(buffer);

	if (len > 0)
	{
		int err = buffer->space(len);
		if (err != 0)
			return err;
	}

	return async_op(buffer,len,0);
}

int OOSvrBase::detail::AsyncQueued::async_op(OOBase::Buffer* buffer, size_t len, BlockingInfo* param)
{
	// Build a new AsyncOp
	AsyncIOHelper::AsyncOp* op;
	OOBASE_NEW(op,AsyncIOHelper::AsyncOp);
	if (!op)
		return ERROR_OUTOFMEMORY;
	
	op->buffer = buffer->duplicate();
	op->len = len;
	op->param = param;

	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	// Check for close
	if (!m_helper)
	{
		op->buffer->release();
		delete op;
		return EBADF;
	}
	
	// See if we have an op in progress
	bool bIssueNow = m_ops.empty();
	
	// Insert into queue
	try
	{
		m_ops.push(op);
	} 
	catch (std::exception& e)
	{
		OOBase_CallCriticalFailure(e.what());
	}

	if (bIssueNow)
		issue_next();

	return 0;
}

void OOSvrBase::detail::AsyncQueued::issue_next()
{
	// Lock is assumed to be held...

	if (!m_ops.empty())
	{
		AsyncIOHelper::AsyncOp* op = 0;
		try
		{
			op = m_ops.front();
			m_ops.pop();
		} 
		catch (std::exception& e)
		{
			OOBase_CallCriticalFailure(e.what());
		}

		if (m_sender)
			m_helper->send(op);
		else
			m_helper->recv(op);
	}
}

void OOSvrBase::detail::AsyncQueued::notify_async(AsyncIOHelper::AsyncOp* op, int err, void* param, void (*callback)(void*,OOBase::Buffer*,int))
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);

	if (op->param)
	{
		// Acquire the lock... because block_info may be manipulated...
		guard.acquire();

		BlockingInfo* block_info = static_cast<BlockingInfo*>(op->param);

		// Block recv complete
		if (block_info->cancelled)
		{
			// But was cancelled...

			// It's our responsibility to delete param, as the waiter has gone...
			delete block_info;
		}
		else
		{
			// Signal completion
			block_info->err = err;
			block_info->ev.set();	
		}

		// Done with our copy of the op
		op->buffer->release();
		delete op;
	}
	else
	{
		// Call handler
		(*callback)(param,op->buffer,err);

		// Done with our copy of the op
		op->buffer->release();
		delete op;

		// Acquire lock... we need it for issue_recv
		guard.acquire();
	}

	issue_next();
}

int OOSvrBase::detail::AsyncQueued::sync_op(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	BlockingInfo* block_info = 0;
	OOBASE_NEW(block_info,BlockingInfo);
	if (!block_info)
		return ERROR_OUTOFMEMORY;

	block_info->cancelled = false;
	block_info->err = 0;
	
	int err = async_op(buffer,len,block_info);
	if (err)
	{
		delete block_info;
	}
	else
	{
		// Wait for the event
		if (!block_info->ev.wait(timeout))
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			// Clear the event pointer so it is just dropped later...
			block_info->cancelled = true;

			err = ETIMEDOUT;
		}
		else
		{
			// Copy the error code...
			err = block_info->err;

			// Done with param...
			delete block_info;
		}
	}

	return err;
}

void OOSvrBase::detail::AsyncQueued::close()
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	if (m_helper)
	{
		m_helper->release();
		m_helper = 0;

		// Tight spin until all outstanding operations have stopped, close() will cause this...
		while (!m_ops.empty())
		{
			guard.release();

			OOBase::Thread::yield();

			guard.acquire();
		}
	}
}

OOSvrBase::detail::AsyncSocketImpl::AsyncSocketImpl(AsyncIOHelper* helper) :
		m_receiver(false,helper),
		m_sender(true,helper),
		m_close_count(0)
{
	helper->bind_handler(this);
}

OOSvrBase::detail::AsyncSocketImpl::~AsyncSocketImpl()
{
	close();
}

void OOSvrBase::detail::AsyncSocketImpl::on_async_recv_i(void* param, OOBase::Buffer* buffer, int err)
{
	return static_cast<AsyncSocketImpl*>(param)->on_async_recv(buffer,err);
}

void OOSvrBase::detail::AsyncSocketImpl::on_async_sent_i(void* param, OOBase::Buffer* buffer, int err)
{
	return static_cast<AsyncSocketImpl*>(param)->on_async_sent(buffer,err);
}

int OOSvrBase::detail::AsyncSocketImpl::async_recv(OOBase::Buffer* buffer, size_t len)
{
	return m_receiver.async_op(buffer,len);
}

void OOSvrBase::detail::AsyncSocketImpl::on_recv(AsyncIOHelper::AsyncOp* recv_op, int err)
{
	// Don't report the close error, it's not really an 'error'
	bool bClose = is_close(err);
	if (bClose)
		err = 0;

	m_receiver.notify_async(recv_op,err,this,&on_async_recv_i);

	if (bClose)
		on_closed();
}

int OOSvrBase::detail::AsyncSocketImpl::recv_buffer(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	return m_receiver.sync_op(buffer,len,timeout);
}
		
size_t OOSvrBase::detail::AsyncSocketImpl::recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout)
{
	if (len == 0)
		return 0;

	OOBase::Buffer* buffer;
	OOBASE_NEW(buffer,OOBase::Buffer(len));
	if (!buffer)
	{
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}

	*perr = m_receiver.sync_op(buffer,len,timeout);

	size_t ret = buffer->length();
	assert(ret <= len);
	memcpy(buf,buffer->rd_ptr(),buffer->length());
	
	buffer->release();

	return ret;
}

int OOSvrBase::detail::AsyncSocketImpl::async_send(OOBase::Buffer* buffer)
{
	return m_sender.async_op(buffer,0);
}

void OOSvrBase::detail::AsyncSocketImpl::on_sent(AsyncIOHelper::AsyncOp* send_op, int err)
{
	// Don't report the close error, it's not really an 'error'
	bool bClose = is_close(err);
	if (bClose)
		err = 0;

	m_sender.notify_async(send_op,err,this,&on_async_sent_i);

	if (bClose)
		on_closed();
}
			
int OOSvrBase::detail::AsyncSocketImpl::send_buffer(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout)
{
	return m_sender.sync_op(buffer,0,timeout);
}

int OOSvrBase::detail::AsyncSocketImpl::send(const void* buf, size_t len, const OOBase::timeval_t* timeout)
{
	if (len == 0)
		return 0;

	assert(buf);

	OOBase::Buffer* buffer;
	OOBASE_NEW(buffer,OOBase::Buffer(len));
	if (!buffer)
		return ERROR_OUTOFMEMORY;
		
	memcpy(buffer->wr_ptr(),buf,len);
	buffer->wr_ptr(len);
	
	int ret = m_sender.sync_op(buffer,0,timeout);

	buffer->release();

	return ret;
}

void OOSvrBase::detail::AsyncSocketImpl::close()
{
	// The helpers will block until all operations have completed...
	m_receiver.close();
	m_sender.close();
}

void OOSvrBase::detail::AsyncSocketImpl::on_closed()
{
	// Signal close on the first message only...
	if (m_close_count++ == 0)
		on_async_closed();
}
