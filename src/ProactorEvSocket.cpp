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

#include "../include/OOBase/Condition.h"
#include "../include/OOBase/Posix.h"
#include "./BSDSocket.h"

#include <string.h>

namespace
{
	int create_socket(int family, int socktype, int protocol, int& err)
	{
		err = 0;
		int sock = ::socket(family,socktype,protocol);
		if (sock == -1)
		{
			err = errno;
			return sock;
		}

		err = OOBase::BSD::set_non_blocking(sock,true);
		if (err != 0)
		{
			::close(sock);
			return -1;
		}

	#if defined(HAVE_UNISTD_H)
		err = OOBase::POSIX::set_close_on_exec(sock,true);
		if (err != 0)
		{
			::close(sock);
			return -1;
		}
	#endif

		return sock;
	}

	class AsyncSocket : public OOSvrBase::AsyncLocalSocket
	{
	public:
		AsyncSocket(OOSvrBase::detail::ProactorEv* pProactor);
		virtual ~AsyncSocket();
	
		int bind(int fd);

		int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		int get_uid(uid_t& uid);
	
	private:
		OOSvrBase::detail::ProactorEv* m_pProactor;
		int                            m_fd;		
	};
}

AsyncSocket::AsyncSocket(OOSvrBase::detail::ProactorEv* pProactor) :
		m_pProactor(pProactor),
		m_fd(-1)
{ }

AsyncSocket::~AsyncSocket()
{
	m_pProactor->unbind_fd(m_fd);
}

int AsyncSocket::bind(int fd)
{
	int err = m_pProactor->bind_fd(fd);
	if (err == 0)
		m_fd = fd;
	
	return err;
}

int AsyncSocket::recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
{
	return m_pProactor->recv(m_fd,param,callback,buffer,bytes,timeout);
}

int AsyncSocket::send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
{
	return m_pProactor->send(m_fd,param,callback,buffer);
}

int AsyncSocket::send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
{
	return m_pProactor->send_v(m_fd,param,callback,buffers,count);
}

int AsyncSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
{
#if defined(HAVE_UNISTD_H)
	return OOBase::POSIX::get_peer_uid(m_fd,uid);
#else
	return EINVAL;
#endif
}

namespace
{
	class SocketAcceptor : public OOSvrBase::Acceptor
	{
	public:
		typedef void (*callback_t)(void* param, OOSvrBase::AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		typedef void (*callback_local_t)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err);
	
		SocketAcceptor();
		virtual ~SocketAcceptor();

		int listen(size_t backlog);
		int stop();
		int bind(OOSvrBase::detail::ProactorEv* pProactor, void* param, const sockaddr* addr, socklen_t addr_len, callback_t callback);
		int bind(OOSvrBase::detail::ProactorEv* pProactor, void* param, const sockaddr* addr, socklen_t addr_len, mode_t mode, callback_local_t callback);
	
	private:
		class Acceptor
		{
		public:
			Acceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param);
			virtual ~Acceptor();

			int listen(size_t backlog);
			int stop(bool destroy);
		
			int bind(const sockaddr* addr, socklen_t addr_len);
			void set_callback(callback_t callback);
			void set_callback(callback_local_t callback, mode_t mode);
		
		private:	
			OOSvrBase::detail::ProactorEv*   m_pProactor;
			int                              m_fd;
			OOBase::Condition::Mutex         m_lock;
			OOBase::Condition                m_condition;
			sockaddr*                        m_addr;
			socklen_t                        m_addr_len;
			bool                             m_listening;
			bool                             m_in_progress;
			size_t                           m_refcount;
			void*                            m_param;
			callback_t                       m_callback;
			callback_local_t                 m_callback_local;
			mode_t                           m_mode;
		
			static void on_accept(int fd, int events, void* param);
			void do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
			int init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, size_t backlog);
		};
		Acceptor* m_pAcceptor;
	};
}

SocketAcceptor::SocketAcceptor() : 
		m_pAcceptor(NULL)
{ }

SocketAcceptor::~SocketAcceptor()
{
	if (m_pAcceptor)
		m_pAcceptor->stop(true);
}

int SocketAcceptor::listen(size_t backlog)
{
	if (!m_pAcceptor)
		return EBADF;
	
	return m_pAcceptor->listen(backlog);
}

int SocketAcceptor::stop()
{
	if (!m_pAcceptor)
		return EBADF;
	
	return m_pAcceptor->stop(false);
}

