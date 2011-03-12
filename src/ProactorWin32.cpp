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

#include "../include/OOBase/SmartPtr.h"
#include "../include/OOBase/Thread.h"

#include "Win32Socket.h"
#include "ProactorWin32.h"
#include "Win32Impl.h"


namespace
{
	//class IOHelper : public OOSvrBase::detail::AsyncIOHelper
	//{
	//public:
	//	IOHelper(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle);
	//	virtual ~IOHelper();

	//	int bind();
	//	
	//	void recv(AsyncOp* recv_op);
	//	void send(AsyncOp* send_op);
	//	
	//	void close();		
	//			
	//protected:
	//	OOBase::Win32::SmartHandle m_handle;

	//private:
	//	void do_read();
	//	void handle_read(DWORD dwErrorCode);

	//	void do_write();
	//	void handle_write(DWORD dwErrorCode);

	//	static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
	//	static DWORD CALLBACK completion_fn2(LPVOID lpThreadParameter);

	//	struct Completion
	//	{
	//		OVERLAPPED      m_ov;
	//		AsyncOp*        m_op;
	//		bool            m_is_reading;
	//		IOHelper*       m_this_ptr;
	//		DWORD           m_err;
	//		bool            m_last;
	//	};

	//	Completion m_read_complete;
	//	Completion m_write_complete;

	//	OOSvrBase::Win32::ProactorImpl* const m_pProactor;
	//};

	//class PipeIOHelper : public IOHelper
	//{
	//public:
	//	PipeIOHelper(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle) : IOHelper(pProactor,handle)
	//	{}

	//	virtual void shutdown(bool /*bSend*/, bool /*bRecv*/)
	//	{
	//		m_handle.close();
	//	}
	//};

	//class SocketIOHelper : public IOHelper
	//{
	//public:
	//	SocketIOHelper(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle) : IOHelper(pProactor,handle)
	//	{}

	//	void shutdown(bool bSend, bool bRecv)
	//	{
	//		int how = -1;
	//		if (bSend)
	//			how = (bRecv ? SD_BOTH : SD_SEND);
	//		else if (bRecv)
	//			how = SD_RECEIVE;

	//		if (how != -1)
	//			::shutdown((SOCKET)(HANDLE)m_handle,how);
	//	}
	//};

	//IOHelper::IOHelper(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle) :
	//		m_handle(handle),
	//		m_pProactor(pProactor)
	//{
	//	m_read_complete.m_is_reading = true;
	//	m_read_complete.m_op = 0;
	//	m_read_complete.m_this_ptr = this;
	//	m_write_complete.m_is_reading = false;
	//	m_write_complete.m_op = 0;
	//	m_write_complete.m_this_ptr = this;
	//}

	//IOHelper::~IOHelper()
	//{
	//}

	//void IOHelper::close()
	//{
	//	m_handle.close();
	//}

	//int IOHelper::bind()
	//{
	//	if (!OOBase::Win32::BindIoCompletionCallback(m_handle,&completion_fn,0))
	//		return GetLastError();

	//	return 0;
	//}

	//void IOHelper::recv(AsyncOp* recv_op)
	//{
	//	assert(!m_read_complete.m_op);

	//	// Reset the completion info
	//	memset(&m_read_complete.m_ov,0,sizeof(OVERLAPPED));
	//	m_read_complete.m_err = 0;
	//	m_read_complete.m_op = recv_op;
	//	m_read_complete.m_last = false;
	//			
	//	do_read();
	//}

	//void IOHelper::send(AsyncOp* send_op)
	//{
	//	assert(!m_write_complete.m_op);

	//	// Reset the completion info
	//	memset(&m_write_complete.m_ov,0,sizeof(OVERLAPPED));
	//	m_write_complete.m_err = 0;
	//	m_write_complete.m_op = send_op;
	//			
	//	do_write();
	//}

	//VOID IOHelper::completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	//{
	//	Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);

	//	if (pInfo->m_is_reading)
	//	{
	//		// Update wr_ptr
	//		pInfo->m_op->buffer->wr_ptr(dwNumberOfBytesTransfered);

