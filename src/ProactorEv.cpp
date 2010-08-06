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

#if defined(HAVE_EV_H)

#include "../include/OOBase/internal/BSDSocket.h"
#include "../include/OOSvrBase/internal/ProactorImpl.h"
#include "../include/OOSvrBase/internal/ProactorEv.h"

#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#endif /* HAVE_SYS_UN_H */

#if defined(_WIN32)
#include <ws2tcpip.h>

#define ssize_t int
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define socket_errno WSAGetLastError()
#define SOCKET_WOULD_BLOCK WSAEWOULDBLOCK
#define ECONNRESET WSAECONNRESET

#else

#define socket_errno (errno)
#define SOCKET_WOULD_BLOCK EAGAIN
#define closesocket(s) ::close(s)

#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4127) // Conditional expressions
//#pragma warning(disable: 4250) // Moans about virtual base classes
#endif

namespace
{
	class IOHelper : public OOSvrBase::detail::AsyncIOHelper
	{
	public:
		IOHelper(OOSvrBase::Ev::ProactorImpl* pProactor, int fd);
		virtual ~IOHelper();
		
		void recv(AsyncOp* recv_op);
		void send(AsyncOp* send_op);
		void close();
		void shutdown(bool bSend, bool bRecv);
				
	private:
		void do_read();
		void handle_read(int err, size_t have_read);

		void do_write();
		void handle_write(int err, size_t have_sent);

		struct Completion : public OOSvrBase::Ev::ProactorImpl::io_watcher
		{
			AsyncOp*        m_op;
			IOHelper*       m_this_ptr;
		};

		OOSvrBase::Ev::ProactorImpl* m_proactor;
		Completion                   m_read_complete;
		Completion                   m_write_complete;

		static void read_ready(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher);
		static void write_ready(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher);
	};
	
	IOHelper::IOHelper(OOSvrBase::Ev::ProactorImpl* pProactor, int fd) :
			m_proactor(pProactor)
	{
		m_read_complete.m_this_ptr = this;
		m_read_complete.callback = &read_ready;
		m_read_complete.m_op = 0;
		m_proactor->init_watcher(&m_read_complete,fd,EV_READ);

		m_write_complete.m_this_ptr = this;
		m_write_complete.callback = &write_ready;
		m_write_complete.m_op = 0;
		m_proactor->init_watcher(&m_write_complete,fd,EV_WRITE);
	}

	IOHelper::~IOHelper()
	{
		closesocket(m_read_complete.fd);
	}

	void IOHelper::close()
	{
		closesocket(m_read_complete.fd);
	}

	void IOHelper::shutdown(bool bSend, bool bRecv)
	{
		int how = -1;
		if (bSend)
			how = (bRecv ? SHUT_RDWR : SHUT_WR);
		else if (bRecv)
			how = SHUT_RD;

		if (how != -1)
			::shutdown(m_read_complete.fd,how);
	}

	void IOHelper::recv(AsyncOp* recv_op)
	{
		assert(!m_read_complete.m_op);

		// Reset the completion info
		m_read_complete.m_op = recv_op;
				
		do_read();
	}

	void IOHelper::send(AsyncOp* send_op)
	{
		assert(!m_write_complete.m_op);

		// Reset the completion info
		m_write_complete.m_op = send_op;
				
		do_write();
	}

	void IOHelper::do_read()
	{
		size_t to_read = static_cast<DWORD>(m_read_complete.m_op->len);
		if (to_read == 0)
			to_read = static_cast<DWORD>(m_read_complete.m_op->buffer->space());

		ssize_t have_read = ::recv(m_read_complete.fd,m_read_complete.m_op->buffer->wr_ptr(),to_read,0);
		if (have_read > 0)
		{
			handle_read(0,have_read);
		}
		else if (have_read == 0)
		{
			handle_read(ECONNRESET,have_read);
		}
		else if (have_read < 0)
		{
			int err = socket_errno;
			if (err == SOCKET_WOULD_BLOCK)
				m_proactor->start_watcher(&m_read_complete);
			else
				handle_read(err,0);
		}
	}