int SocketAcceptor::bind(OOSvrBase::detail::ProactorEv* pProactor, void* param, const sockaddr* addr, socklen_t addr_len, callback_t callback)
{
	m_pAcceptor = new (std::nothrow) SocketAcceptor::Acceptor(pProactor,param);
	if (!m_pAcceptor)
		return ENOMEM;
	
	int err = m_pAcceptor->bind(addr,addr_len);
	if (err != 0)
	{
		delete m_pAcceptor;
		m_pAcceptor = NULL;
	}
	else
		m_pAcceptor->set_callback(callback);	
		
	return err;
}

int SocketAcceptor::bind(OOSvrBase::detail::ProactorEv* pProactor, void* param, const sockaddr* addr, socklen_t addr_len, mode_t mode, callback_local_t callback)
{
	m_pAcceptor = new (std::nothrow) SocketAcceptor::Acceptor(pProactor,param);
	if (!m_pAcceptor)
		return ENOMEM;
	
	int err = m_pAcceptor->bind(addr,addr_len);
	if (err != 0)
	{
		delete m_pAcceptor;
		m_pAcceptor = NULL;
	}
	else
		m_pAcceptor->set_callback(callback,mode);	
		
	return err;
}

SocketAcceptor::Acceptor::Acceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param) :
		m_pProactor(pProactor),
		m_addr(NULL),
		m_addr_len(0),
		m_listening(false),
		m_in_progress(false),
		m_refcount(1),
		m_param(param),
		m_callback(NULL),
		m_callback_local(NULL),		
		m_mode(0)
{ }

SocketAcceptor::Acceptor::~Acceptor()
{
	OOBase::HeapFree(m_addr);
}

void SocketAcceptor::Acceptor::set_callback(SocketAcceptor::callback_t callback)
{
	m_callback = callback;
}

void SocketAcceptor::Acceptor::set_callback(SocketAcceptor::callback_local_t callback, mode_t mode)
{
	m_callback_local = callback;
	m_mode = mode;
}

int SocketAcceptor::Acceptor::bind(const sockaddr* addr, socklen_t addr_len)
{
	m_addr = (sockaddr*)OOBase::HeapAllocate(addr_len);
	if (!m_addr)
		return ENOMEM;
	
	memcpy(m_addr,addr,addr_len);
	m_addr_len = addr_len;
	
	return 0;
}

int SocketAcceptor::Acceptor::listen(size_t backlog)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_listening)
		return 0;

	m_listening = true;

	int err = init_accept(guard,backlog);
	if (err != 0)
		m_listening = false;

	return err;
}

int SocketAcceptor::Acceptor::stop(bool destroy)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_listening)
	{
		m_listening = false;
		
		if (m_fd != -1)
		{
			m_pProactor->close_watcher(m_fd);
			m_fd = -1;
		}
	}
	
	// Wait for all pending operations to complete
	while (m_in_progress && !m_listening)
		m_condition.wait(m_lock);
		
	if (destroy && --m_refcount == 0)
	{
		guard.release();
		delete this;
	}

	return 0;
}

int SocketAcceptor::Acceptor::init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, size_t backlog)
{
	// Create a new socket
	int err = 0;
	int fd = -1;
	if ((fd = create_socket(m_addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return err;
		
	// Bind to the address
	if (::bind(fd,m_addr,m_addr_len) == -1)
	{
		err = errno;
		close(fd);
	}
	else
	{
		// Create a watcher
		err = m_pProactor->new_watcher(fd,EV_READ,this,&on_accept);
		if (err != 0)
			close(fd);
		else
		{
			// Start the socket listening...
			if (::listen(fd,backlog) == -1)
			{
				err = errno;
				m_pProactor->close_watcher(fd);
			}
			else
			{
				m_fd = fd;
				do_accept(guard);
			}
		}
	}
		
	return err;
}

void SocketAcceptor::Acceptor::on_accept(int fd, int /*events*/, void* param)
{
	Acceptor* pThis = static_cast<Acceptor*>(param);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	pThis->m_in_progress = false;
	
	pThis->do_accept(guard);
}

void SocketAcceptor::Acceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	while (m_listening)
	{
		sockaddr_storage addr = {0};
		socklen_t addr_len = sizeof(addr);
		
		int err = 0;
		int new_fd = ::accept(m_fd,(sockaddr*)&addr,&addr_len);
		if (new_fd == -1)
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Will complete later...
				err = m_pProactor->start_watcher(m_fd);
				if (err == 0)
				{
					m_in_progress = true;
					return;
				}
			}
		}
		
		// Keep us alive while the callbacks fire
		++m_refcount;
		
		guard.release();
		
		::AsyncSocket* pSocket = NULL;
		if (new_fd != -1)
		{
			err = OOBase::POSIX::set_close_on_exec(new_fd,true);
			if (err == 0)
				err = OOBase::BSD::set_non_blocking(new_fd,true);
					
			if (err == 0)
			{
				// Wrap the handle
				pSocket = new (std::nothrow) ::AsyncSocket(m_pProactor);
				if (!pSocket)
					err = ENOMEM;
				else
				{
					err = pSocket->bind(new_fd);
					if (err != 0)
					{
						delete pSocket;
						pSocket = NULL;
					}
				}
			}
			
			if (err != 0)
				close(new_fd);
		}
		
		if (m_callback_local)
			(*m_callback_local)(m_param,pSocket,err);
		else
			(*m_callback)(m_param,pSocket,(sockaddr*)&addr,addr_len,err);
				
		guard.acquire();
		
		// See if we have been killed in the callback
		if (--m_refcount == 0)
		{
			guard.release();
			delete this;
			break;
		}
	}
}

