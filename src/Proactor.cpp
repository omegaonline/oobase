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

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Condition.h"

#include "../include/OOSvrBase/Proactor.h"

#if defined(_WIN32)
#include "ProactorWin32.h"
#elif defined(HAVE_EV_H)
#include "ProactorEv.h"
#else
#error No suitable proactor implementation!
#endif

namespace
{
	struct WaitCallback
	{
		OOBase::Condition        m_condition;
		OOBase::Condition::Mutex m_lock;
		bool                     m_complete;
		int                      m_err;

		static void callback_recv(void* param, OOBase::Buffer*, int err)
		{
			callback(param,err);
		}

		static void callback(void* param, int err)
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
		m_impl(NULL)
{
#if defined(_WIN32)
	m_impl = new (std::nothrow) detail::ProactorWin32();
#elif defined(HAVE_EV_H)
	m_impl = new (std::nothrow) detail::ProactorEv();
#else
#error Fix me!
#endif
	if (m_impl)
	{
		int err = m_impl->init();
		if (err != 0)
		{
			delete m_impl;
			OOBase_CallCriticalFailure(err);
		}
	}
	else
		OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
}

OOSvrBase::Proactor::Proactor(bool) :
		m_impl(NULL)
{
	// This stops derived classes recursively constructing themselves (using the default constructor)
}

OOSvrBase::Proactor::~Proactor()
{
	delete m_impl;
}

int OOSvrBase::Proactor::init()
{
	return 0;
}

int OOSvrBase::Proactor::run(int& err, const OOBase::timeval_t* timeout)
{
	return m_impl->run(err,timeout);
}

void OOSvrBase::Proactor::stop()
{
	return m_impl->stop();
}

OOSvrBase::Acceptor* OOSvrBase::Proactor::accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	return m_impl->accept_local(param,callback,path,err,psa);
}

OOSvrBase::Acceptor* OOSvrBase::Proactor::accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err)
{
	return m_impl->accept_remote(param,callback,addr,addr_len,err);
}

OOSvrBase::AsyncSocket* OOSvrBase::Proactor::attach_socket(OOBase::socket_t sock, int& err)
{
	return m_impl->attach_socket(sock,err);
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Proactor::attach_local_socket(OOBase::socket_t sock, int& err)
{
	return m_impl->attach_local_socket(sock,err);
}

OOSvrBase::AsyncSocket* OOSvrBase::Proactor::connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::timeval_t* timeout)
{
	return m_impl->connect_socket(addr,addr_len,err,timeout);
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

	OOBase::Guard<OOBase::Condition::Mutex> guard(wait.m_lock);

	int err = recv(&wait,&WaitCallback::callback_recv,buffer,bytes,timeout);
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

	OOBase::Guard<OOBase::Condition::Mutex> guard(wait.m_lock);

	int err = send(&wait,&WaitCallback::callback,buffer);
	if (err == 0)
	{
		while (!wait.m_complete)
			wait.m_condition.wait(wait.m_lock);
		
		err = wait.m_err;
	}

	return err;
}
