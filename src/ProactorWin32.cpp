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

#if defined(_WIN32)

#if !defined(STATUS_PIPE_BROKEN)
#define STATUS_PIPE_BROKEN 0xC000014BL
#endif

#include "../include/OOBase/SmartPtr.h"
#include "../include/OOSvrBase/internal/ProactorImpl.h"
#include "../include/OOSvrBase/internal/ProactorWin32.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4250) // Moans about virtual base classes
#endif

#include <Ws2tcpip.h>
#include <mswsock.h>

namespace
{
	SOCKET create_socket(int family, int socktype, int protocol, int* perr)
	{
		*perr = 0;
		SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET)
			*perr = WSAGetLastError();

		return sock;
	}

	class IOHelper : public OOSvrBase::detail::AsyncIOHelper
	{
	public:
		IOHelper(HANDLE handle);
		virtual ~IOHelper();

		int bind();
		
		void recv(AsyncOp* recv_op);
		void send(AsyncOp* send_op);
		
		void close();		
				
	protected:
		OOBase::Win32::SmartHandle m_handle;

	private:
		void do_read();
		void handle_read(DWORD dwErrorCode);

		void do_write();
		void handle_write(DWORD dwErrorCode);

		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
		static DWORD CALLBACK completion_fn2(LPVOID lpThreadParameter);

		struct Completion
		{
			OVERLAPPED      m_ov;
			AsyncOp*        m_op;
			bool            m_is_reading;
			IOHelper*       m_this_ptr;
			DWORD           m_err;
		};

		Completion m_read_complete;
		Completion m_write_complete;
	};

	class PipeIOHelper : public IOHelper
	{
	public:
		PipeIOHelper(HANDLE handle) : IOHelper(handle)
		{}

		virtual void shutdown(bool /*bSend*/, bool /*bRecv*/)
		{}
	};

	class SocketIOHelper : public IOHelper
	{
	public:
		SocketIOHelper(HANDLE handle) : IOHelper(handle)
		{}

		void shutdown(bool bSend, bool bRecv)
		{
			int how = -1;
			if (bSend)
				how = (bRecv ? SD_BOTH : SD_SEND);
			else if (bRecv)
				how = SD_RECEIVE;

			if (how != -1)
				::shutdown((SOCKET)(HANDLE)m_handle,how);
		}
	};

	IOHelper::IOHelper(HANDLE handle) :
			m_handle(handle)
	{
		m_read_complete.m_is_reading = true;
		m_read_complete.m_op = 0;
		m_read_complete.m_this_ptr = this;
		m_write_complete.m_is_reading = false;
		m_write_complete.m_op = 0;
		m_write_complete.m_this_ptr = this;
	}

	IOHelper::~IOHelper()
	{
	}

	void IOHelper::close()
	{
		m_handle.close();
	}

	int IOHelper::bind()
	{
		if (!OOBase::Win32::BindIoCompletionCallback(m_handle,&completion_fn,0))
			return GetLastError();

		return 0;
	}

	void IOHelper::recv(AsyncOp* recv_op)
	{
		assert(!m_read_complete.m_op);

		// Reset the completion info
		memset(&m_read_complete.m_ov,0,sizeof(OVERLAPPED));
		m_read_complete.m_err = 0;
		m_read_complete.m_op = recv_op;
				
		do_read();
	}

	void IOHelper::send(AsyncOp* send_op)
	{
		assert(!m_write_complete.m_op);

		// Reset the completion info
		memset(&m_write_complete.m_ov,0,sizeof(OVERLAPPED));
		m_write_complete.m_err = 0;
		m_write_complete.m_op = send_op;
				
		do_write();
	}

	VOID CALLBACK IOHelper::completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);

		// Stash error code
		pInfo->m_err = dwErrorCode;

		if (pInfo->m_is_reading)
		{
			// Update wr_ptr
			pInfo->m_op->buffer->wr_ptr(dwNumberOfBytesTransfered);

			if (pInfo->m_op->len)
				pInfo->m_op->len -= dwNumberOfBytesTransfered;
		}
		else
		{
			// Update rd_ptr
			pInfo->m_op->buffer->rd_ptr(dwNumberOfBytesTransfered);
		}

		// Now forward the completion into the worker pool again as a 'long function'
		if (!QueueUserWorkItem(completion_fn2,pInfo,WT_EXECUTELONGFUNCTION))
			OOBase_CallCriticalFailure(GetLastError());
	}

	DWORD IOHelper::completion_fn2(LPVOID lpThreadParameter)
	{
		Completion* pInfo = static_cast<Completion*>(lpThreadParameter);
		if (pInfo->m_is_reading)
			pInfo->m_this_ptr->handle_read(pInfo->m_err);
		else
			pInfo->m_this_ptr->handle_write(pInfo->m_err);	
		return 0;
	}

	void IOHelper::do_read()
	{
		DWORD dwToRead = static_cast<DWORD>(m_read_complete.m_op->len);
		if (dwToRead == 0)
			dwToRead = static_cast<DWORD>(m_read_complete.m_op->buffer->space());

		DWORD dwRead = 0;
		if (!ReadFile(m_handle,m_read_complete.m_op->buffer->wr_ptr(),dwToRead,&dwRead,&m_read_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
				completion_fn(dwErr,dwRead,&m_read_complete.m_ov);
		}
	}

	void IOHelper::handle_read(DWORD dwErrorCode)
	{
		if (dwErrorCode == 0 && m_read_complete.m_op->len > 0)
		{
			// More to read
			return do_read();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_read_complete.m_op;
		m_read_complete.m_op = 0;

		// Call handlers
		m_handler->on_recv(op,dwErrorCode);
	}

	void IOHelper::do_write()
	{
		DWORD dwWritten = 0;
		if (!WriteFile(m_handle,m_write_complete.m_op->buffer->rd_ptr(),static_cast<DWORD>(m_write_complete.m_op->buffer->length()),&dwWritten,&m_write_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
				completion_fn(dwErr,dwWritten,&m_write_complete.m_ov);
		}
	}

	void IOHelper::handle_write(DWORD dwErrorCode)
	{
		if (dwErrorCode == 0 && m_write_complete.m_op->buffer->length() > 0)
		{
			// More to send
			return do_write();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_write_complete.m_op;
		m_write_complete.m_op = 0;

		// Call handlers
		m_handler->on_sent(op,dwErrorCode);
	}

	class AsyncSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>
	{
	public:
		AsyncSocket(IOHelper* helper) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>(helper)
		{
		}

		static AsyncSocket* Create(SOCKET sock, DWORD& dwErr)
		{
			IOHelper* helper = 0;
			OOBASE_NEW(helper,SocketIOHelper((HANDLE)sock));
			if (!helper)
			{
				dwErr = ERROR_OUTOFMEMORY;
				return 0;
			}

			dwErr = helper->bind();
			if (dwErr)
			{
				delete helper;
				return 0;
			}

			AsyncSocket* pSock = 0;
			OOBASE_NEW(pSock,AsyncSocket(helper));
			if (!pSock)
			{
				dwErr = ERROR_OUTOFMEMORY;
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
			case ERROR_INVALID_HANDLE:
			case ERROR_NETNAME_DELETED:
			case WSAECONNABORTED:
			case WSAECONNRESET:
				return true;

			default:
				return false;
			}
		}
	};

	class SocketAcceptor : public OOBase::Socket
	{
	public:
		SocketAcceptor(SOCKET sock, OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>* handler, int address_family);
		virtual ~SocketAcceptor();

		int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			return ERROR_INVALID_FUNCTION;
		}

		size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			*perr = ERROR_INVALID_FUNCTION;
			return 0;
		}

		void shutdown(bool bSend, bool bRecv);

		int accept();

	private:
		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

		struct Completion
		{
			OVERLAPPED                 m_ov;
			SOCKET                     m_socket;
			SocketAcceptor*            m_this_ptr;
			char                       m_addresses[(sizeof(sockaddr_in6) + 16)*2];
		};

		OOBase::Condition::Mutex                      m_lock;
		OOBase::Condition                             m_condition;
		bool                                          m_closed;
		Completion                                    m_completion;
		SOCKET                                        m_socket;
		OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>*  m_handler;
		size_t                                        m_async_count;
		int                                           m_address_family;
		LPFN_ACCEPTEX                                 m_lpfnAcceptEx;
				
		void do_accept(DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
		void next_accept();
	};

	SocketAcceptor::SocketAcceptor(SOCKET sock, OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>* handler, int address_family) :
			m_closed(false),
			m_socket(sock),
			m_handler(handler),
			m_async_count(0),
			m_address_family(address_family),
			m_lpfnAcceptEx(0)
	{
		assert(address_family == AF_INET || address_family == AF_INET6);
		assert(m_socket != INVALID_SOCKET);

		m_completion.m_this_ptr = this;
	}

	SocketAcceptor::~SocketAcceptor()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		m_closed = true;

		closesocket(m_socket);

		while (m_async_count)
		{
			m_condition.wait(m_lock);
		}
	}

	int SocketAcceptor::accept()
	{
		//----------------------------------------
		// Load the AcceptEx function into memory using WSAIoctl.
		// The WSAIoctl function is an extension of the ioctlsocket()
		// function that can use overlapped I/O. The function's 3rd
		// through 6th parameters are input and output buffers where
		// we pass the pointer to our AcceptEx function. This is used
		// so that we can call the AcceptEx function directly, rather
		// than refer to the Mswsock.lib library.
		static const GUID guid_AcceptEx = WSAID_ACCEPTEX;

		DWORD dwBytes;
		if (WSAIoctl(m_socket, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			(void*)&guid_AcceptEx, 
			sizeof(guid_AcceptEx),
			&m_lpfnAcceptEx, 
			sizeof(m_lpfnAcceptEx), 
			&dwBytes, 
			NULL, 
			NULL) != 0)
		{
			int err = WSAGetLastError();
			closesocket(m_socket);
			return err;
		}

		if (!OOBase::Win32::BindIoCompletionCallback((HANDLE)m_socket,&completion_fn,0))
		{
			int err = GetLastError();
			closesocket(m_socket);
			return err;
		}

		// Just accept the next one...
		next_accept();

		return 0;
	}

	void SocketAcceptor::next_accept()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		if (m_closed)
			return;

		memset(&m_completion.m_ov,0,sizeof(OVERLAPPED));

		int err = 0;
		m_completion.m_socket = create_socket(m_address_family,SOCK_STREAM,IPPROTO_TCP,&err);
		if (!err)
		{
			++m_async_count;

			DWORD dwBytes = 0;
			if (!(*m_lpfnAcceptEx)(m_socket,m_completion.m_socket,m_completion.m_addresses,0,sizeof(m_completion.m_addresses)/2,sizeof(m_completion.m_addresses)/2,&dwBytes,&m_completion.m_ov))
			{
				err = WSAGetLastError();
				if (err == ERROR_IO_PENDING)
				{
					// Async - finish later...
					return;
				}
			}

			--m_async_count;
		}

		do_accept(err,guard);
	}

	VOID CALLBACK SocketAcceptor::completion_fn(DWORD dwErrorCode, DWORD, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
		
		OOBase::Guard<OOBase::Condition::Mutex> guard(pInfo->m_this_ptr->m_lock);

		--pInfo->m_this_ptr->m_async_count;

		pInfo->m_this_ptr->do_accept(dwErrorCode,guard);
	}

	void SocketAcceptor::do_accept(DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
	{
		if (m_closed)
		{
			if (m_completion.m_socket != INVALID_SOCKET)
				closesocket(m_completion.m_socket);

			guard.release();
			m_condition.signal();
			return;
		}
		
		AsyncSocket* pSocket = 0;
		if (dwErr == 0)
		{
			pSocket = AsyncSocket::Create(m_completion.m_socket,dwErr);
			if (!pSocket && m_completion.m_socket != INVALID_SOCKET)
				closesocket(m_completion.m_socket);
		}

		guard.release();

		// Call the acceptor
		if (m_handler->on_accept(pSocket,dwErr))
			next_accept();
	}

	void SocketAcceptor::shutdown(bool /*bSend*/, bool /*bRecv*/)
	{
	}

	class AsyncLocalSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>
	{
	public:
		AsyncLocalSocket(HANDLE hPipe, PipeIOHelper* helper) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>(helper),
				m_hPipe(hPipe)
		{
		}

		static AsyncLocalSocket* Create(HANDLE hPipe, DWORD& dwErr)
		{
			PipeIOHelper* helper = 0;
			OOBASE_NEW(helper,PipeIOHelper(hPipe));
			if (!helper)
			{
				dwErr = ERROR_OUTOFMEMORY;
				return 0;
			}

			dwErr = helper->bind();
			if (dwErr)
			{
				delete helper;
				return 0;
			}

			AsyncLocalSocket* sock = 0;
			OOBASE_NEW(sock,AsyncLocalSocket(hPipe,helper));
			if (!sock)
			{
				dwErr = ERROR_OUTOFMEMORY;
				delete helper;
				return 0;
			}

			return sock;
		}

	private:
		bool is_close(int err) const
		{
			return  (err == STATUS_PIPE_BROKEN);
		}

		HANDLE m_hPipe;

		int get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid);
	};

	int AsyncLocalSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
	{
		if (!ImpersonateNamedPipeClient(m_hPipe))
			return GetLastError();

		OOBase::Win32::SmartHandle ptruid;
		BOOL bRes = OpenThreadToken(GetCurrentThread(),TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE,FALSE,&ptruid);
		DWORD err = 0;
		if (!bRes)
			err = GetLastError();

		if (!RevertToSelf())
			OOBase_CallCriticalFailure(GetLastError());
		
		if (!bRes)
			return err;

		uid = ptruid.detach();
		return 0;
	}

	class PipeAcceptor : public OOBase::Socket
	{
	public:
		PipeAcceptor(const std::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler);
		virtual ~PipeAcceptor();

		int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			return ERROR_INVALID_FUNCTION;
		}

		size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			*perr = ERROR_INVALID_FUNCTION;
			return 0;
		}

		void shutdown(bool bSend, bool bRecv);

		void accept();

	private:
		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

		struct Completion
		{
			OVERLAPPED                 m_ov;
			OOBase::Win32::SmartHandle m_hPipe;
			PipeAcceptor*              m_this_ptr;
		};

		OOBase::Condition::Mutex                           m_lock;
		OOBase::Condition                                  m_condition;
		bool                                               m_closed;
		Completion                                         m_completion;
		OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>*  m_handler;
		const std::string                                  m_pipe_name;
		LPSECURITY_ATTRIBUTES                              m_psa;
		size_t                                             m_async_count;
		
		void next_accept(bool bExclusive);
		void do_accept(DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
	};

	PipeAcceptor::PipeAcceptor(const std::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler) :
			m_closed(false),
			m_handler(handler),
			m_pipe_name(pipe_name),
			m_psa(psa),
			m_async_count(0)
	{
		m_completion.m_this_ptr = this;
	}

	PipeAcceptor::~PipeAcceptor()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		m_closed = true;

		m_completion.m_hPipe.close();

		while (m_async_count)
		{
			m_condition.wait(m_lock);
		}
	}

	void PipeAcceptor::accept()
	{
		next_accept(true);
	}

	void PipeAcceptor::next_accept(bool bExclusive)
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		assert(!m_completion.m_hPipe.is_valid());

		if (m_closed)
			return;

		DWORD dwOpenMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;

		if (bExclusive)
			dwOpenMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;

		DWORD dwErr = 0;
		m_completion.m_hPipe = CreateNamedPipeA(m_pipe_name.c_str(),dwOpenMode,
								   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
								   PIPE_UNLIMITED_INSTANCES,
								   8192,8192,0,m_psa);

		if (!m_completion.m_hPipe.is_valid())
			dwErr = GetLastError();
		else if (!OOBase::Win32::BindIoCompletionCallback(m_completion.m_hPipe,&completion_fn,0))
		{
			dwErr = GetLastError();
			m_completion.m_hPipe.close();
		}

		if (dwErr == 0)
		{
			memset(&m_completion.m_ov,0,sizeof(OVERLAPPED));

			++m_async_count;

			if (!ConnectNamedPipe(m_completion.m_hPipe,&m_completion.m_ov))
			{
				dwErr = GetLastError();
				if (dwErr == ERROR_IO_PENDING)
				{
					// Will complete later...
					return;
				}
			}

			--m_async_count;
		}

		do_accept(dwErr,guard);
	}

	VOID CALLBACK PipeAcceptor::completion_fn(DWORD dwErrorCode, DWORD, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
		
		OOBase::Guard<OOBase::Condition::Mutex> guard(pInfo->m_this_ptr->m_lock);

		--pInfo->m_this_ptr->m_async_count;

		pInfo->m_this_ptr->do_accept(dwErrorCode,guard);
	}

	void PipeAcceptor::do_accept(DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
	{
		if (m_closed)
		{
			m_completion.m_hPipe.close();
			
			guard.release();
			m_condition.signal();
			return;
		}

		AsyncLocalSocket* pSocket = 0;
		if (dwErr == 0)
		{
			pSocket = AsyncLocalSocket::Create(m_completion.m_hPipe,dwErr);
			if (pSocket)
				m_completion.m_hPipe.detach();
			else
				m_completion.m_hPipe.close();
		}

		guard.release();

		// Call the acceptor
		if (m_handler->on_accept(pSocket,dwErr))
			next_accept(false);
	}

	void PipeAcceptor::shutdown(bool /*bSend*/, bool /*bRecv*/)
	{
	}
}

