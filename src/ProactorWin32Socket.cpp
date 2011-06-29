///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#include "ProactorWin32.h"

#if defined(_WIN32)

#include "../include/OOBase/Allocator.h"
#include "../include/OOBase/Condition.h"
#include "Win32Socket.h"

namespace
{
	SOCKET create_socket(int family, int socktype, int protocol, int& err)
	{
		void* TODO; // Move me elsewhere
		
		err = 0;
		SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET)
			err = WSAGetLastError();

		return sock;
	}

	class AsyncSocket : public OOSvrBase::AsyncSocket
	{
	public:
		AsyncSocket(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET hSocket);
		virtual ~AsyncSocket();

		int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		void shutdown(bool bSend, bool bRecv);
	
	private:
		OOSvrBase::Win32::ProactorImpl* m_pProactor;
		SOCKET                          m_hSocket;
					
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
	};
}

AsyncSocket::AsyncSocket(OOSvrBase::Win32::ProactorImpl* pProactor, SOCKET hSocket) :
		m_pProactor(pProactor),
		m_hSocket(hSocket)
{
}

AsyncSocket::~AsyncSocket()
{
	closesocket(m_hSocket);
}

int AsyncSocket::recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
{
	// Must have a callback function
	assert(callback);
	
	size_t total = bytes;
	if (total == 0)
	{
		total = buffer->space();
	
		// I imagine you might want to increase space()?
		assert(total > 0);
	}
	else
	{
		// Make sure we have room
		int dwErr = buffer->space(total);
		if (dwErr != 0)
			return dwErr;
	}
	
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_recv);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(m_pProactor);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(buffer->addref());
	
	void* TODO; // Add timeout support!
	
	WSABUF wsa_buf;
	wsa_buf.buf = buffer->wr_ptr();
	wsa_buf.len = static_cast<u_long>(total);
	
	DWORD dwRead = 0;
	DWORD dwFlags = 0;
	if (WSARecv(m_hSocket,&wsa_buf,1,&dwRead,&dwFlags,pOv,NULL) == SOCKET_ERROR)
	{
		dwErr = GetLastError();
		if (dwErr == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	assert(dwErr || total == dwRead);
	
	on_recv((HANDLE)m_hSocket,dwRead,dwErr,pOv);
	
	return 0;
}

void AsyncSocket::on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
		
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]);
	
	pProactor->delete_overlapped(pOv);
	
	buffer->wr_ptr(dwBytes);
	
	// Call callback
	try
	{
		(*callback)(param,buffer,dwErr);
	}
	catch (...)
	{
		assert((0,"Unhandled exception"));
	}

	buffer->release();
}

int AsyncSocket::send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
{
	size_t total = buffer->length();
	
	// I imagine you might want to increase length()?
	assert(total > 0);
		
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(m_pProactor);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(buffer->addref());
	
	WSABUF wsa_buf;
	wsa_buf.buf = const_cast<char*>(buffer->rd_ptr());
	wsa_buf.len = static_cast<u_long>(total);
	
	DWORD dwSent = 0;
	if (WSASend(m_hSocket,&wsa_buf,1,&dwSent,0,pOv,NULL) == SOCKET_ERROR)
	{
		dwErr = GetLastError();
		if (dwErr == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
		
	assert(dwErr || total == dwSent);
	
	on_send((HANDLE)m_hSocket,dwSent,dwErr,pOv);
	
	return 0;
}

void AsyncSocket::on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]);
	
	pProactor->delete_overlapped(pOv);
	
	buffer->rd_ptr(dwBytes);
	
	// Call callback
	if (callback)
	{
		try
		{
			(*callback)(param,buffer,dwErr);
		}
		catch (...)
		{
			assert((0,"Unhandled exception"));
		}
	}

	buffer->release();
}