	void IOHelper::handle_read(int err, size_t have_read)
	{
		// Update wr_ptr
		m_read_complete.m_op->buffer->wr_ptr(have_read);

		if (m_read_complete.m_op->len)
			m_read_complete.m_op->len -= have_read;

		if (err == 0 && m_read_complete.m_op->len > 0)
		{
			// More to read
			return do_read();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_read_complete.m_op;
		m_read_complete.m_op = 0;

		// Call handlers
		m_handler->on_recv(op,err);
	}
	
	void IOHelper::do_write()
	{
		ssize_t have_sent = ::send(m_write_complete.fd,m_write_complete.m_op->buffer->rd_ptr(),m_write_complete.m_op->buffer->length(),0);
		if (have_sent > 0)
		{
			handle_write(0,static_cast<size_t>(have_sent));
		}
		else
		{
			int err = socket_errno;
			if (err == SOCKET_WOULD_BLOCK)
				m_proactor->start_watcher(&m_write_complete);
			else
				handle_write(err,0);
		}
	}

	void IOHelper::handle_write(int err, size_t have_sent)
	{
		// Update rd_ptr
		m_write_complete.m_op->buffer->rd_ptr(have_sent);

		if (err == 0 && m_write_complete.m_op->buffer->length() > 0)
		{
			// More to send
			return do_write();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_write_complete.m_op;
		m_write_complete.m_op = 0;

		// Call handlers
		m_handler->on_sent(op,err);
	}

	void IOHelper::read_ready(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher)
	{
		static_cast<IOHelper::Completion*>(watcher)->m_this_ptr->do_read();
	}

	void IOHelper::write_ready(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher)
	{
		static_cast<IOHelper::Completion*>(watcher)->m_this_ptr->do_write();
	}

	class AsyncSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>
	{
	public:
		AsyncSocket(IOHelper* helper) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>(helper)
		{
		}

		static AsyncSocket* Create(OOSvrBase::Ev::ProactorImpl* proactor, OOBase::Socket::socket_t sock)
		{
			IOHelper* helper = 0;
			OOBASE_NEW(helper,IOHelper(proactor,sock));
			if (!helper)
				return 0;

			AsyncSocket* pSock = 0;
			OOBASE_NEW(pSock,AsyncSocket(helper));
			if (!pSock)
			{
				delete helper;
				return 0;
			}

			return pSock;
		}

	private:
		bool is_close(int err) const
		{
			switch (err)
			{
#if defined(_WIN32)
			case WSAECONNABORTED:
			case WSAECONNRESET:
				return true;
#else
#endif
			default:
				return false;
			}
		}
	};

#if defined(HAVE_SYS_UN_H)
	class AsyncLocalSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>
	{
	public:
		AsyncLocalSocket(IOHelper* helper, OOSvrBase::IOHandler<OOSvrBase::AsyncLocalSocket>* handler) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>(helper,handler)
		{
		}

		static AsyncLocalSocket* Create(OOSvrBase::Ev::ProactorImpl* proactor, OOSvrBase::IOHandler<OOSvrBase::AsyncLocalSocket>* handler, OOBase::Socket::socket_t sock)
		{
			IOHelper* helper = 0;
			OOBASE_NEW(helper,IOHelper(proactor,sock));
			if (!helper)
				return 0;

			AsyncLocalSocket* pSock = 0;
			OOBASE_NEW(pSock,AsyncLocalSocket(helper,handler));
			if (!pSock)
			{
				helper->release();
				return 0;
			}

			return pSock;
		}

	private:
		bool is_close(int err) const
		{
			return false;
		}
	};
#endif

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	class SocketAcceptor : public OOBase::Socket
	{
	public:
		SocketAcceptor(OOSvrBase::Ev::ProactorImpl* pProactor, OOBase::Socket::socket_t sock, OOSvrBase::Acceptor<SOCKET_TYPE>* handler);
		virtual ~SocketAcceptor();

		int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			return EINVAL;
		}

		size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			*perr = EINVAL;
			return 0;
		}

		void shutdown(bool bSend, bool bRecv);

		void accept();

	private:
		struct Completion : public OOSvrBase::Ev::ProactorImpl::io_watcher
		{
			SocketAcceptor* m_this_ptr;
		};

		OOBase::Condition::Mutex                 m_lock;
		OOBase::Condition                        m_condition;
		OOSvrBase::Ev::ProactorImpl*             m_proactor;
		Completion                               m_watcher;
		OOSvrBase::Acceptor<SOCKET_TYPE>*        m_handler;
		size_t                                   m_async_count;
		bool                                     m_closed;

		static void on_accept(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher);	
		void do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
	};

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::SocketAcceptor(OOSvrBase::Ev::ProactorImpl* pProactor, OOBase::Socket::socket_t sock, OOSvrBase::Acceptor<SOCKET_TYPE>* handler) :
				m_proactor(pProactor),
				m_handler(handler),
				m_async_count(0),
				m_closed(false)
	{
		m_watcher.m_this_ptr = this;
		m_watcher.callback = &on_accept;
		m_proactor->init_watcher(&m_watcher,sock,EV_READ);

		// Problem with accept()... see: http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#The_special_problem_of_accept_ing_wh
		//void* EV_TODO;
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::~SocketAcceptor()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		m_closed = true;

		closesocket(m_watcher.fd);

		while (m_async_count)
		{
			m_condition.wait(m_lock);
		}
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::accept()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		do_accept(guard);
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::on_accept(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher)
	{
		SocketAcceptor* pThis = static_cast<Completion*>(watcher)->m_this_ptr;

		OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);

		--pThis->m_async_count;
		
		if (pThis->m_closed)
		{
			guard.release();
			pThis->m_condition.signal();
		}
		else
			pThis->do_accept(guard);
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
	{
		// Call accept on the fd...
		int err = 0;
		OOBase::Socket::socket_t new_fd = ::accept(m_watcher.fd,0,0);
		if (new_fd == INVALID_SOCKET)
		{
			err = socket_errno;
			if (err == SOCKET_WOULD_BLOCK)
			{
				++m_async_count;
				m_proactor->start_watcher(&m_watcher);
				return;
			}
		}
		
		// Prepare socket if okay
		if (!err && new_fd != INVALID_SOCKET)
		{
			err = OOBase::BSD::set_close_on_exec(new_fd,true);
			if (!err)
				err = OOBase::BSD::set_non_blocking(new_fd,true);

			if (err)
			{
				closesocket(new_fd);
				new_fd = INVALID_SOCKET;
			}
		}

		// Wrap socket
		SOCKET_TYPE* pSocket = 0;
		if (!err && new_fd != INVALID_SOCKET)
		{
			pSocket = SOCKET_IMPL::Create(m_proactor,new_fd);
			if (!pSocket)
			{
				err = ENOMEM;
				closesocket(new_fd);
			}
		}

		guard.release();

		// Call the handler...
		if (m_handler->on_accept(pSocket,err))
		{
			guard.acquire();

			if (!m_closed)
				do_accept(guard);
		}
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::shutdown(bool /*bSend*/, bool /*bRecv*/)
	{
	}

#if defined(HAVE_SYS_UN_H)
	class LocalSocketAcceptor : public SocketAcceptor<OOSvrBase::AsyncLocalSocket,AsyncLocalSocket>
	{
	public:
		LocalSocketAcceptor(OOSvrBase::Ev::ProactorImpl* pProactor, OOBase::Socket::socket_t sock, OOSvrBase::IOHandler<SOCKET_TYPE>* sock_handler, OOSvrBase::Acceptor<SOCKET_TYPE>* handler, const std::string& path) :
				SocketAcceptor<OOSvrBase::AsyncLocalSocket,AsyncLocalSocket>(pProactor,sock,sock_handler,handler),
				m_path(path)
		{}

		virtual ~LocalSocketAcceptor()
		{
			if (!m_path.empty())
				unlink(m_path.c_str());
		}
	}
#endif
}

OOSvrBase::Ev::ProactorImpl::ProactorImpl() :
		OOSvrBase::Proactor(false),
		m_pLoop(0),
		m_pIOQueue(0),
		m_bStop(false)
{
	// Create an ev loop
	m_pLoop = ev_loop_new(EVFLAG_AUTO | EVFLAG_NOENV);
	if (!m_pLoop)
		OOBase_CallCriticalFailure("ev_loop_new failed");

	// Init the async watcher so we can alert the loop
	ev_async_init(&m_alert,&on_alert);
	m_alert.data = this;

	ev_async_start(m_pLoop,&m_alert);

	// Get number of processors in a better way...
	void* POSIX_TODO;

	// Create the worker pool
	int num_procs = 2;
	for (int i=0; i<num_procs; ++i)
	{
		OOBase::SmartPtr<OOBase::Thread> ptrThread;
		OOBASE_NEW(ptrThread,OOBase::Thread());

		ptrThread->run(&worker,this);

		m_workers.push_back(ptrThread);
	}
}

OOSvrBase::Ev::ProactorImpl::~ProactorImpl()
{
	// Acquire the global lock
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	m_bStop = true;

	// Alert the loop
	ev_async_send(m_pLoop,&m_alert);

	guard.release();

	// Wait for all the threads to finish
	for (std::vector<OOBase::SmartPtr<OOBase::Thread> >::iterator i=m_workers.begin(); i!=m_workers.end(); ++i)
	{
		(*i)->join();
	}

	// Done with the loop
	ev_loop_destroy(m_pLoop);
}

int OOSvrBase::Ev::ProactorImpl::worker(void* param)
{
	try
	{
		return static_cast<ProactorImpl*>(param)->worker_i();
	}
	catch (...)
	{
		return EINVAL;
	}
}

int OOSvrBase::Ev::ProactorImpl::worker_i()
{
	for (std::queue<io_watcher*> io_queue;!m_bStop;)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_ev_lock);

		// Swap over the IO queue to our local one...
		m_pIOQueue = &io_queue;

		// Loop while we are just processing async's
		while (io_queue.empty() && !m_bStop)
		{
			m_bAsyncTriggered = false;

			// Run the ev loop once with the lock held
			ev_loop(m_pLoop,EVLOOP_ONESHOT);

			if (m_bAsyncTriggered)
			{
				// Acquire the global lock
				OOBase::Guard<OOBase::SpinLock> guard2(m_lock);

				// Check for restart of watchers
				while (!m_start_queue.empty())
				{
					io_watcher* i = m_start_queue.front();
					m_start_queue.pop();

					// Add to the loop
					ev_io_start(m_pLoop,i);
				}
			}
		}

		// Release the loop mutex... freeing the next thread to poll...
		guard.release();

		// Process our queue
		while (!io_queue.empty())
		{
			io_watcher* i = io_queue.front();
			io_queue.pop();
		
			if (i->callback)
				i->callback(i);
		}
	}