OOSvrBase::Win32::ProactorImpl::ProactorImpl() :
		OOSvrBase::Proactor(false)
{
}

OOSvrBase::Win32::ProactorImpl::~ProactorImpl()
{
}

OOBase::Socket* OOSvrBase::Win32::ProactorImpl::accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa)
{
	OOBase::SmartPtr<PipeAcceptor> pAcceptor = 0;
	OOBASE_NEW(pAcceptor,PipeAcceptor("\\\\.\\pipe\\" + path,psa,handler));
	if (!pAcceptor)
	{
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}
		
	pAcceptor->accept();
	return pAcceptor.detach();
}

OOBase::Socket* OOSvrBase::Win32::ProactorImpl::accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr)
{
	*perr = 0;

	int af_family = AF_INET;
	OOBase::Socket::socket_t sock = INVALID_SOCKET;
	if (address.empty())
	{
		sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_addr.S_un.S_addr = ADDR_ANY;

		if (!port.empty())
			addr.sin_port = htons((u_short)atoi(port.c_str()));

		if ((sock = create_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP,perr)) == INVALID_SOCKET)
			return 0;
		
		if (bind(sock,(sockaddr*)&addr,sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			*perr = WSAGetLastError();
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
			*perr = WSAGetLastError();
			return 0;
		}

		// Loop trying to connect on each address until one succeeds
		for (addrinfo* pAddr = pResults; pAddr != 0; pAddr = pAddr->ai_next)
		{
			if ((sock = create_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,perr)) != INVALID_SOCKET)
			{
				if (bind(sock,pAddr->ai_addr,pAddr->ai_addrlen) == SOCKET_ERROR)
				{
					*perr = WSAGetLastError();
					closesocket(sock);
				}
				else
				{
					af_family = pAddr->ai_family;
					break;
				}
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
		*perr = WSAGetLastError();
		closesocket(sock);
		return 0;
	}

	// Wrap up in a controlling socket class
	OOBase::SmartPtr<SocketAcceptor> pAccept = 0;
	OOBASE_NEW(pAccept,SocketAcceptor(sock,handler,af_family));
	if (!pAccept)
	{
		*perr = ENOMEM;
		closesocket(sock);
		return 0;
	}

	*perr = pAccept->accept();
	if (*perr)
		return 0;

	return pAccept.detach();
}

OOSvrBase::AsyncSocket* OOSvrBase::Win32::ProactorImpl::attach_socket(OOBase::Socket::socket_t sock, int* perr)
{
	// The socket must have been opened as WSA_FLAG_OVERLAPPED!!!

	DWORD dwErr = 0;
	OOSvrBase::AsyncSocket* pSocket = ::AsyncSocket::Create(sock,dwErr);
	*perr = dwErr;	
	return pSocket;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::attach_local_socket(OOBase::Socket::socket_t sock, int* perr)
{
	// Wrap socket
	DWORD dwErr = 0;
	OOSvrBase::AsyncLocalSocket* pSocket = ::AsyncLocalSocket::Create((HANDLE)sock,dwErr);
	*perr = dwErr;	
	return pSocket;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::connect_local_socket(const std::string& path, int* perr, const OOBase::timeval_t* wait)
{
	assert(perr);
	*perr = 0;

	std::string pipe_name = "\\\\.\\pipe\\" + path;

	OOBase::timeval_t wait2 = (wait ? *wait : OOBase::timeval_t::MaxTime);
	OOBase::Countdown countdown(&wait2);

	OOBase::Win32::SmartHandle hPipe;
	while (wait2 != OOBase::timeval_t::Zero)
	{
		hPipe = CreateFileA(pipe_name.c_str(),
							PIPE_ACCESS_DUPLEX,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_FLAG_OVERLAPPED,
							NULL);

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		DWORD dwErr = GetLastError();
		if (dwErr != ERROR_PIPE_BUSY)
		{
			*perr = dwErr;
			return 0;
		}

		DWORD dwWait = NMPWAIT_USE_DEFAULT_WAIT;
		if (wait)
		{
			countdown.update();

			dwWait = wait2.msec();
		}

		if (!WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			*perr = GetLastError();
			return 0;
		}
	}

	// Wrap socket
	DWORD dwErr = 0;
	OOSvrBase::AsyncLocalSocket* pSocket = ::AsyncLocalSocket::Create(hPipe,dwErr);
	*perr = dwErr;

	if (pSocket)
		hPipe.detach();
	else
		hPipe.close();
	
	return pSocket;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // _WIN32