	//		if (pInfo->m_op->len)
	//			pInfo->m_op->len -= dwNumberOfBytesTransfered;

	//		pInfo->m_last = (dwNumberOfBytesTransfered == 0);

	//		if (dwErrorCode == 0 && pInfo->m_op->len > 0 && !pInfo->m_last)
	//		{
	//			// More to read
	//			pInfo->m_this_ptr->do_read();

	//			pInfo->m_this_ptr->m_pProactor->release();
	//			return;
	//		}
	//	}
	//	else
	//	{
	//		// Update rd_ptr
	//		pInfo->m_op->buffer->rd_ptr(dwNumberOfBytesTransfered);

	//		if (dwErrorCode == 0 && pInfo->m_op->buffer->length() > 0)
	//		{
	//			// More to send
	//			pInfo->m_this_ptr->do_write();

	//			pInfo->m_this_ptr->m_pProactor->release();
	//			return;
	//		}
	//	}

	//	// Stash error code
	//	pInfo->m_err = dwErrorCode;

	//	// Now forward the completion into the worker pool again as a 'long function'
	//	if (!QueueUserWorkItem(completion_fn2,pInfo,WT_EXECUTELONGFUNCTION))
	//		OOBase_CallCriticalFailure(GetLastError());
	//}

	//DWORD IOHelper::completion_fn2(LPVOID lpThreadParameter)
	//{
	//	Completion* pInfo = static_cast<Completion*>(lpThreadParameter);

	//	OOSvrBase::Win32::ProactorImpl* pProactor = pInfo->m_this_ptr->m_pProactor;

	//	if (pInfo->m_is_reading)
	//		pInfo->m_this_ptr->handle_read(pInfo->m_err);
	//	else
	//		pInfo->m_this_ptr->handle_write(pInfo->m_err);	

	//	pProactor->release();
	//	return 0;
	//}

	//void IOHelper::do_read()
	//{
	//	DWORD dwToRead = static_cast<DWORD>(m_read_complete.m_op->len);
	//	if (dwToRead == 0)
	//		dwToRead = static_cast<DWORD>(m_read_complete.m_op->buffer->space());

	//	m_pProactor->addref();

	//	DWORD dwRead = 0;
	//	if (!ReadFile(m_handle,m_read_complete.m_op->buffer->wr_ptr(),dwToRead,&dwRead,&m_read_complete.m_ov))
	//	{
	//		DWORD dwErr = GetLastError();
	//		if (dwErr != ERROR_IO_PENDING)
	//			completion_fn(dwErr,dwRead,&m_read_complete.m_ov);
	//	}
	//}

	//void IOHelper::handle_read(DWORD dwErrorCode)
	//{
	//	// Stash op... another op may be about to occur...
	//	AsyncOp* op = m_read_complete.m_op;
	//	m_read_complete.m_op = 0;

	//	// Call handlers
	//	m_handler->on_recv(op,dwErrorCode,m_read_complete.m_last);
	//}

	//void IOHelper::do_write()
	//{
	//	m_pProactor->addref();

	//	DWORD dwWritten = 0;
	//	if (!WriteFile(m_handle,m_write_complete.m_op->buffer->rd_ptr(),static_cast<DWORD>(m_write_complete.m_op->buffer->length()),&dwWritten,&m_write_complete.m_ov))
	//	{
	//		DWORD dwErr = GetLastError();
	//		if (dwErr != ERROR_IO_PENDING)
	//			completion_fn(dwErr,dwWritten,&m_write_complete.m_ov);
	//	}
	//}

	//void IOHelper::handle_write(DWORD dwErrorCode)
	//{
	//	// Stash op... another op may be about to occur...
	//	AsyncOp* op = m_write_complete.m_op;
	//	m_write_complete.m_op = 0;

	//	// Call handlers
	//	m_handler->on_sent(op,dwErrorCode);
	//}

	//class AsyncSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>
	//{
	//public:
	//	AsyncSocket(IOHelper* helper) :
	//			OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncSocket>(helper)
	//	{
	//	}

