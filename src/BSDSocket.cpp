///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOBase/GlobalNew.h"

#if defined(HAVE_UNISTD_H)

#include "../include/OOBase/Socket.h"
#include "../include/OOBase/Posix.h"

#include "BSDSocket.h"

#include <fcntl.h>
#include <netdb.h>
#include <sys/un.h>

OOBase::socket_t OOBase::Net::open_socket(int family, int type, int protocol, int& err)
{
#if defined(SOCK_NONBLOCK)
	socket_t sock = ::socket(family,type | SOCK_NONBLOCK,protocol);
#else
	socket_t sock = ::socket(family,type,protocol);
#endif

	if (sock == -1)
		err = errno;
	else
		err = 0;


#if !defined(SOCK_NONBLOCK)
	if ((err = OOBase::POSIX::set_non_blocking(sock,true)) != 0)
	{
		OOBase::Net::close_socket(sock);
		sock = -1;
	}
#endif

	return sock;
}

int OOBase::Net::close_socket(socket_t sock)
{
	return POSIX::close(sock);
}

int OOBase::Net::bind(socket_t sock, const sockaddr* addr, socklen_t addr_len)
{
	if (::bind(sock,addr,addr_len) == -1)
		return errno;

	return 0;
}

int OOBase::Net::connect(socket_t sock, const sockaddr* addr, socklen_t addrlen, const Timeout& timeout)
{
	// Check blocking state
	bool non_blocking = false;
	bool changed_blocking = false;
	int err = 0;

	// Make sure we are non-blocking
	if (!timeout.is_infinite())
	{
		err = POSIX::get_non_blocking(sock,non_blocking);
		if (err)
			return err;

		if (!non_blocking)
		{
			changed_blocking = true;
			err = POSIX::set_non_blocking(sock,true);
			if (err)
				return err;
		}
	}

	// Do the connect
	do
	{
		err = ::connect(sock,addr,addrlen);
	}
	while (err != 0 && errno == EINTR);

	if (!err)
	{
		if (changed_blocking)
			err = POSIX::set_non_blocking(sock,non_blocking);

		return err;
	}

	// Check to see if we actually have an error
	if (errno != EINPROGRESS)
		err = errno;
	else
	{
		// Wait for the connect to go ahead - by select()ing on write
		fd_set wfds;
		while (!timeout.has_expired())
		{
			int count = 0;
			do
			{
				FD_ZERO(&wfds);
				FD_SET(sock,&wfds);

				struct ::timeval tv;
				timeout.get_timeval(tv);

				count = ::select(static_cast<int>(sock)+1,NULL,&wfds,NULL,timeout.is_infinite() ? NULL : &tv);
			}
			while (count == -1 && errno == EINTR);

			if (count == -1)
				err = errno;
			else if (count == 0)
				err = ETIMEDOUT;
			else
			{
				// If connect() did something...
				socklen_t len = sizeof(err);
				if (::getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
					err = errno;
			}
			break;
		}
	}

	if (changed_blocking)
	{
		int err2 = POSIX::set_non_blocking(sock,non_blocking);
		if (!err)
			err = err2;
	}

	return err;
}

int OOBase::Net::accept(socket_t accept_sock, socket_t& new_sock, const Timeout& timeout)
{
	new_sock = -1;

	// Check blocking state
	bool non_blocking = false;
	bool changed_blocking = false;
	int err = 0;

	// Make sure we are non-blocking
	if (!timeout.is_infinite())
	{
		err = POSIX::get_non_blocking(accept_sock,non_blocking);
		if (err)
			return err;

		if (!non_blocking)
		{
			changed_blocking = true;
			err = POSIX::set_non_blocking(accept_sock,true);
			if (err)
				return err;
		}
	}

	while (!err && !timeout.has_expired())
	{
		do
		{
			new_sock = ::accept(accept_sock,NULL,NULL);
		}
		while (new_sock == -1 && errno == EINTR);

		if (new_sock != -1)
			break;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			err = errno;
			break;
		}

		fd_set read_fds;
		int count = 0;
		do
		{
			FD_ZERO(&read_fds);
			FD_SET(accept_sock,&read_fds);

			struct ::timeval tv;
			timeout.get_timeval(tv);

			count = ::select(static_cast<int>(accept_sock)+1,&read_fds,NULL,NULL,timeout.is_infinite() ? NULL : &tv);
		}
		while (count == -1 && errno == EINTR);

		if (count == -1)
			err = errno;
		else if (count == 0)
			err = ETIMEDOUT;
	}

	if (changed_blocking)
	{
		int err2 = POSIX::set_non_blocking(accept_sock,non_blocking);
		if (!err)
			err = err2;
	}

	if (err && new_sock != -1)
	{
		Net::close_socket(new_sock);
		new_sock = -1;
	}

	return err;
}

