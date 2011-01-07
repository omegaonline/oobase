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

#include <algorithm>

#if defined(HAVE_EV_H)

#include "../include/OOBase/internal/BSDSocket.h"
#include "../include/OOBase/Posix.h"
#include "../include/OOSvrBase/internal/ProactorImpl.h"
#include "../include/OOSvrBase/internal/ProactorEv.h"

#if defined(HAVE_UNISTD_H)
#include <sys/un.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#if defined(_WIN32)
#include <ws2tcpip.h>

#define ssize_t int
#define SHUT_RDWR SD_BOTH
#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define socket_errno WSAGetLastError()
#define SOCKET_WOULD_BLOCK WSAEWOULDBLOCK
#define EINPROGRESS WSAEWOULDBLOCK
#define ETIMEDOUT WSAETIMEDOUT

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
	int connect_i(OOBase::Socket::socket_t sock, const sockaddr* addr, size_t addrlen, const OOBase::timeval_t* wait)
	{
		// Do the connect
		if (::connect(sock,addr,addrlen) != -1)
			return 0;

		// Check to see if we actually have an error
		int err = socket_errno;
		if (err != EINPROGRESS)
			return err;

		// Wait for the connect to go ahead - by select()ing on write
		::timeval tv;
		if (wait)
		{
			tv.tv_sec = static_cast<long>(wait->tv_sec());
			tv.tv_usec = wait->tv_usec();
		}

		fd_set wfds;
		fd_set efds; // Annoyingly Winsock uses the exceptions not just the writes

		for (;;)
		{
			FD_ZERO(&wfds);
			FD_ZERO(&efds);
			FD_SET(sock,&wfds);
			FD_SET(sock,&efds);

			int count = select(sock+1,0,&wfds,&efds,wait ? &tv : 0);
			if (count == -1)
			{
				err = socket_errno;
				if (err != EINTR)
					return err;
			}
			else if (count == 1)
			{
				// If connect() did something...
				socklen_t len = sizeof(err);
				if (getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len) == -1)
					return socket_errno;

				return err;
			}
			else
				return ETIMEDOUT;
		}
	}

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
			bool            m_last;
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
		m_read_complete.m_last = false;
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
		m_read_complete.m_last = false;

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
		size_t to_read = m_read_complete.m_op->len;
		if (to_read == 0)
			to_read = m_read_complete.m_op->buffer->space();

		ssize_t have_read = ::recv(m_read_complete.fd,m_read_complete.m_op->buffer->wr_ptr(),to_read,0);
		if (have_read >= 0)
		{
			handle_read(0,have_read);
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

		m_read_complete.m_last = (have_read == 0);

		if (err == 0 && m_read_complete.m_op->len > 0 && !m_read_complete.m_last)
		{
			// More to read
			return do_read();
		}

		// Stash op... another op may be about to occur...
		AsyncOp* op = m_read_complete.m_op;
		m_read_complete.m_op = 0;

		// Call handlers
		m_handler->on_recv(op,err,m_read_complete.m_last);
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
			IOHelper* helper;
			OOBASE_NEW_T(IOHelper,helper,IOHelper(proactor,sock));
			if (!helper)
				return 0;

			AsyncSocket* pSock;
			OOBASE_NEW_T(AsyncSocket,pSock,AsyncSocket(helper));
			if (!pSock)
			{
				OOBASE_DELETE(IOHelper,helper);
				return 0;
			}

			return pSock;
		}
	};