	//	static AsyncSocket* Create(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, int& dwErr)
	//	{
	//		IOHelper* helper;
	//		OOBASE_NEW_T(SocketIOHelper,helper,SocketIOHelper(pProactor,(HANDLE)sock));
	//		if (!helper)
	//		{
	//			dwErr = ERROR_OUTOFMEMORY;
	//			return 0;
	//		}

	//		dwErr = helper->bind();
	//		if (dwErr)
	//		{
	//			OOBASE_DELETE(IOHelper,helper);
	//			return 0;
	//		}

	//		AsyncSocket* pSock;
	//		OOBASE_NEW_T(AsyncSocket,pSock,AsyncSocket(helper));
	//		if (!pSock)
	//		{
	//			dwErr = ERROR_OUTOFMEMORY;
	//			OOBASE_DELETE(IOHelper,helper);
	//			return 0;
	//		}

	//		return pSock;
	//	}
	//};

	//class SocketAcceptor : public OOBase::Socket
	//{
	//public:
	//	SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>* handler, int address_family);
	//	virtual ~SocketAcceptor();

	//	int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
	//	{
	//		return ERROR_INVALID_FUNCTION;
	//	}

	//	size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
	//	{
	//		*perr = ERROR_INVALID_FUNCTION;
	//		return 0;
	//	}

	//	void shutdown(bool bSend, bool bRecv);

	//	int accept();

	//private:
	//	static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);
	//	static DWORD CALLBACK completion_fn2(LPVOID lpThreadParameter);

	//	struct Completion
	//	{
	//		OVERLAPPED                 m_ov;
	//		int                        m_err;
	//		SOCKET                     m_socket;
	//		SocketAcceptor*            m_this_ptr;
	//		char                       m_addresses[(sizeof(sockaddr_in6) + 16)*2];
	//	};

	//	OOSvrBase::Win32::ProactorImpl* const         m_pProactor;
	//	OOBase::Condition::Mutex                      m_lock;
	//	OOBase::Condition                             m_condition;
	//	bool                                          m_closed;
	//	Completion                                    m_completion;
	//	SOCKET                                        m_socket;
	//	OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>*  m_handler;
	//	size_t                                        m_async_count;
	//	int                                           m_address_family;
	//			
	//	void do_accept(int& dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
	//	int next_accept();
	//};

	//SocketAcceptor::SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET sock, OOSvrBase::Acceptor<OOSvrBase::AsyncSocket>* handler, int address_family) :
	//		m_pProactor(pProactor),
	//		m_closed(false),
	//		m_socket(sock),
	//		m_handler(handler),
	//		m_async_count(0),
	//		m_address_family(address_family),
	//		m_lpfnAcceptEx(0)
	//{
	//	assert(address_family == AF_INET || address_family == AF_INET6);
	//	assert(m_socket != INVALID_SOCKET);

	//	m_completion.m_this_ptr = this;
	//}

	//SocketAcceptor::~SocketAcceptor()
	//{
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	//	m_closed = true;

	//	closesocket(m_socket);

	//	while (m_async_count)
	//	{
	//		m_condition.wait(m_lock);
	//	}
	//}

	//int SocketAcceptor::accept()
	//{
	//		if (!OOBase::Win32::BindIoCompletionCallback((HANDLE)m_socket,&completion_fn,0))
	//		{
	//			int err = GetLastError();
	//			closesocket(m_socket);
	//			return err;
	//		}
	
	//	// Just accept the next one...
	//	return next_accept();
	//}

	//int SocketAcceptor::next_accept()
	//{
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	//	if (m_closed)
	//		return WSAESHUTDOWN;

	//	memset(&m_completion.m_ov,0,sizeof(OVERLAPPED));
	//	m_completion.m_err = 0;

	//	int err = 0;
	//	m_completion.m_socket = create_socket(m_address_family,SOCK_STREAM,IPPROTO_TCP,&err);
	//	if (!err)
	//	{
	//		m_pProactor->addref();
	//		++m_async_count;

