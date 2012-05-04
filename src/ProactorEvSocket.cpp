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
		if (!err)
			err = OOBase::POSIX::set_close_on_exec(sock,true);

		if (err)
		{
			OOBase::POSIX::close(sock);
			return -1;
		}

		return sock;
	}

	class AsyncSocket : public OOSvrBase::AsyncLocalSocket
	{
	public:
		AsyncSocket(OOSvrBase::detail::ProactorEv* pProactor);
		virtual ~AsyncSocket();
	
		int bind(int fd);

		int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes);
		int send(void* param, send_callback_t callback, OOBase::Buffer* buffer);
		int send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count);

		int get_uid(uid_t& uid);
	
	private:
		OOSvrBase::detail::ProactorEv* m_pProactor;
		void*                          m_handle;
	};
}

AsyncSocket::AsyncSocket(OOSvrBase::detail::ProactorEv* pProactor) :
		m_pProactor(pProactor),
		m_handle(NULL)
{ }

AsyncSocket::~AsyncSocket()
{
	m_pProactor->unbind(m_handle);
}

int AsyncSocket::bind(int fd)
{
	int err = 0;
	m_handle = m_pProactor->bind(fd,err);
	return err;
}

int AsyncSocket::recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
{
	return m_pProactor->recv(m_handle,param,callback,buffer,bytes);
}

int AsyncSocket::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	return m_pProactor->send(m_handle,param,callback,buffer);
}

int AsyncSocket::send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	return m_pProactor->send_v(m_handle,param,callback,buffers,count);
}

int AsyncSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
{
	return OOBase::POSIX::get_peer_uid(m_pProactor->get_fd(m_handle),uid);
}

namespace
{
	class SocketAcceptor : public OOSvrBase::Acceptor
	{
	public:
		SocketAcceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback);
		SocketAcceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param, OOSvrBase::Proactor::accept_local_callback_t callback);
		virtual ~SocketAcceptor();

		int bind(const sockaddr* addr, socklen_t addr_len);

	private:
		OOSvrBase::detail::ProactorEv*                m_pProactor;
		void*                                         m_param;
		OOSvrBase::Proactor::accept_remote_callback_t m_callback;
		OOSvrBase::Proactor::accept_local_callback_t  m_callback_local;
		void*                                         m_handle;
		
		static bool on_accept(void* param, int fd);
		bool do_accept(int fd);
	};
}

SocketAcceptor::SocketAcceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback) :
		m_pProactor(pProactor),
		m_param(param),
		m_callback(callback),
		m_callback_local(NULL),
		m_handle(NULL)
{ }

SocketAcceptor::SocketAcceptor(OOSvrBase::detail::ProactorEv* pProactor, void* param, OOSvrBase::Proactor::accept_local_callback_t callback) :
		m_pProactor(pProactor),
		m_param(param),
		m_callback(NULL),
		m_callback_local(callback),
		m_handle(NULL)
{ }

SocketAcceptor::~SocketAcceptor()
{
	m_pProactor->close_acceptor(m_handle);
}

int SocketAcceptor::bind(const sockaddr* addr, socklen_t addr_len)
{
	// Create a new socket
	int err = 0;
	int fd = -1;
	if ((fd = create_socket(addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return err;
		
	// Bind to the address
	if (::bind(fd,addr,addr_len) == -1 ||
			::listen(fd,SOMAXCONN) == -1)
	{
		err = errno;
		OOBase::POSIX::close(fd);
	}
	else
	{
		// Create a watcher
		m_handle = m_pProactor->new_acceptor(fd,this,&on_accept,err);
		if (!m_handle)
			OOBase::POSIX::close(fd);
	}

	return err;
}

bool SocketAcceptor::on_accept(void* param, int fd)
{
	return static_cast<SocketAcceptor*>(param)->do_accept(fd);
}

bool SocketAcceptor::do_accept(int fd)
{
	for (;;)
	{
		sockaddr_storage addr = {0};
		socklen_t addr_len = 0;

		int new_fd = -1;
		do
		{
			addr_len = sizeof(addr);

			new_fd = ::accept(fd,(sockaddr*)&addr,&addr_len);
		}
		while (new_fd == -1 && errno == EINTR);
		
		int err = 0;
		if (new_fd == -1)
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Will complete later...
				return true;
			}
		}
		
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
				OOBase::POSIX::close(new_fd);
		}
		
		if (m_callback_local)
			(*m_callback_local)(m_param,pSocket,err);
		else
			(*m_callback)(m_param,pSocket,(sockaddr*)&addr,addr_len,err);
		
		if (new_fd == -1)
		{
			// accept() failed
			return false;
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
	
	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor(this,param,callback);
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

OOSvrBase::Acceptor* OOSvrBase::detail::ProactorEv::accept_local(void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		err = EINVAL;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor(this,param,callback);
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
				
		err = pAcceptor->bind((sockaddr*)&addr,addr_len);
		if (err == 0)
		{
			if (chmod(path,mode) == -1)
				err = errno;
		}

		if (err != 0)
		{
			delete pAcceptor;
			pAcceptor = NULL;
		}
	}

	return pAcceptor;
}

OOSvrBase::AsyncSocket* OOSvrBase::detail::ProactorEv::connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::Timeout& timeout)
{
	int fd = -1;
	if ((fd = create_socket(addr->sa_family,SOCK_STREAM,0,err)) == -1)
		return NULL;
	
	if ((err = OOBase::BSD::connect(fd,addr,addr_len,timeout)) != 0)
	{
		OOBase::POSIX::close(fd);
		return NULL;
	}
	
	OOSvrBase::AsyncLocalSocket* pSocket = attach_local_socket(fd,err);
	if (!pSocket)
		OOBase::POSIX::close(fd);
	
	return pSocket;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::detail::ProactorEv::connect_local_socket(const char* path, int& err, const OOBase::Timeout& timeout)
{	
	int fd = -1;
	if ((fd = create_socket(AF_UNIX,SOCK_STREAM,0,err)) == -1)
		return NULL;
	
	// Compose filename
	sockaddr_un addr = {0};
	socklen_t addr_len;
	OOBase::POSIX::create_unix_socket_address(addr,addr_len,path);
		
	if ((err = OOBase::BSD::connect(fd,(sockaddr*)&addr,addr_len,timeout)) != 0)
	{
		OOBase::POSIX::close(fd);
		return NULL;
	}

	OOSvrBase::AsyncLocalSocket* pSocket = attach_local_socket(fd,err);
	if (!pSocket)
		OOBase::POSIX::close(fd);
	
	return pSocket;
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