namespace
{
	class Socket : public OOBase::Socket
	{
	public:
		Socket(OOBase::socket_t sock, bool local);
		virtual ~Socket();

		size_t send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);

		void close();

		int get_peer_uid(uid_t& uid) const;
		int send_socket(OOBase::socket_t sock, const OOBase::Timeout& timeout);
		int recv_socket(OOBase::socket_t& sock, const OOBase::Timeout& timeout);

	private:
		const int  m_sock;
		const bool m_local;

		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;

		int do_select(bool bWrite, const OOBase::Timeout& timeout);
	};

	int connect_i(const char* address, const char* port, int& err, const OOBase::Timeout& timeout)
	{
		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = NULL;
		if (::getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			err = errno;
			return -1;
		}

		int sock = -1;

		// Took too long to resolve...
		if (timeout.has_expired())
			err = ETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				sock = OOBase::Net::open_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,err);
				if (err)
					break;

				if ((err = OOBase::Net::connect(sock,pAddr->ai_addr,pAddr->ai_addrlen,timeout)) != 0)
					OOBase::Net::close_socket(sock);
				else
					break;

				if (timeout.has_expired())
				{
					err = ETIMEDOUT;
					break;
				}
			}
		}

		// Done with address info
		::freeaddrinfo(pResults);

		return sock;
	}
}

Socket::Socket(int sock, bool local) :
		m_sock(sock),
		m_local(local)
{
}

Socket::~Socket()
{
	OOBase::Net::close_socket(m_sock);
}

int Socket::do_select(bool bWrite, const OOBase::Timeout& timeout)
{
	fd_set fds,efds;
	int count = 0;
	do
	{
		FD_ZERO(&fds);
		FD_ZERO(&efds);
		FD_SET(m_sock,&fds);
		FD_SET(m_sock,&efds);

		struct ::timeval tv;
		timeout.get_timeval(tv);

		count = ::select(m_sock+1,(bWrite ? NULL : &fds),(bWrite ? &fds : NULL),&efds,timeout.is_infinite() ? NULL : &tv);
	}
	while (count == -1 && errno == EINTR);

	if (count == -1)
		return errno;

	if (count == 0)
		return ETIMEDOUT;

	if (FD_ISSET(m_sock,&efds))
	{
		// Error
		int err = 0;
		socklen_t len = sizeof(err);
		if (::getsockopt(m_sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
			err = errno;

		return err;
	}

	return 0;
}

size_t Socket::send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
	const char* cbuf = static_cast<const char*>(buf);
	size_t to_send = len;
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
	{
		err = ETIMEDOUT;
		return 0;
	}

	do
	{
		ssize_t sent = 0;
		do
		{
			sent = ::send(m_sock,cbuf,to_send,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent > 0)
		{
			to_send -= sent;
			cbuf += sent;
		}
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(true,timeout);
			}
		}
	}
	while (err == 0 && to_send);

	return (len - to_send);
}