	//		DWORD dwBytes = 0;
	//		if (!(*m_lpfnAcceptEx)(m_socket,m_completion.m_socket,m_completion.m_addresses,0,sizeof(m_completion.m_addresses)/2,sizeof(m_completion.m_addresses)/2,&dwBytes,&m_completion.m_ov))
	//		{
	//			err = WSAGetLastError();
	//			if (err == ERROR_IO_PENDING)
	//			{
	//				// Async - finish later...
	//				return 0;
	//			}
	//		}

	//		--m_async_count;
	//		m_pProactor->release();
	//	}

	//	do_accept(err,guard);
	//	return err;
	//}

	//VOID SocketAcceptor::completion_fn(DWORD dwErrorCode, DWORD, LPOVERLAPPED lpOverlapped)
	//{
	//	Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
	//	
	//	// Stash error code
	//	pInfo->m_err = dwErrorCode;

	//	// Now forward the completion into the worker pool again as a 'long function'
	//	if (!QueueUserWorkItem(completion_fn2,pInfo,WT_EXECUTELONGFUNCTION))
	//		OOBase_CallCriticalFailure(GetLastError());
	//}

	//DWORD SocketAcceptor::completion_fn2(LPVOID lpThreadParameter)
	//{
	//	Completion* pInfo = static_cast<Completion*>(lpThreadParameter);

	//	OOSvrBase::Win32::ProactorImpl* pProactor = pInfo->m_this_ptr->m_pProactor;
	//	
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(pInfo->m_this_ptr->m_lock);

	//	--pInfo->m_this_ptr->m_async_count;

	//	pInfo->m_this_ptr->do_accept(pInfo->m_err,guard);

	//	pProactor->release();
	//	return 0;
	//}

	//void SocketAcceptor::do_accept(int& dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
	//{
	//	if (m_closed)
	//	{
	//		if (m_completion.m_socket != INVALID_SOCKET)
	//			closesocket(m_completion.m_socket);

	//		guard.release();
	//		m_condition.signal();
	//		return;
	//	}

	//	OOSvrBase::AsyncSocketPtr ptrSocket;
	//	OOBase::local_string strAddress;

	//	if (dwErr == 0)
	//	{
	//		ptrSocket = AsyncSocket::Create(m_pProactor,m_completion.m_socket,dwErr);
	//		if (!ptrSocket)
	//		{
	//			if (m_completion.m_socket != INVALID_SOCKET)
	//				closesocket(m_completion.m_socket);
	//		}
	//		else
	//		{
	//			sockaddr* pLocal = 0;
	//			sockaddr* pRemote = 0;
	//			int len1,len2;

	//			(*m_lpfnGetAcceptExSockAddrs)(m_completion.m_addresses,0,sizeof(m_completion.m_addresses)/2,sizeof(m_completion.m_addresses)/2,&pLocal,&len1,&pRemote,&len2);
	//			
	//			if (pRemote->sa_family == AF_INET)
	//				strAddress = inet_ntoa(reinterpret_cast<sockaddr_in*>(pRemote)->sin_addr);
	//		}
	//	}

	//	guard.release();

	//	// Call the acceptor
	//	if (m_handler->on_accept(ptrSocket,strAddress.c_str(),dwErr))
	//	{
	//		dwErr = 0;
	//		next_accept();
	//	}
	//}

	//void SocketAcceptor::shutdown(bool /*bSend*/, bool /*bRecv*/)
	//{
	//}

	//class AsyncLocalSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>
	//{
	//public:
	//	AsyncLocalSocket(HANDLE hPipe, PipeIOHelper* helper) :
	//			OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>(helper),
	//			m_hPipe(hPipe)
	//	{
	//	}

	//	static AsyncLocalSocket* Create(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE hPipe, DWORD& dwErr)
	//	{
	//		PipeIOHelper* helper;
	//		OOBASE_NEW_T(PipeIOHelper,helper,PipeIOHelper(pProactor,hPipe));
	//		if (!helper)
	//		{
	//			dwErr = ERROR_OUTOFMEMORY;
	//			return 0;
	//		}

	//		dwErr = helper->bind();
	//		if (dwErr)
	//		{
	//			OOBASE_DELETE(PipeIOHelper,helper);
	//			return 0;
	//		}