int AsyncSocket::send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
{
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (dwErr != 0)
		return dwErr;
	
	OOBase::Buffer** buffers_copy = new (std::nothrow) OOBase::Buffer*[count];
	if (!buffers_copy)
	{
		m_pProactor->delete_overlapped(pOv);
		return ERROR_OUTOFMEMORY;
	}
	
	OOBase::SmartPtr<WSABUF> wsa_bufs = new (std::nothrow) WSABUF[count];
	
	for (size_t i=0;i<count;++i)
	{
		wsa_bufs[i].len = static_cast<u_long>(buffers[i]->length());

		// I imagine you might want to increase length()?
		assert(wsa_bufs[i].len > 0);
		
		buffers_copy[i] = buffers[i]->addref();
		wsa_bufs[i].buf = const_cast<char*>(buffers[i]->rd_ptr());
	}
			
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(m_pProactor);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(buffers_copy);
	pOv->m_extras[4] = static_cast<ULONG_PTR>(count);
		
	DWORD dwSent = 0;
	if (WSASend(m_hSocket,wsa_bufs,count,&dwSent,0,pOv,NULL) == SOCKET_ERROR)
	{
		dwErr = GetLastError();
		if (dwErr == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	assert(dwErr || dwSent);
	
	on_send_v((HANDLE)m_hSocket,dwSent,dwErr,pOv);
		
	return 0;
}

void AsyncSocket::on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffers[], size_t count, int err);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer** buffers = reinterpret_cast<OOBase::Buffer**>(pOv->m_extras[3]);
	size_t count = static_cast<size_t>(pOv->m_extras[4]);
	
	pProactor->delete_overlapped(pOv);
	
	for (size_t i=0;dwBytes>0 && i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (len > dwBytes)
			len = dwBytes;
		
		buffers[i]->rd_ptr(len);
		dwBytes -= len;
	}
	
	// Call callback
	if (callback)
	{
		try
		{
			(*callback)(param,buffers,count,dwErr);
		}
		catch (...)
		{
			assert((0,"Unhandled exception"));
		}
	}
		
	for (size_t i=0;i<count;++i)
		buffers[i]->release();
	
	delete [] buffers;
}

void AsyncSocket::shutdown(bool bSend, bool bRecv)
{
	int how = -1;
	if (bSend && bRecv)
		how = SD_BOTH;
	else if (bSend)
		how = SD_SEND;
	else if (bRecv)
		how = SD_RECEIVE;
	
	if (how != -1)
		::shutdown(m_hSocket,how);
}

namespace
{
	class SocketAcceptor : public OOSvrBase::Acceptor
	{
	public:
		SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err));
		virtual ~SocketAcceptor();

		int listen(size_t backlog);
		int stop();

		int bind(const struct sockaddr* addr, size_t addr_len);
	
	private:
		OOSvrBase::Win32::ProactorImpl*  m_pProactor;
		OOBase::Condition::Mutex         m_lock;
		OOBase::Condition                m_condition;
		struct sockaddr*                 m_addr;
		size_t                           m_addr_len;
		SOCKET                           m_socket;
		size_t                           m_backlog;
		size_t                           m_pending;
		OOBase::Win32::SmartHandle       m_hEvent;
		HANDLE                           m_hWait;
		void*                            m_param;
		void (*m_callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err);
		
		static void on_completion(HANDLE hSocket, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void CALLBACK accept_ready(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

		int init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
		void on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf);
		int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
	};
}

SocketAcceptor::SocketAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err)) :
		m_pProactor(pProactor),
		m_addr(NULL),
		m_addr_len(0),
		m_socket(INVALID_SOCKET),
		m_backlog(0),
		m_pending(0),
		m_hWait(NULL),
		m_param(param),
		m_callback(callback)
{ }

SocketAcceptor::~SocketAcceptor()
{
	stop();
	
	if (m_hWait != NULL)
		UnregisterWaitEx(m_hWait,INVALID_HANDLE_VALUE);
}