int Socket::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	struct iovec static_bufs[4];
	OOBase::SmartPtr<struct iovec,OOBase::LocalAllocator> ptrBufs;

	struct msghdr msg = {0};
	msg.msg_iov = static_bufs;
	msg.msg_iovlen = count;

	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		if (!ptrBufs.allocate(count * sizeof(struct iovec)))
			return ENOMEM;

		msg.msg_iov = ptrBufs;
	}

	for (size_t i=0;i<count;++i)
	{
		msg.msg_iov[i].iov_len = buffers[i]->length();
		msg.msg_iov[i].iov_base = const_cast<char*>(buffers[i]->rd_ptr());
	}

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
		return ETIMEDOUT;

	size_t first_buffer = 0;
	int err = 0;
	do
	{
		ssize_t sent = 0;
		do
		{
			sent = ::sendmsg(m_sock,&msg,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent > 0)
		{
			// Update buffers...
			do
			{
				if (static_cast<size_t>(sent) >= msg.msg_iov->iov_len)
				{
					buffers[first_buffer]->rd_ptr(msg.msg_iov->iov_len);
					++first_buffer;
					sent -= msg.msg_iov->iov_len;
					++msg.msg_iov;
					if (--msg.msg_iovlen == 0)
						break;
				}
				else
				{
					buffers[first_buffer]->rd_ptr(sent);
					msg.msg_iov->iov_len -= sent;
					msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + sent;
					sent = 0;
				}
			}
			while (sent > 0);
		}
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(true,timeout);
			}
		}
	}
	while (err == 0 && msg.msg_iovlen);

	return err;
}

int Socket::send_socket(OOBase::socket_t sock, const OOBase::Timeout& timeout)
{
	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
		return ETIMEDOUT;

	int err = 0;
	do
	{
		struct iovec iov;
		iov.iov_base = const_cast<char*>("F");
		iov.iov_len = 1;

		char control[CMSG_SPACE(sizeof(OOBase::socket_t))] = {0};

		struct msghdr msg = {0};
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);

		struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(OOBase::socket_t));
		*reinterpret_cast<OOBase::socket_t*>(CMSG_DATA(cmsg)) = sock;

		ssize_t sent = 0;
		do
		{
			sent = ::sendmsg(m_sock,&msg,0
#if defined(MSG_NOSIGNAL)
				| MSG_NOSIGNAL
#endif
			);
		}
		while (sent == -1 && errno == EINTR);

		if (sent == 1)
			break;

		err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK)
		{
			// Do the select...
			err = do_select(true,timeout);
		}
	}
	while (err == 0);

	return err;
}

size_t Socket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	char* cbuf = static_cast<char*>(buf);
	size_t to_recv = len;
	err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
	{
		err = ETIMEDOUT;
		return 0;
	}

	do
	{
		ssize_t recvd = 0;
		do
		{
			recvd = ::recv(m_sock,cbuf,to_recv,0);
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd > 0)
		{
			to_recv -= recvd;
			cbuf += recvd;

			if (!bAll)
				break;
		}
		else if (recvd == 0)
			break;
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(false,timeout);
			}
		}
	}
	while (err == 0 && to_recv);

	return (len - to_recv);
}

int Socket::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	struct iovec static_bufs[4];
	OOBase::SmartPtr<struct iovec,OOBase::LocalAllocator> ptrBufs;

	struct msghdr msg = {0};
	msg.msg_iov = static_bufs;
	msg.msg_iovlen = count;

	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		if (!ptrBufs.allocate(count * sizeof(struct iovec)))
			return ENOMEM;

		msg.msg_iov = ptrBufs;
	}

	for (size_t i=0;i<count;++i)
	{
		msg.msg_iov[i].iov_len = buffers[i]->space();
		msg.msg_iov[i].iov_base = buffers[i]->wr_ptr();
	}

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
		return ETIMEDOUT;

	size_t first_buffer = 0;
	int err = 0;
	do
	{
		ssize_t recvd = 0;
		do
		{
			recvd = ::recvmsg(m_sock,&msg,0);
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd > 0)
		{
			// Update buffers...
			do
			{
				if (static_cast<size_t>(recvd) >= msg.msg_iov->iov_len)
				{
					buffers[first_buffer]->wr_ptr(msg.msg_iov->iov_len);
					++first_buffer;
					recvd -= msg.msg_iov->iov_len;
					++msg.msg_iov;
					if (--msg.msg_iovlen == 0)
						break;
				}
				else
				{
					buffers[first_buffer]->wr_ptr(recvd);
					msg.msg_iov->iov_len -= recvd;
					msg.msg_iov->iov_base = static_cast<char*>(msg.msg_iov->iov_base) + recvd;
					recvd = 0;
				}
			}
			while (recvd > 0);
		}
		else if (recvd == 0)
			break;
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(false,timeout);
			}
		}
	}
	while (err == 0 && msg.msg_iovlen);

	return err;
}