	//		AsyncLocalSocket* sock;
	//		OOBASE_NEW_T(AsyncLocalSocket,sock,AsyncLocalSocket(hPipe,helper));
	//		if (!sock)
	//		{
	//			dwErr = ERROR_OUTOFMEMORY;
	//			OOBASE_DELETE(PipeIOHelper,helper);
	//			return 0;
	//		}

	//		return sock;
	//	}

	//private:
	//	HANDLE m_hPipe;

	//	int get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid);
	//};

	//int AsyncLocalSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
	//{
	//	if (!ImpersonateNamedPipeClient(m_hPipe))
	//		return GetLastError();

	//	OOBase::Win32::SmartHandle ptruid;
	//	BOOL bRes = OpenThreadToken(GetCurrentThread(),TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE,FALSE,&ptruid);
	//	DWORD err = 0;
	//	if (!bRes)
	//		err = GetLastError();

	//	if (!RevertToSelf())
	//		OOBase_CallCriticalFailure(GetLastError());
	//	
	//	if (!bRes)
	//		return err;

	//	uid = ptruid.detach();
	//	return 0;
	//}

	//class PipeAcceptor : public OOBase::Socket
	//{
	//public:
	//	PipeAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, const OOBase::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler);
	//	virtual ~PipeAcceptor();

	//	int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
	//	{
	//		return ERROR_INVALID_FUNCTION;
	//	}

	//	size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
	//	{
	//		*perr = ERROR_INVALID_FUNCTION;
	//		return 0;
	//	}

	//	void shutdown(bool bSend, bool bRecv);

	//	int accept();

	//private:
	//	static VOID CALLBACK completion_fn(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

	//	struct Completion
	//	{
	//		OVERLAPPED                 m_ov;
	//		OOBase::Win32::SmartHandle m_hPipe;
	//		HANDLE                     m_hWait;
	//		PipeAcceptor*              m_this_ptr;
	//	};

	//	OOSvrBase::Win32::ProactorImpl* const              m_pProactor;
	//	OOBase::Condition::Mutex                           m_lock;
	//	OOBase::Condition                                  m_condition;
	//	bool                                               m_closed;
	//	Completion                                         m_completion;
	//	OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>*  m_handler;
	//	const OOBase::string                               m_pipe_name;
	//	LPSECURITY_ATTRIBUTES                              m_psa;
	//	size_t                                             m_async_count;
	//	
	//	int next_accept(bool bFirstTime);
	//	void do_accept(DWORD& dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
	//};

	//PipeAcceptor::PipeAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, const OOBase::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler) :
	//		m_pProactor(pProactor),
	//		m_closed(false),
	//		m_handler(handler),
	//		m_pipe_name(pipe_name),
	//		m_psa(psa),
	//		m_async_count(0)
	//{
	//	m_completion.m_this_ptr = this;
	//}

	//PipeAcceptor::~PipeAcceptor()
	//{
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	//	m_closed = true;

	//	m_completion.m_hPipe.close();

	//	while (m_async_count)
	//	{
	//		m_condition.wait(m_lock);
	//	}
	//}

	//int PipeAcceptor::accept()
	//{
	//	return next_accept(true);
	//}

	//int PipeAcceptor::next_accept(bool bFirstTime)
	//{
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

	//	assert(!m_completion.m_hPipe.is_valid());

	//	if (m_closed)
	//		return ERROR_PIPE_NOT_CONNECTED;

	//	bool bDoAccept = !bFirstTime;

	//	DWORD dwOpenFlags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	//	if (bFirstTime)
	//		dwOpenFlags |= FILE_FLAG_FIRST_PIPE_INSTANCE;

	//	DWORD dwErr = 0;
	//	HANDLE hPipe = CreateNamedPipeA(m_pipe_name.c_str(),dwOpenFlags,
	//							   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
	//							   PIPE_UNLIMITED_INSTANCES,
	//							   0,0,0,m_psa);