#if defined(HAVE_UNISTD_H)
	class AsyncLocalSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>
	{
	public:
		AsyncLocalSocket(IOHelper* helper, OOBase::Socket::socket_t sock) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>(helper),
				m_fd(sock)
		{
		}

		static AsyncLocalSocket* Create(OOSvrBase::Ev::ProactorImpl* proactor, OOBase::Socket::socket_t sock)
		{
			IOHelper* helper;
			OOBASE_NEW_T(IOHelper,helper,IOHelper(proactor,sock));
			if (!helper)
				return 0;

			AsyncLocalSocket* pSock;
			OOBASE_NEW_T(AsyncLocalSocket,pSock,AsyncLocalSocket(helper,sock));
			if (!pSock)
			{
				OOBASE_DELETE(IOHelper,helper);
				return 0;
			}

			return pSock;
		}

	private:
		OOBase::Socket::socket_t m_fd;

		int get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid);
	};

	int AsyncLocalSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
	{
#if defined(HAVE_GETPEEREID)
		/* OpenBSD style:  */
		gid_t gid;
		if (getpeereid(m_fd, &uid, &gid) != 0)
		{
			/* We didn't get a valid credentials struct. */
			return errno;
		}
		return 0;

#elif defined(HAVE_SO_PEERCRED)
		/* Linux style: use getsockopt(SO_PEERCRED) */
		ucred peercred;
		socklen_t so_len = sizeof(peercred);

		if (getsockopt(m_fd, SOL_SOCKET, SO_PEERCRED, &peercred, &so_len) != 0 || so_len != sizeof(peercred))
		{
			/* We didn't get a valid credentials struct. */
			return errno;
		}
		uid = peercred.uid;
		return 0;

#elif defined(HAVE_GETPEERUCRED)
		/* Solaris > 10 */
		ucred_t* ucred = NULL; /* must be initialized to NULL */
		if (getpeerucred(m_fd, &ucred) != 0)
		{
			/* We didn't get a valid credentials struct. */
			return errno;
		}

		if ((uid = ucred_geteuid(ucred)) == -1)
		{
			int err = errno;
			ucred_free(ucred);
			return err;
		}
		return 0;

#elif (defined(HAVE_STRUCT_CMSGCRED) || defined(HAVE_STRUCT_FCRED) || defined(HAVE_STRUCT_SOCKCRED)) && defined(HAVE_LOCAL_CREDS)

		/*
		* Receive credentials on next message receipt, BSD/OS,
		* NetBSD. We need to set this before the client sends the
		* next packet.
		*/
		int on = 1;
		if (setsockopt(m_fd, 0, LOCAL_CREDS, &on, sizeof(on)) != 0)
			return errno;

		/* Credentials structure */
#if defined(HAVE_STRUCT_CMSGCRED)
		typedef cmsgcred Cred;
		#define cruid cmcred_uid
#elif defined(HAVE_STRUCT_FCRED)
		typedef fcred Cred;
		#define cruid fc_uid
#elif defined(HAVE_STRUCT_SOCKCRED)
		typedef sockcred Cred;
		#define cruid sc_uid
#endif
		/* Compute size without padding */
		char cmsgmem[ALIGN(sizeof(struct cmsghdr)) + ALIGN(sizeof(Cred))];   /* for NetBSD */

		/* Point to start of first structure */
		struct cmsghdr* cmsg = (struct cmsghdr*)cmsgmem;
		struct iovec iov;

		msghdr msg;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = (char *) cmsg;
		msg.msg_controllen = sizeof(cmsgmem);
		memset(cmsg, 0, sizeof(cmsgmem));

		/*
		 * The one character which is received here is not meaningful; its
		 * purposes is only to make sure that recvmsg() blocks long enough for the
		 * other side to send its credentials.
		 */
		char buf;
		iov.iov_base = &buf;
		iov.iov_len = 1;

		if (recvmsg(m_fd, &msg, 0) < 0 || cmsg->cmsg_len < sizeof(cmsgmem) || cmsg->cmsg_type != SCM_CREDS)
			return errno;

		Cred* cred = (Cred*)CMSG_DATA(cmsg);
		uid = cred->cruid;
		return 0;

#else
		// We can't handle this situation
		#error Fix me!
#endif
	}

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

		int accept();

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
		int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
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

		if (m_async_count)
		{
			m_proactor->stop_watcher(&m_watcher);

			while (m_async_count)
				m_condition.wait(m_lock);
		}
		
		closesocket(m_watcher.fd);
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	int SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::accept()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		return do_accept(guard);
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::on_accept(OOSvrBase::Ev::ProactorImpl::io_watcher* watcher)
	{
		SocketAcceptor* pThis = static_cast<Completion*>(watcher)->m_this_ptr;

		OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);

		--pThis->m_async_count;

		pThis->do_accept(guard);
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	int SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
	{
		int err = 0;
		while (!m_closed)
		{
			// Call accept on the fd...
			OOBase::Socket::socket_t new_fd = ::accept(m_watcher.fd,0,0);
			if (new_fd == INVALID_SOCKET)
			{
				err = socket_errno;
				if (err == SOCKET_WOULD_BLOCK)
				{
					++m_async_count;
					m_proactor->start_watcher(&m_watcher);
					return 0;
				}
			}

			guard.release();

			// Prepare socket if okay
			if (!err && new_fd != INVALID_SOCKET)
			{
				err = OOBase::BSD::set_non_blocking(new_fd,true);

	#if defined (HAVE_UNISTD_H)
				if (!err)
					err = OOBase::POSIX::set_close_on_exec(new_fd,true);				
	#endif
				if (err)
				{
					closesocket(new_fd);
					new_fd = INVALID_SOCKET;
				}
			}

			OOBase::stack_string strAddress;
			if (!err && new_fd != INVALID_SOCKET)
			{
				// Get the address...
				union
				{
					sockaddr m_addr;
					char     m_buf[1024];
				} address;

				socklen_t len = sizeof(address);
				if (getpeername(new_fd,&address.m_addr,&len) != 0)
				{
					err = errno;
					closesocket(new_fd);
					new_fd = INVALID_SOCKET;
				}

				if (address.m_addr.sa_family == AF_INET ||
					address.m_addr.sa_family == AF_INET6)
				{
					char szBuf[INET6_ADDRSTRLEN+1] = {0};
					strAddress = inet_ntop(address.m_addr.sa_family,&address.m_addr,szBuf,INET6_ADDRSTRLEN);
				}
			}

			// Wrap socket
			OOBase::SmartPtr<SOCKET_TYPE,OOSvrBase::SocketDestructor> ptrSocket;
			if (!err && new_fd != INVALID_SOCKET)
			{
				ptrSocket = SOCKET_IMPL::Create(m_proactor,new_fd);
				if (!ptrSocket)
				{
					err = ENOMEM;
					closesocket(new_fd);
				}
			}

			// Call the handler...
			bool bAgain = m_handler->on_accept(ptrSocket,strAddress.c_str(),err);

			guard.acquire();

			if (!bAgain)
				break;

			err = 0;
		} 

		if (m_closed)
			m_condition.signal();

		return err;
	}

	template <typename SOCKET_TYPE, typename SOCKET_IMPL>
	void SocketAcceptor<SOCKET_TYPE,SOCKET_IMPL>::shutdown(bool /*bSend*/, bool /*bRecv*/)
	{
	}

