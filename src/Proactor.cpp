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

OOSvrBase::Proactor::Proactor(bool) :
		m_impl(0)
{
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

OOSvrBase::detail::AsyncQueued::AsyncQueued(bool sender, AsyncIOHelper* helper, AsyncHandler* handler) :
		m_sender(sender),
		m_helper(helper),
		m_handler(handler)
{
}

OOSvrBase::detail::AsyncQueued::~AsyncQueued()
{
	// Complete everything
	void* TODO;


	if (m_sender)
		delete m_helper;
}

void OOSvrBase::detail::AsyncQueued::shutdown()
{
	m_helper->shutdown();
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

bool OOSvrBase::detail::AsyncQueued::notify_async(AsyncIOHelper::AsyncOp* op, int err)
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock,false);

	bool bRelease = true;
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

		// This is a sync op, caller need not release
		bRelease = false;
	}
	else
	{
		// Check close status...
		bool bClose = m_handler->is_close(err);
		if (bClose)
			err = 0;

		// Call handler
		if (m_sender)
			m_handler->on_sent(op->buffer,err);
		else
			m_handler->on_recv(op->buffer,err);

		if (bClose)
			m_handler->on_closed();

		// Done with our copy of the op
		op->buffer->release();
		delete op;

		// Acquire lock... we need it for issue_recv
		guard.acquire();
	}

	issue_next();

	return bRelease;
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

void OOSvrBase::detail::AsyncQueued::complete_asyncs()
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	void* TODO;
}

OOSvrBase::detail::AsyncSocketImpl::AsyncSocketImpl(AsyncIOHelper* helper, AsyncHandler* handler) :
		m_receiver(false,helper,handler),
		m_sender(true,helper,handler),
		m_refcount(1)
{
	helper->bind_handler(this);
}

OOSvrBase::detail::AsyncSocketImpl::~AsyncSocketImpl()
{
}

void OOSvrBase::detail::AsyncSocketImpl::dispose()
{
	m_receiver.complete_asyncs();
	m_sender.complete_asyncs();

	release();
}

void OOSvrBase::detail::AsyncSocketImpl::addref()
{
	++m_refcount;
}

void OOSvrBase::detail::AsyncSocketImpl::release()
{
	if (--m_refcount == 0)
		delete this;
}

int OOSvrBase::detail::AsyncSocketImpl::async_recv(OOBase::Buffer* buffer, size_t len)
{
	addref();
	int err = m_receiver.async_op(buffer,len);
	if (err != 0)
		release();

	return err;
}

void OOSvrBase::detail::AsyncSocketImpl::on_recv(AsyncIOHelper::AsyncOp* recv_op, int err)
{
	if (m_receiver.notify_async(recv_op,err))
		release();
}

int OOSvrBase::detail::AsyncSocketImpl::recv(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
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
	addref();
	int err = m_sender.async_op(buffer,0);
	if (err != 0)
		release();

	return err;
}

void OOSvrBase::detail::AsyncSocketImpl::on_sent(AsyncIOHelper::AsyncOp* send_op, int err)
{
	if (m_sender.notify_async(send_op,err))
		release();
}
			
int OOSvrBase::detail::AsyncSocketImpl::send(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout)
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

void OOSvrBase::detail::AsyncSocketImpl::shutdown()
{
	m_receiver.shutdown();
}