	//	if (hPipe == INVALID_HANDLE_VALUE)
	//		dwErr = GetLastError();
	//	else
	//	{
	//		memset(&m_completion.m_ov,0,sizeof(OVERLAPPED));
	//		m_completion.m_ov.hEvent = ::CreateEvent(NULL,TRUE,FALSE,NULL);
	//		if (!m_completion.m_ov.hEvent)
	//			dwErr = GetLastError();
	//		else
	//			m_completion.m_hPipe = hPipe;
	//	}

	//	if (dwErr == 0)
	//	{
	//		++m_async_count;
	//		m_pProactor->addref();

	//		bDoAccept = true;

	//		if (!ConnectNamedPipe(m_completion.m_hPipe,&m_completion.m_ov))
	//		{
	//			dwErr = GetLastError();
	//			if (dwErr == ERROR_IO_PENDING)
	//			{
	//				if (RegisterWaitForSingleObject(&m_completion.m_hWait,m_completion.m_ov.hEvent,&completion_fn,&m_completion,INFINITE,WT_EXECUTELONGFUNCTION | WT_EXECUTEONLYONCE))
	//				{
	//					// Will complete later...
	//					return 0;
	//				}
	//				else
	//					dwErr = GetLastError();			
	//			}
	//			
	//			if (dwErr == ERROR_PIPE_CONNECTED)
	//				dwErr = 0;
	//		}

	//		if (dwErr != 0)
	//			m_completion.m_hPipe.close();

	//		CloseHandle(m_completion.m_ov.hEvent);		

	//		m_pProactor->release();
	//		--m_async_count;
	//	}

	//	if (bDoAccept)
	//		do_accept(dwErr,guard);

	//	return dwErr;
	//}

	//VOID PipeAcceptor::completion_fn(PVOID lpParameter, BOOLEAN /*TimerOrWaitFired*/)
	//{
	//	Completion* pInfo = static_cast<Completion*>(lpParameter);

	//	OOSvrBase::Win32::ProactorImpl* pProactor = pInfo->m_this_ptr->m_pProactor;

	//	DWORD dwErrorCode = 0;
	//	DWORD dwBytes = 0;
	//	if (!GetOverlappedResult(pInfo->m_hPipe,&pInfo->m_ov,&dwBytes,TRUE))
	//		dwErrorCode = GetLastError();

	//	// Close the handles first...
	//	UnregisterWait(pInfo->m_hWait);
	//	CloseHandle(pInfo->m_ov.hEvent);
	//	
	//	OOBase::Guard<OOBase::Condition::Mutex> guard(pInfo->m_this_ptr->m_lock);

	//	--pInfo->m_this_ptr->m_async_count;

	//	pInfo->m_this_ptr->do_accept(dwErrorCode,guard);

	//	pProactor->release();
	//}

	//void PipeAcceptor::do_accept(DWORD& dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
	//{
	//	if (m_closed)
	//	{
	//		guard.release();
	//		m_condition.signal();
	//		return;
	//	}

	//	OOSvrBase::AsyncLocalSocketPtr ptrSocket;
	//	if (dwErr == 0)
	//	{
	//		ptrSocket = AsyncLocalSocket::Create(m_pProactor,m_completion.m_hPipe,dwErr);
	//		if (ptrSocket)
	//			m_completion.m_hPipe.detach();
	//		else
	//			m_completion.m_hPipe.close();
	//	}

	//	guard.release();

	//	// Call the acceptor
	//	if (m_handler->on_accept(ptrSocket,m_pipe_name.c_str(),dwErr))
	//	{
	//		dwErr = 0;
	//		next_accept(false);
	//	}
	//}

	//void PipeAcceptor::shutdown(bool /*bSend*/, bool /*bRecv*/)
	//{
	//}
}

OOSvrBase::Win32::ProactorImpl::ProactorImpl() :
		OOSvrBase::Proactor(false),
		m_refcount(0)
{
	OOBase::Win32::WSAStartup();
}

OOSvrBase::Win32::ProactorImpl::~ProactorImpl()
{
	while (m_refcount > 0)
		OOBase::Thread::yield();
}

#endif // _WIN32
