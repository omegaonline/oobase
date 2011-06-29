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

namespace
{
	class AsyncSocket : public OOSvrBase::AsyncSocket
	{
	public:
		AsyncSocket(OOSvrBase::Win32::ProactorImpl* pProactor, int fd);
		virtual ~AsyncSocket();

		int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		void shutdown(bool bSend, bool bRecv);
	
	private:
		OOSvrBase::Win32::ProactorImpl* m_pProactor;
		int                             m_fd;
					
		
	};
}

namespace
{
	class SocketAcceptor : public OOSvrBase::Acceptor
	{
	public:
		SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, int fd, void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err));
		virtual ~SocketAcceptor();

		int listen(size_t backlog);
		int stop();
	
	private:
		OOSvrBase::Win32::ProactorImpl*  m_pProactor;
		OOBase::Condition::Mutex         m_lock;
		OOBase::Condition                m_condition;
		struct sockaddr*                 m_addr;
		size_t                           m_addr_len;
		size_t                           m_backlog;
		size_t                           m_pending;
		void*                            m_param;
		void (*m_callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err);
		
		static void on_completion(HANDLE hSocket, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void CALLBACK accept_ready(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

		void on_accept(HANDLE hSocket, bool bRemove, DWORD dwErr);
		int bind(const struct sockaddr* addr, size_t addr_len);
	};
}

SocketAcceptor::SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err)) :
		m_pProactor(pProactor),
		m_addr(NULL),
		m_addr_len(0),
		m_backlog(0),
		m_pending(0),
		m_param(param),
		m_callback(callback)
{ }

SocketAcceptor::~SocketAcceptor()
{
	stop();
}

int SocketAcceptor::bind(const struct sockaddr* addr, size_t addr_len)
{
	m_addr = (struct sockaddr*)OOBase::malloc(addr_len);
	if (!m_addr)
		return ERROR_OUTOFMEMORY;
	
	memcpy(m_addr,addr,addr_len);
	m_addr_len = addr_len;
	
	return 0;
}

int SocketAcceptor::listen(size_t backlog)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (backlog > 8 || backlog == 0)
		backlog = 8;
	
	if (m_backlog != 0)
	{
		m_backlog = backlog;
		return 0;
	}
	else
	{
		m_backlog = backlog;
		
		return init_accept(guard);
	}
}

void SocketAcceptor::stop()
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		m_pProactor->close_watcher(m_watcher);
		m_watcher = NULL;
	}
	
	// Wait for all pending operations to complete
	while (m_pending != 0 && m_backlog == 0)
	{
		m_condition.wait(m_lock);
	}
}

int SocketAcceptor::init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	// Create a new socket
	int err = 0;
	int sock = -1;
	if ((sock = create_socket(m_addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return err;
		
	// Bind to the address
	int err = 0;
	if (::bind(sock,m_addr,m_addr_len) == -1)
	{
		err = errno;
		close(sock);
	}
	else
	{
		// Create a watcher
		m_watcher = m_pProactor->new_watcher(sock,EV_READ | EV_ERROR,this,&on_accept,err);
		if (err != 0)
			close(sock);
		else
		{
			// Start the socket listening...
			if (::listen(sock,m_backlog) == -1)
				err = errno;
			else
				err = do_accept(guard,sock);
			
			if (err != 0)
			{
				m_pProactor->close_watcher(m_watcher);
				m_watcher = NULL;
			}
		}
	}
		
	return err;
}

void SocketAcceptor::on_accept(void* param, int fd, int /*events*/)
{
	SocketAcceptor* pThis = static_cast<SocketAcceptor*>(param);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	pThis->do_accept(guard,fd);
}

void SocketAcceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, int fd)
{
	int err = 0;
	do
	{
		struct sockaddr_storage addr = {0};
		socklen_t addr_len = 0;
		
		int new_fd = ::accept(fd,(struct sockaddr*)&addr,&addr_len);
		if (new_fd == -1)
		{
			err = errno;
			if (err != EAGAIN && err != EWOULDBLOCK)
				break;
			
			// Will complete later...
			err = m_pProactor->start_watcher(m_watcher);
			if (err != 0)
				break;
			
			// Will complete later...
			++m_pending;
			return;
		}
		
		err = OOBase::BSD::set_non_blocking(new_fd,true);
		if (!err)
			err = OOBase::POSIX::set_close_on_exec(new_fd,true);
		
	}
	while (m_pending < m_backlog);
}

OOSvrBase::Acceptor* OOSvrBase::Ev::ProactorImpl::accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err)
{
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		assert((0,"Invalid parameters"));
		err = EINVAL;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = new (std::no_throw) SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		err = pAcceptor->bind(addr,addr_len);
		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}
		
	return pAcceptor;
}

OOSvrBase::AsyncSocket* OOSvrBase::Ev::ProactorImpl::connect_socket(const struct sockaddr* addr, size_t addr_len, int& err, const OOBase::timeval_t* timeout)
{
	int sock = -1;
	if ((sock = create_socket(addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return NULL;
	
	if (::connect(sock,addr,addr_len) == -1)
	{
		err = errno;
		close(sock);
		return NULL;
	}
	
	AsyncSocket* pSocket = new (std::no_throw) AsyncSocket(m_pProactor,sock);
	if (!pSocket)
	{
		close(sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pSocket;
}

OOSvrBase::AsyncSocket* OOSvrBase::Ev::ProactorImpl::attach_socket(OOBase::socket_t sock, int& err)
{
	// Set non-blocking...
	err = OOBase::BSD::set_non_blocking(sock,true);
	if (err)
		return NULL;
	
	AsyncSocket* pSocket = new (std::nothrow) AsyncSocket(m_pProactor,sock);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;
		
	return pSocket;
}
