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

#include "../include/OOBase/CustomNew.h"
#include "../include/OOSvrBase/Proactor.h"

//#include "ProactorImpl.h"

#if defined(HAVE_EV_H)
#include "ProactorEv.h"
#elif defined(_WIN32)
#include "ProactorWin32.h"
#else
#error No suitable proactor implementation!
#endif

#if defined(_WIN32)
#define ETIMEDOUT WSAETIMEDOUT
#define ENOTCONN  WSAENOTCONN
#else
#define ERROR_OUTOFMEMORY ENOMEM
#endif

namespace
{
	struct WaitCallback
	{
		OOBase::Condition        m_condition;
		OOBase::Condition::Mutex m_lock;
		bool                     m_complete;
		int                      m_err;

		static void callback(void* param, OOBase::Buffer*, int err)
		{
			WaitCallback* pThis = static_cast<WaitCallback*>(param);

			OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);

			pThis->m_err = err;
			pThis->m_complete = true;
			pThis->m_condition.signal();
		}
	};
}

OOSvrBase::Proactor::Proactor() :
		m_impl(0)
{
#if defined(HAVE_EV_H)
	m_impl = new (OOBase::critical) Ev::ProactorImpl();
#elif defined(_WIN32)
	m_impl = new (OOBase::critical) Win32::ProactorImpl();
#else
#error Fix me!
#endif
}

OOSvrBase::Proactor::Proactor(bool) :
		m_impl(0)
{
}

OOSvrBase::Proactor::~Proactor()
{
	delete m_impl;
}

OOSvrBase::Acceptor* OOSvrBase::Proactor::accept_local(void* param, void (*callback)(void* param, AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	return m_impl->accept_local(param,callback,path,err,psa);
}

OOSvrBase::Acceptor* OOSvrBase::Proactor::accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err)
{
	return m_impl->accept_remote(param,callback,addr,addr_len,err);
}

OOSvrBase::AsyncSocket* OOSvrBase::Proactor::attach_socket(OOBase::socket_t sock, int& err)
{
	return m_impl->attach_socket(sock,err);
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Proactor::attach_local_socket(OOBase::socket_t sock, int& err)
{
	return m_impl->connect_local_socket(path,perr,wait);
}

OOSvrBase::detail::AsyncQueued::AsyncQueued(bool sender, AsyncIOHelper* helper, AsyncHandler* handler) :
		m_sender(sender),
		m_helper(helper),
		m_handler(handler),
		m_closed(false)
{
}

OOSvrBase::detail::AsyncQueued::~AsyncQueued()
{
	assert(m_ops.empty());

	while (!m_vecAsyncs.empty())
	{
		AsyncIOHelper::AsyncOp* p = m_vecAsyncs.back();
		delete p;
		m_vecAsyncs.pop_back();
	}
		
	if (!m_sender)
		delete m_helper;
}

void OOSvrBase::detail::AsyncQueued::dispose()
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	// Handler has gone already, don't allow any further notification
	m_handler = 0;
	m_closed = true;

	guard.release();

	shutdown();
}

void OOSvrBase::detail::AsyncQueued::shutdown()
{
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	if (!m_closed)
	{
		// See if we have an op in progress
		bool bIssueNow = m_ops.empty();
		
		// Insert a NULL into queue
		m_ops.push(0);
		
		if (bIssueNow)
			issue_next(guard);
	}
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Proactor::connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout)
{
	return m_impl->connect_local_socket(path,err,timeout);
}

int OOSvrBase::AsyncSocket::recv(OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
{
	WaitCallback wait;
	wait.m_complete = false;
	wait.m_err = 0;

	if (m_closed)
		return ENOTCONN;
	
	// Build a new AsyncOp
	AsyncIOHelper::AsyncOp* op = 0;
	if (!m_vecAsyncs.empty())
	{
		op = m_vecAsyncs.back();
		m_vecAsyncs.pop_back();
	}
	else
	{
		op = new (std::nothrow) AsyncIOHelper::AsyncOp;
		if (!op)
			return ERROR_OUTOFMEMORY;
	}
	
	op->buffer = buffer->addref();
	op->len = len;
	op->param = param;

	// See if we have an op in progress
	bool bIssueNow = m_ops.empty();
	
	// Insert into queue
	m_ops.push(op);
	
	if (bIssueNow)
		issue_next(guard);

	return 0;
}

void OOSvrBase::detail::AsyncQueued::issue_next(OOBase::Guard<OOBase::SpinLock>& guard)
{
	// Lock is assumed to be held...
	int err = recv(&wait,&WaitCallback::callback,buffer,bytes,timeout);
	if (err == 0)
	{
		while (!wait.m_complete)
			wait.m_condition.wait(wait.m_lock);
		
		err = wait.m_err;
	}

	return err;
}

int OOSvrBase::AsyncSocket::send(OOBase::Buffer* buffer)
{
	WaitCallback wait;
	wait.m_complete = false;
	wait.m_err = 0;

			// It's our responsibility to delete param, as the waiter has gone...
			delete block_info;
		}
		else
		{
			// Signal completion
			block_info->err = err;
			block_info->ev.set();	
		}

	int err = send(&wait,&WaitCallback::callback,buffer);
	if (err == 0)
	{
		while (!wait.m_complete)
			wait.m_condition.wait(wait.m_lock);
		
		err = wait.m_err;
	}

	// Done with our copy of the op
	op->buffer->release();

	if (m_vecAsyncs.size() < 4)
		m_vecAsyncs.push_back(op);
	else
		delete op;
	
	if (bLast)
		m_closed = true;

	// Pop the op we just performed
	assert(m_ops.front() == op);

	m_ops.pop();
	
	issue_next(guard);

	return bRelease;
}

int OOSvrBase::detail::AsyncQueued::sync_op(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	BlockingInfo* block_info = new (std::nothrow) BlockingInfo();
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

			// Just check we haven't just been signalled
			if (!block_info->ev.is_set())
			{
				// Clear the event pointer so it is just dropped later...
				block_info->cancelled = true;

				return ETIMEDOUT;
			}
		}
		
		// Copy the error code...
		err = block_info->err;

		// Done with param...
		delete block_info;
	}

	return err;
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
	// Dispose both sender and receiver
	m_receiver.dispose();
	m_sender.dispose();

	release();
}

void OOSvrBase::detail::AsyncSocketImpl::shutdown(bool bSend, bool bRecv)
{
	if (bSend)
		m_sender.shutdown();

	if (bRecv)
		m_receiver.shutdown();
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

void OOSvrBase::detail::AsyncSocketImpl::on_recv(AsyncIOHelper::AsyncOp* recv_op, int err, bool bLast)
{
	if (m_receiver.notify_async(recv_op,err,bLast))
		release();
}

int OOSvrBase::detail::AsyncSocketImpl::recv(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout)
{
	return m_receiver.sync_op(buffer,len,timeout);
}
		
int OOSvrBase::detail::AsyncSocketImpl::async_send(OOBase::Buffer* buffer)
{
	addref();
	int err = m_sender.async_op(buffer,0);
	if (err != 0)
		release();

	return err;
}

