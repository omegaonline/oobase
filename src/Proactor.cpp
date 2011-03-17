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

#include "../include/OOBase/Condition.h"

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
	OOBASE_NEW_T_CRITICAL(Ev::ProactorImpl,m_impl,Ev::ProactorImpl());
#elif defined(_WIN32)
	OOBASE_NEW_T_CRITICAL(Win32::ProactorImpl,m_impl,Win32::ProactorImpl());
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
	OOBASE_DELETE(Proactor,m_impl);
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
	return m_impl->attach_local_socket(sock,err);
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