	return 0;
}

void OOSvrBase::Ev::ProactorImpl::init_watcher(io_watcher* watcher, int fd, int events)
{
	watcher->data = this;
	ev_io_init(watcher,&on_io,fd,events);
}

void OOSvrBase::Ev::ProactorImpl::start_watcher(io_watcher* watcher)
{
	try
	{
		// Add to the queue
		OOBase::Guard<OOBase::SpinLock> guard(m_lock);

		m_start_queue.push(watcher);

		// Alert the loop
		ev_async_send(m_pLoop,&m_alert);
	}
	catch (std::exception& e)
	{
		OOBase_CallCriticalFailure(e.what());
	}
}

void OOSvrBase::Ev::ProactorImpl::on_alert(ev_loop_t*, ev_async* w, int)
{
	static_cast<ProactorImpl*>(w->data)->m_bAsyncTriggered = true;
}

void OOSvrBase::Ev::ProactorImpl::on_io(ev_loop_t*, ev_io* w, int events)
{
	static_cast<ProactorImpl*>(w->data)->on_io_i(static_cast<io_watcher*>(w),events);
}

void OOSvrBase::Ev::ProactorImpl::on_io_i(io_watcher* watcher, int /*events*/)
{
	try
	{
		// Add ourselves to the pending io queue
		m_pIOQueue->push(watcher);

		// Stop the watcher - we can do something
		ev_io_stop(m_pLoop,watcher);
	}
	catch (std::exception& e)
	{
		OOBase_CallCriticalFailure(e.what());
	}
}