OOSvrBase::Acceptor* OOSvrBase::detail::ProactorEv::accept_remote(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err), const sockaddr* addr, socklen_t addr_len, int& err)
{
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		err = EINVAL;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor();
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		err = pAcceptor->bind(this,param,addr,addr_len,callback);
		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}
		
	return pAcceptor;
}

OOSvrBase::Acceptor* OOSvrBase::detail::ProactorEv::accept_local(void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		err = EINVAL;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = NULL;
	
#if defined(HAVE_UNISTD_H)
	pAcceptor = new (std::nothrow) SocketAcceptor();
	if (!pAcceptor)
		err = ENOMEM;
	else
	{
		mode_t mode = 0666;
		if (psa)
			mode = psa->mode;
	
		// Compose filename
		sockaddr_un addr = {0};
		socklen_t addr_len;
		OOBase::POSIX::create_unix_socket_address(addr,addr_len,path);
				
		err = pAcceptor->bind(this,param,(sockaddr*)&addr,addr_len,mode,callback);
		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}
#else
	// If we don't have UNIX sockets, we can't do much, use Win32 Proactor instead
	err = ENOENT;

	(void)param;
	(void)psa;
#endif

	return pAcceptor;
}

OOSvrBase::AsyncSocket* OOSvrBase::detail::ProactorEv::connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::timeval_t* timeout)
{
	int fd = -1;
	if ((fd = create_socket(addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return NULL;
	
	if ((err = OOBase::BSD::connect(fd,addr,addr_len,timeout)) != 0)
	{
		close(fd);
		return NULL;
	}
	
	OOSvrBase::AsyncLocalSocket* pSocket = attach_local_socket(fd,err);
	if (!pSocket)
		close(fd);
	
	return pSocket;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::detail::ProactorEv::connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout)
{	
#if defined(HAVE_UNISTD_H)
		
	int fd = -1;
	if ((fd = create_socket(AF_UNIX,SOCK_STREAM,0,err)) == -1)
		return NULL;
	
	// Compose filename
	sockaddr_un addr = {0};
	socklen_t addr_len;
	OOBase::POSIX::create_unix_socket_address(addr,addr_len,path);
		
	if ((err = OOBase::BSD::connect(fd,(sockaddr*)&addr,addr_len,timeout)) != 0)
	{
		close(fd);
		return NULL;
	}

	OOSvrBase::AsyncLocalSocket* pSocket = attach_local_socket(fd,err);
	if (!pSocket)
		close(fd);
	
	return pSocket;
	
#else
	
	// If we don't have UNIX sockets, we can't do much, use Win32 Proactor instead
	err = ENOENT;

	(void)path;

	return NULL;

#endif
}

OOSvrBase::AsyncSocket* OOSvrBase::detail::ProactorEv::attach_socket(OOBase::socket_t sock, int& err)
{
	return attach_local_socket(sock,err);
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::detail::ProactorEv::attach_local_socket(OOBase::socket_t sock, int& err)
{
	// Set non-blocking...
	err = OOBase::BSD::set_non_blocking(sock,true);
	if (err)
		return NULL;
	
	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this);
	if (!pSocket)
		err = ENOMEM;
	else
	{
		err = pSocket->bind(sock);
		if (err != 0)
		{
			delete pSocket;
			pSocket = NULL;
		}
	}
		
	return pSocket;
}

#endif // defined(HAVE_EV_H)