int SocketAcceptor::bind(const struct sockaddr* addr, size_t addr_len)
{
	m_addr = (struct sockaddr*)OOBase::Allocate(addr_len,0,__FILE__,__LINE__);
	if (!m_addr)
		return ERROR_OUTOFMEMORY;
	
	memcpy(m_addr,addr,addr_len);
	m_addr_len = addr_len;
	
	// Create an event to wait on
	m_hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	if (!m_hEvent.is_valid())
	{
		int err = GetLastError();
		OOBase::Free(m_addr,0);
		m_addr = NULL;
		m_addr_len = 0;
		return err;
	}
	
	// Set up a registered wait
	if (!RegisterWaitForSingleObject(&m_hWait,m_hEvent,&accept_ready,this,INFINITE,WT_EXECUTEDEFAULT))
	{
		int err = GetLastError();
		OOBase::Free(m_addr,0);
		m_addr = NULL;
		m_addr_len = 0;
		return err;
	}
	
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

int SocketAcceptor::stop()
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		closesocket(m_socket);
	}
	
	// Wait for all pending operations to complete
	while (m_pending != 0 && m_backlog == 0)
	{
		m_condition.wait(m_lock);
	}
	
	if (m_backlog == 0)
		m_socket = INVALID_SOCKET;

	return 0;
}

int SocketAcceptor::init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	// Create a new socket
	int err = 0;
	if ((m_socket = create_socket(m_addr->sa_family,SOCK_STREAM,0,err)) == INVALID_SOCKET)
		return err;
		
	// Bind to the address
	if (::bind(m_socket,m_addr,m_addr_len) == SOCKET_ERROR)
		err = WSAGetLastError();
	else
	{
		// Bind to the proactors completion port
		err = m_pProactor->bind((HANDLE)m_socket);
		if (err == 0)
		{
			// Now subscribe for notification
			if (WSAEventSelect(m_socket,m_hEvent,FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
				err = WSAGetLastError();
			else
			{
				// Start the socket listening...
				if (::listen(m_socket,m_backlog) == SOCKET_ERROR)
					err = WSAGetLastError();
				else
				{
					// And now queue up some accepts
					err = do_accept(guard);
				}
			}
		}
	}
	
	if (err != 0)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	
	return err;
}

int SocketAcceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	int dwErr = 0;
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
		
	SOCKET sockNew = INVALID_SOCKET;
	void* buf = NULL;
	
	do
	{
		if ((sockNew = create_socket(m_addr->sa_family,SOCK_STREAM,0,dwErr)) == INVALID_SOCKET)
			break;

		dwErr = m_pProactor->bind((HANDLE)sockNew);
		if (dwErr != 0)
			break;
							
		if (!pOv)
		{
			dwErr = m_pProactor->new_overlapped(pOv,&on_completion);
			if (dwErr != 0)
				break;
			
			buf = OOBase::Allocate((m_addr_len+16)*2,0,__FILE__,__LINE__);
			if (!buf)
			{
				dwErr = ERROR_OUTOFMEMORY;
				break;
			}
			
			// Set this pointer
			pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(this);
			pOv->m_extras[1] = static_cast<ULONG_PTR>(sockNew);
			pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buf);
		}
		
		DWORD dwRead = 0;
		if (!OOBase::Win32::WSAAcceptEx(m_socket,sockNew,buf,0,m_addr_len+16,m_addr_len+16,&dwRead,pOv))
		{
			dwErr = WSAGetLastError();
			if (dwErr != ERROR_IO_PENDING)
				break;
			
			// Will complete later...
			++m_pending;
			pOv = NULL;
			buf = NULL;
			dwErr = 0;
						
			continue;
		}
		
		guard.release();
		
		// Call the callback
		on_accept(sockNew,false,0,buf);		
		sockNew = INVALID_SOCKET;
		
		guard.acquire();
	}
	while (m_pending < m_backlog);
	
	if (sockNew != INVALID_SOCKET)
		closesocket(sockNew);
	
	if (buf)
		OOBase::Free(buf,0);
	
	if (pOv)
		m_pProactor->delete_overlapped(pOv);
	
	return dwErr;
}