OOBase::Socket* OOSvrBase::Ev::ProactorImpl::accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa)
{
#if !defined(HAVE_SYS_UN_H)
	// If we don't have unix sockets, we can't do much, use Win32 Proactor instead
	*perr = ENOENT;

	(void)handler;
	(void)path;
	(void)psa;

	return 0;
#else

	// path is a UNIX pipe name - e.g. /tmp/ooserverd

	*perr = 0;
	int fd = OOBase::socket(PF_UNIX,SOCK_STREAM,0,perr);
	if (fd == -1)
		return 0;
	
	// Compose filename
	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	memset(addr.sun_path,0,sizeof(addr.sun_path));
	path.copy(addr.sun_path,sizeof(addr.sun_path)-1);

	// Bind...
	if (bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) != 0)
	{
		*perr = errno;

		// Unlink any existing inode
		unlink(path.c_str());

		close(fd);
		return 0;
	}

	// Chmod
	mode_t mode = 0777;
	if (psa)
		mode = psa->mode;

	if (chmod(path.c_str(),mode) != 0)
	{
		*perr = errno;
		close(fd);
		return 0;
	}

	// Listen...
	if (listen(fd,0) != 0)
	{
		*perr = errno;
		close(fd);
		return 0;
	}

	// Wrap up in a controlling socket class
	OOBase::SmartPtr<LocalSocketAcceptor> pAccept = 0;
	OOBASE_NEW(pAccept,LocalSocketAcceptor(this,sock,sock_handler,handler,path));
	if (!pAccept)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	pAccept->accept();
	return pAccept.detach();