int Socket::recv_socket(OOBase::socket_t& sock, const OOBase::Timeout& timeout)
{
	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
		return ETIMEDOUT;

	int err = 0;
	do
	{
		char c;
		struct iovec iov;
		iov.iov_base = &c;
		iov.iov_len = 1;

		char control[CMSG_SPACE(sizeof(OOBase::socket_t))] = {0};

		struct msghdr msg = {0};
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);

		struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(OOBase::socket_t));

		ssize_t recvd = 0;
		do
		{
			recvd = ::recvmsg(m_sock,&msg,0);
		}
		while (recvd == -1 && errno == EINTR);

		if (recvd > 0)
		{
			sock = *reinterpret_cast<OOBase::socket_t*>(CMSG_DATA(cmsg));
			break;
		}

		if (recvd == 0)
			err = ECONNABORTED;
		else
		{
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
			{
				// Do the select...
				err = do_select(false,timeout);
			}
		}
	}
	while (err == 0);

	return err;
}

void Socket::close()
{
	::shutdown(m_sock,SHUT_WR);
}

int Socket::get_peer_uid(uid_t& uid) const
{
	if (!m_local)
		return ENOTSUP;

	return OOBase::POSIX::get_peer_uid(m_sock,uid);
}

OOBase::Socket* OOBase::Socket::attach(socket_t sock, int& err)
{
	err = OOBase::POSIX::set_non_blocking(sock,true);
	if (err)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock,false);
	if (!pSocket)
		err = ENOMEM;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::attach_local(socket_t sock, int& err)
{
	err = OOBase::POSIX::set_non_blocking(sock,true);
	if (err)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock,true);
	if (!pSocket)
		err = ENOMEM;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int& err, const Timeout& timeout)
{
	socket_t sock = connect_i(address,port,err,timeout);
	if (sock == -1)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock,false);
	if (!pSocket)
	{
		err = ENOMEM;
		OOBase::Net::close_socket(sock);
	}

	return pSocket;
}

void OOBase::POSIX::create_unix_socket_address(sockaddr_un& addr, socklen_t& len, const char* path)
{
	addr.sun_family = AF_UNIX;
	
	if (path[0] == '\0')
	{
		addr.sun_path[0] = '\0';
		strncpy(addr.sun_path+1,path+1,sizeof(addr.sun_path)-1);
		len = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path+1) + 1;
	}
	else
	{
		strncpy(addr.sun_path,path,sizeof(addr.sun_path)-1);
		addr.sun_path[sizeof(addr.sun_path)-1] = '\0';
		len = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path);
	}
}

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int& err, const Timeout& timeout)
{
	int sock = Net::open_socket(AF_UNIX,SOCK_STREAM,0,err);
	if (err)
		return NULL;

	sockaddr_un addr;
	socklen_t addr_len = 0;
	POSIX::create_unix_socket_address(addr,addr_len,path);
	
	if ((err = Net::connect(sock,(sockaddr*)(&addr),addr_len,timeout)) != 0)
	{
		OOBase::Net::close_socket(sock);
		return NULL;
	}

	OOBase::Socket* pSocket = new (std::nothrow) ::Socket(sock,true);
	if (!pSocket)
	{
		err = ENOMEM;
		OOBase::Net::close_socket(sock);
	}

	return pSocket;
}

#endif // defined(HAVE_UNISTD_H)