#if defined(HAVE_UNISTD_H)
	class LocalSocketAcceptor : public SocketAcceptor<OOSvrBase::AsyncLocalSocket,AsyncLocalSocket>
	{
	public:
		LocalSocketAcceptor(OOSvrBase::Ev::ProactorImpl* pProactor, OOBase::Socket::socket_t sock, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler, const char* path) :
				SocketAcceptor<OOSvrBase::AsyncLocalSocket,AsyncLocalSocket>(pProactor,sock,handler),
				m_path(path)
		{}

		virtual ~LocalSocketAcceptor()
		{
			if (!m_path.empty())
				unlink(m_path.c_str());
		}

	private:
		const OOBase::string m_path;
	};
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

	// Create the worker pool
	int num_procs = 2;
	for (int i=0; i<num_procs; ++i)
	{
		OOBase::SmartPtr<OOBase::Thread> ptrThread;
		OOBASE_NEW_T_CRITICAL(OOBase::Thread,ptrThread,OOBase::Thread(false));

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
	for (workerDeque::iterator i=m_workers.begin(); i!=m_workers.end(); ++i)
		(*i)->join();
	
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
	for (dequeType io_queue;!m_bStop;)
	{
		// Make this a condition variable and spawn more threads on demand...
		void* TODO;
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
					m_start_queue.pop_front();

					// Add to the loop
					ev_io_start(m_pLoop,i);
				}

				// Check for stop of watchers
				while (!m_stop_queue.empty())
				{
					io_watcher* i = m_stop_queue.front();
					m_stop_queue.pop_front();

					// Stop it
					ev_io_stop(m_pLoop,i);

					// And add to the io_queue
					io_queue.push_back(i);
				}
			}
		}

		// Make sure we have stops for non-pendings...
		while (!m_stop_queue.empty())
		{
			io_watcher* i = m_stop_queue.front();
			m_stop_queue.pop_front();

			dequeType::iterator j = std::find(io_queue.begin(),io_queue.end(),i);
			if (j == io_queue.end())
			{
				// Stop it
				ev_io_stop(m_pLoop,i);

				// And add to the io_queue
				io_queue.push_back(i);
			}
		}

		// Release the loop mutex... freeing the next thread to poll...
		guard.release();

		// Process our queue
		while (!io_queue.empty())
		{
			io_watcher* i = io_queue.front();
			io_queue.pop_front();

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
	// Add to the queue
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	m_start_queue.push_back(watcher);

	// Alert the loop
	ev_async_send(m_pLoop,&m_alert);
}

void OOSvrBase::Ev::ProactorImpl::stop_watcher(io_watcher* watcher)
{
	// Add to the queue
	OOBase::Guard<OOBase::SpinLock> guard(m_lock);

	m_stop_queue.push_back(watcher);

	// Alert the loop
	ev_async_send(m_pLoop,&m_alert);
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
	// Add ourselves to the pending io queue
	m_pIOQueue->push_back(watcher);

	// Stop the watcher - we can do something
	ev_io_stop(m_pLoop,watcher);
}