#endif
}

OOBase::Socket* OOSvrBase::Ev::ProactorImpl::accept_remote(Acceptor<OOSvrBase::AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr)
{
	*perr = 0;
	OOBase::Socket::socket_t sock = INVALID_SOCKET;
	if (address.empty())
	{
		sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_addr.S_un.S_addr = ADDR_ANY;

		if (!port.empty())
			addr.sin_port = htons((u_short)atoi(port.c_str()));

		if ((sock = OOBase::BSD::create_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP,perr)) == INVALID_SOCKET)
			return 0;
		
		if (bind(sock,(sockaddr*)&addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			*perr = socket_errno;
			closesocket(sock);
			return 0;
		}
	}
	else
	{
		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_protocol = IPPROTO_TCP;

		addrinfo* pResults = 0;
		if (getaddrinfo(address.c_str(),port.c_str(),&hints,&pResults) != 0)
		{
			*perr = socket_errno;
			return 0;
		}

		// Loop trying to connect on each address until one succeeds
		for (addrinfo* pAddr = pResults; pAddr != 0; pAddr = pAddr->ai_next)
		{
			if ((sock = OOBase::BSD::create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
			{
				if (bind(sock,pAddr->ai_addr,pAddr->ai_addrlen) == SOCKET_ERROR)
				{
					*perr = socket_errno;
					closesocket(sock);
				}
				else
					break;
			}
		}

		// Done with address info
		freeaddrinfo(pResults);

		// Clear error
		if (sock != INVALID_SOCKET)
			*perr = 0;
	}

	// Now start the socket listening...
	if (listen(sock,SOMAXCONN) == SOCKET_ERROR)
	{
		*perr = socket_errno;
		closesocket(sock);
		return 0;
	}

	// Wrap up in a controlling socket class
	typedef SocketAcceptor<OOSvrBase::AsyncSocket,::AsyncSocket> socket_templ_t;
	OOBase::SmartPtr<socket_templ_t> pAccept = 0;
	OOBASE_NEW(pAccept,socket_templ_t(this,sock,handler));
	if (!pAccept)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	pAccept->accept();
	return pAccept.detach();
}

OOSvrBase::AsyncSocket* OOSvrBase::Ev::ProactorImpl::attach_socket(OOBase::Socket::socket_t sock, int* perr)
{
	// Set non-blocking...
	*perr = OOBase::BSD::set_non_blocking(sock,true);
	if (*perr)
		return 0;

	// Wrap socket
	OOSvrBase::AsyncSocket* pSocket = ::AsyncSocket::Create(this,sock);
	if (!pSocket)
		*perr = ENOMEM;
	
	return pSocket;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Ev::ProactorImpl::connect_local_socket(const std::string& path, int* perr, const OOBase::timeval_t* wait)
{
#if !defined(HAVE_SYS_UN_H)
	// If we don't have unix sockets, we can't do much, use Win32 Proactor instead
	*perr = ENOENT;

	(void)sock;
	
	return 0;
#else

	// Set non-blocking...
	*perr = OOBase::BSD::set_non_blocking(sock,true);
	if (*perr)
		return 0;

	// Wrap socket
	OOSvrBase::AsyncLocalSocket* pSocket = ::AsyncLocalSocket::Create(this,sock);
	if (!pSocket)
		*perr = ENOMEM;
	
	return pSocket;
#endif
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Ev::ProactorImpl::attach_local_socket(OOBase::Socket::socket_t sock, int* perr)
{
	FIX ME!
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // HAVE_EV_H