void SocketAcceptor::accept_ready(PVOID lpParameter, BOOLEAN)
{
	SocketAcceptor* pThis = static_cast<SocketAcceptor*>(lpParameter);
	
	WSANETWORKEVENTS events = {0};
	if (WSAEnumNetworkEvents(pThis->m_socket,pThis->m_hEvent,&events) == 0)
	{
		if ((events.lNetworkEvents & FD_ACCEPT) && events.iErrorCode[FD_ACCEPT] == 0)
		{
			OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
			
			pThis->do_accept(guard);
		}
	}
}

void SocketAcceptor::on_completion(HANDLE hSocket, DWORD /*dwBytes*/, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{	
	SocketAcceptor* pThis = reinterpret_cast<SocketAcceptor*>(pOv->m_extras[0]);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = pThis->m_pProactor;
	
	// Perform the callback
	pThis->on_accept((SOCKET)hSocket,true,dwErr,reinterpret_cast<void*>(pOv->m_extras[2]));
	
	// Return the overlapped to the proactor
	pProactor->delete_overlapped(pOv);
}

void SocketAcceptor::on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock,false);
	
	bool bSignal = false;
	bool bAgain = false;
	if (bRemove)
	{
		guard.acquire();

		--m_pending;
		
		bAgain = (m_pending < m_backlog);
		bSignal = (m_pending == 0 && m_backlog == 0);
		
		guard.release();
	}
	
	OOSvrBase::AsyncSocket* pSocket = NULL;
	struct sockaddr* remote_addr = NULL;
	int remote_addr_len = 0;
		
	if (dwErr == 0)
	{
		// Get addresses
		struct sockaddr* local_addr = NULL;
		int local_addr_len = 0;
		OOBase::Win32::WSAGetAcceptExSockAddrs(addr_buf,0,m_addr_len+16,m_addr_len+16,&local_addr,&local_addr_len,&remote_addr,&remote_addr_len);
			
		// Wrap the handle
		pSocket = new (std::nothrow) AsyncSocket(m_pProactor,hSocket);
		if (!pSocket)
			dwErr = ERROR_OUTOFMEMORY;
	}
	
	if (bRemove)
		OOBase::Free(addr_buf,0);
	
	if (dwErr != 0)
	{
		remote_addr = NULL;
		remote_addr_len = 0;
		closesocket(hSocket);
	}
	
	try
	{
		(*m_callback)(m_param,pSocket,remote_addr,remote_addr_len,dwErr);
	}
	catch (...)
	{
		assert((0,"Unhandled exception"));
	}
	
	if (bAgain)
	{
		guard.acquire();
		
		// Try to accept some more connections
		dwErr = do_accept(guard);
		if (dwErr != 0)
		{
			guard.release();
			
			try
			{
				(*m_callback)(m_param,NULL,NULL,0,dwErr);
			}
			catch (...)
			{
				assert((0,"Unhandled exception"));
			}
			
			guard.acquire();
		}
		
		bSignal = (m_pending == 0 && m_backlog == 0);
	}
	
	if (bSignal)
		m_condition.broadcast();
}

OOSvrBase::Acceptor* OOSvrBase::Win32::ProactorImpl::accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err)
{
	OOBase::Win32::WSAStartup();
	
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		assert((0,"Invalid parameters"));
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor(this,param,callback);
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
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

OOSvrBase::AsyncSocket* OOSvrBase::Win32::ProactorImpl::connect_socket(const struct sockaddr* addr, size_t addr_len, int& err, const OOBase::timeval_t* timeout)
{
	SOCKET sock = INVALID_SOCKET;
	if ((sock = create_socket(addr->sa_family,SOCK_STREAM,0,err)) == INVALID_SOCKET)
		return NULL;
	
	if (::connect(sock,addr,addr_len) == SOCKET_ERROR)
	{
		err = WSAGetLastError();
		closesocket(sock);
		return NULL;
	}
	
	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this,sock);
	if (!pSocket)
	{
		closesocket(sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pSocket;
}

OOSvrBase::AsyncSocket* OOSvrBase::Win32::ProactorImpl::attach_socket(OOBase::socket_t sock, int& err)
{
	// The socket must have been opened as WSA_FLAG_OVERLAPPED!!!
	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this,sock);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;
		
	return pSocket;
}

#endif // _WIN32