OOBase::Socket* OOSvrBase::Ev::ProactorImpl::accept_local(Acceptor<AsyncLocalSocket>* handler, const char* path, int* perr, SECURITY_ATTRIBUTES* psa)
{
#if !defined(HAVE_UNISTD_H)
	// If we don't have unix sockets, we can't do much, use Win32 Proactor instead
	*perr = ENOENT;

	(void)handler;
	(void)path;
	(void)psa;

	return 0;
#else

	// path is a UNIX pipe name - e.g. /tmp/ooserverd

	*perr = 0;
	int fd = OOBase::BSD::create_socket(PF_UNIX,SOCK_STREAM,0,perr);
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
	OOBase::SmartPtr<LocalSocketAcceptor> pAccept;
	OOBASE_NEW_T(LocalSocketAcceptor,pAccept,LocalSocketAcceptor(this,fd,handler,path));
	if (!pAccept)
	{
		*perr = ENOMEM;
		closesocket(fd);
		return 0;
	}

	*perr = pAcceptor->accept();
	if (*perr != 0)
		return 0;

	return pAcceptor.detach();

#endif
}

OOBase::Socket* OOSvrBase::Ev::ProactorImpl::accept_remote(Acceptor<OOSvrBase::AsyncSocket>* handler, const char* address, const char* port, int* perr)
{
	*perr = 0;
	OOBase::Socket::socket_t sock = INVALID_SOCKET;
	if (address.empty())
	{
		sockaddr_in addr = {0};
		addr.sin_family = AF_INET;

#if defined(_WIN32)
		addr.sin_addr.S_un.S_addr = ADDR_ANY;
#else
		addr.sin_addr.s_addr = INADDR_ANY;
#endif

		if (!port.empty())
			addr.sin_port = htons((u_short)atoi(port.c_str()));

		if ((sock = OOBase::BSD::create_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP,perr)) == INVALID_SOCKET)
			return 0;

		if (bind(sock,(sockaddr*)&addr,sizeof(sockaddr_in)) == -1)
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

		// Loop trying to bind on each address until one succeeds
		for (addrinfo* pAddr = pResults; pAddr != 0; pAddr = pAddr->ai_next)
		{
			if ((sock = OOBase::BSD::create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
			{
				if (bind(sock,pAddr->ai_addr,pAddr->ai_addrlen) == -1)
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
	if (listen(sock,SOMAXCONN) == -1)
	{
		*perr = socket_errno;
		closesocket(sock);
		return 0;
	}

	// Wrap up in a controlling socket class
	typedef SocketAcceptor<OOSvrBase::AsyncSocket,::AsyncSocket> socket_templ_t;
	OOBase::SmartPtr<socket_templ_t> pAccept;
	OOBASE_NEW_T(socket_templ_t,pAccept,socket_templ_t(this,sock,handler));
	if (!pAccept)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	*perr = pAcceptor->accept();
	if (*perr != 0)
		return 0;

	return pAcceptor.detach();
}

OOSvrBase::AsyncSocketPtr OOSvrBase::Ev::ProactorImpl::attach_socket(OOBase::Socket::socket_t sock, int* perr)
{
	// Set non-blocking...
	*perr = OOBase::BSD::set_non_blocking(sock,true);
	if (*perr)
		return 0;

	// Wrap socket
	OOSvrBase::AsyncSocketPtr ptrSocket = ::AsyncSocket::Create(this,sock);
	if (!ptrSocket)
		*perr = ENOMEM;

	return ptrSocket;
}

OOSvrBase::AsyncLocalSocketPtr OOSvrBase::Ev::ProactorImpl::connect_local_socket(const char* path, int* perr, const OOBase::timeval_t* wait)
{
#if !defined(HAVE_UNISTD_H)
	// If we don't have unix sockets, we can't do much, use Win32 Proactor instead
	*perr = ENOENT;

	(void)path;
	(void)wait;

	return 0;
#else

	OOBase::Socket::socket_t sock = OOBase::BSD::create_socket(AF_UNIX,SOCK_STREAM,0,perr);
	if (sock == -1)
		return 0;

	sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	memset(addr.sun_path,0,sizeof(addr.sun_path));
	path.copy(addr.sun_path,sizeof(addr.sun_path)-1);

	if ((*perr = connect_i(sock,(sockaddr*)(&addr),sizeof(addr),wait)) != 0)
	{
		::close(sock);
		return 0;
	}

	// Set non-blocking...
	*perr = OOBase::BSD::set_non_blocking(sock,true);
	if (*perr)
		return 0;

	// Wrap socket
	OOSvrBase::AsyncLocalSocketPtr ptrSocket = ::AsyncLocalSocket::Create(this,sock);
	if (!ptrSocket)
		*perr = ENOMEM;

	return ptrSocket;
#endif
}

OOSvrBase::AsyncLocalSocketPtr OOSvrBase::Ev::ProactorImpl::attach_local_socket(OOBase::Socket::socket_t sock, int* perr)
{
#if !defined(HAVE_UNISTD_H)
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
	OOSvrBase::AsyncLocalSocketPtr ptrSocket = ::AsyncLocalSocket::Create(this,sock);
	if (!ptrSocket)
		*perr = ENOMEM;

	return ptrSocket;

#endif
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // HAVE_EV_H
