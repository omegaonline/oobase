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

#if !defined(MSG_WAITALL)
#define MSG_WAITALL 0x8
#endif

#include "../include/OOBase/Condition.h"
#include "../include/OOBase/SmartPtr.h"

#include "Win32Socket.h"

namespace
{
	class AsyncSocket : public OOSvrBase::AsyncSocket
	{
	public:
		AsyncSocket(OOSvrBase::detail::ProactorWin32* pProactor, SOCKET hSocket);
		
		int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, send_callback_t callback, OOBase::Buffer* buffer);
		int send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count);
	
	private:
		virtual ~AsyncSocket();

		OOSvrBase::detail::ProactorWin32* m_pProactor;
		SOCKET                            m_hSocket;
					
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
	};
}

AsyncSocket::AsyncSocket(OOSvrBase::detail::ProactorWin32* pProactor, SOCKET hSocket) :
		m_pProactor(pProactor),
		m_hSocket(hSocket)
{ }

AsyncSocket::~AsyncSocket()
{
	closesocket(m_hSocket);
}

int AsyncSocket::recv(void* param, OOSvrBase::AsyncSocket::recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
{
	// Must have a callback function
	assert(callback);

	// Make sure we have room
	int err = buffer->space(bytes);
	if (err != 0)
		return err;
		
	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	err = m_pProactor->new_overlapped(pOv,&on_recv);
	if (err != 0)
		return err;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	buffer->addref();
	
	void* TODO; // Add timeout support!
	
	WSABUF wsa_buf;
	wsa_buf.buf = buffer->wr_ptr();
	wsa_buf.len = static_cast<u_long>(bytes);
	
	DWORD dwRead = 0;
	DWORD dwFlags = (bytes != 0 ? MSG_WAITALL : 0);
	if (WSARecv(m_hSocket,&wsa_buf,1,&dwRead,&dwFlags,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	assert(err || bytes == dwRead);
	
	on_recv((HANDLE)m_hSocket,dwRead,err,pOv);
	
	return 0;
}

void AsyncSocket::on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	recv_callback_t callback = reinterpret_cast<recv_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));

	delete pOv;
		
	// Call callback
	if (callback)
	{
		buffer->wr_ptr(dwBytes);

		(*callback)(param,buffer,dwErr);
	}
}

int AsyncSocket::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	size_t bytes = buffer->length();
	if (bytes == 0)
		return 0;

	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int err = m_pProactor->new_overlapped(pOv,&on_send);
	if (err != 0)
		return err;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	buffer->addref();
	
	WSABUF wsa_buf;
	wsa_buf.buf = const_cast<char*>(buffer->rd_ptr());
	wsa_buf.len = static_cast<u_long>(bytes);
	
	DWORD dwSent = 0;
	if (WSASend(m_hSocket,&wsa_buf,1,&dwSent,0,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
		
	assert(err || bytes == dwSent);
	
	on_send((HANDLE)m_hSocket,dwSent,err,pOv);
	
	return 0;
}

void AsyncSocket::on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_callback_t callback = reinterpret_cast<send_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));

	delete pOv;
		
	// Call callback
	if (callback)
		(*callback)(param,dwErr);
}

int AsyncSocket::send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	if (count == 0)
		return 0;

	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int err = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (err != 0)
		return err;
	
	OOBase::SmartPtr<WSABUF,OOBase::HeapAllocator> wsa_bufs(static_cast<WSABUF*>(OOBase::HeapAllocator::allocate(sizeof(WSABUF) * count)));
	if (!wsa_bufs)
	{
		delete pOv;
		return ERROR_OUTOFMEMORY;
	}

	OOBase::Buffer** buffers_copy = new (std::nothrow) OOBase::Buffer*[count];
	if (!buffers_copy)
	{
		delete pOv;
		return ERROR_OUTOFMEMORY;
	}
	
	size_t total = 0;
	for (size_t i=0;i<count;++i)
	{
		wsa_bufs[i].len = static_cast<u_long>(buffers[i]->length());
		total += wsa_bufs[i].len;

		wsa_bufs[i].buf = const_cast<char*>(buffers[i]->rd_ptr());
		
		buffers_copy[i] = buffers[i];
		buffers_copy[i]->addref();
	}

	if (total == 0)
		return 0;
			
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffers_copy);
	pOv->m_extras[3] = static_cast<ULONG_PTR>(count);
		
	DWORD dwSent = 0;
	if (WSASend(m_hSocket,wsa_bufs,static_cast<DWORD>(count),&dwSent,0,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	assert(err || dwSent);
	
	on_send_v((HANDLE)m_hSocket,dwSent,err,pOv);
		
	return 0;
}

void AsyncSocket::on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	// Get the actual result (because its actually different!)
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult((SOCKET)handle,pOv,&dwBytes,TRUE,&dwFlags))
		dwErr = WSAGetLastError();
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_callback_t callback = reinterpret_cast<send_callback_t>(pOv->m_extras[1]);
	OOBase::Buffer** buffers = reinterpret_cast<OOBase::Buffer**>(pOv->m_extras[2]);
	size_t count = static_cast<size_t>(pOv->m_extras[3]);

	for (size_t i=0;i<count;++i)
		buffers[i]->release();

	delete [] buffers;
	delete pOv;
	
	if (callback)
		(*callback)(param,dwErr);
}

namespace
{
	class SocketAcceptor : public OOSvrBase::Acceptor
	{
	public:
		SocketAcceptor();
		
		int listen(size_t backlog);
		int stop();
		int bind(OOSvrBase::detail::ProactorWin32* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len);
	
	private:
		virtual ~SocketAcceptor();

		class Acceptor
		{
		public:
			Acceptor(OOSvrBase::detail::ProactorWin32* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback);
			
			int listen(size_t backlog);
			int stop(bool destroy);
			int bind(const sockaddr* addr, socklen_t addr_len);
			
		private:
			OOSvrBase::detail::ProactorWin32*             m_pProactor;
			OOBase::Condition::Mutex                      m_lock;
			OOBase::Condition                             m_condition;
			sockaddr*                                     m_addr;
			socklen_t                                     m_addr_len;
			SOCKET                                        m_socket;
			size_t                                        m_backlog;
			size_t                                        m_pending;
			size_t                                        m_refcount;
			OOBase::Win32::SmartHandle                    m_hEvent;
			HANDLE                                        m_hWait;
			void*                                         m_param;
			OOSvrBase::Proactor::accept_remote_callback_t m_callback;
		
			~Acceptor();
			
			static void on_completion(HANDLE hSocket, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
			static void CALLBACK accept_ready(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

			int init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
			bool on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf, OOBase::Guard<OOBase::Condition::Mutex>& guard);
			int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
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
		return WSAEBADF;
	
	return m_pAcceptor->listen(backlog);
}

int SocketAcceptor::stop()
{
	if (!m_pAcceptor)
		return WSAEBADF;
	
	return m_pAcceptor->stop(false);
}

int SocketAcceptor::bind(OOSvrBase::detail::ProactorWin32* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len)
{
	m_pAcceptor = new (std::nothrow) SocketAcceptor::Acceptor(pProactor,param,callback);
	if (!m_pAcceptor)
		return ERROR_OUTOFMEMORY;
	
	int err = m_pAcceptor->bind(addr,addr_len);
	if (err != 0)
	{
		m_pAcceptor->stop(true);
		m_pAcceptor = NULL;
	}
		
	return err;
}

SocketAcceptor::Acceptor::Acceptor(OOSvrBase::detail::ProactorWin32* pProactor, void* param, OOSvrBase::Proactor::accept_remote_callback_t callback) :
		m_pProactor(pProactor),
		m_addr(NULL),
		m_addr_len(0),
		m_socket(INVALID_SOCKET),
		m_backlog(0),
		m_pending(0),
		m_refcount(1),
		m_hWait(NULL),
		m_param(param),
		m_callback(callback)
{ }

SocketAcceptor::Acceptor::~Acceptor()
{ 
	OOBase::HeapFree(m_addr);
}

int SocketAcceptor::Acceptor::bind(const sockaddr* addr, socklen_t addr_len)
{
	m_addr = static_cast<sockaddr*>(OOBase::HeapAllocate(addr_len));
	if (!m_addr)
		return ERROR_OUTOFMEMORY;
	
	memcpy(m_addr,addr,addr_len);
	m_addr_len = addr_len;
	
	// Create an event to wait on
	m_hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	if (!m_hEvent.is_valid())
	{
		int err = GetLastError();
		OOBase::HeapFree(m_addr);
		m_addr = NULL;
		m_addr_len = 0;
		return err;
	}
	
	// Set up a registered wait
	if (!RegisterWaitForSingleObject(&m_hWait,m_hEvent,&accept_ready,this,INFINITE,WT_EXECUTEDEFAULT))
	{
		int err = GetLastError();
		OOBase::HeapFree(m_addr);
		m_addr = NULL;
		m_addr_len = 0;
		return err;
	}
	
	return 0;
}

int SocketAcceptor::Acceptor::listen(size_t backlog)
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

int SocketAcceptor::Acceptor::stop(bool destroy)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (destroy && m_hWait != NULL)
	{
		guard.release();
		
		UnregisterWaitEx(m_hWait,INVALID_HANDLE_VALUE);
	
		guard.acquire();
		
		m_hWait = NULL;
	}
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	
	// Wait for all pending operations to complete
	while (m_pending != 0 && m_backlog == 0)
		m_condition.wait(m_lock);
		
	if (destroy && --m_refcount == 0)
	{
		guard.release();
		delete this;
	}

	return 0;
}

int SocketAcceptor::Acceptor::init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	// Create a new socket
	int err = 0;
	if ((m_socket = OOBase::Win32::create_socket(m_addr->sa_family,SOCK_STREAM,0,err)) == INVALID_SOCKET)
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
				if (::listen(m_socket,static_cast<int>(m_backlog)) == SOCKET_ERROR)
					err = WSAGetLastError();
				else
				{
					// And now queue up some accepts
					err = do_accept(guard);
				}
			}

			if (err != 0)
				m_pProactor->unbind((HANDLE)m_socket);
		}
	}
	
	if (err != 0)
	{
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	
	return err;
}

int SocketAcceptor::Acceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	int err = 0;
	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
		
	SOCKET sockNew = INVALID_SOCKET;
	void* buf = NULL;
	
	while (m_pending < m_backlog)
	{
		if ((sockNew = OOBase::Win32::create_socket(m_addr->sa_family,SOCK_STREAM,0,err)) == INVALID_SOCKET)
			break;

		err = m_pProactor->bind((HANDLE)sockNew);
		if (err != 0)
			break;
							
		if (!pOv)
		{
			err = m_pProactor->new_overlapped(pOv,&on_completion);
			if (err != 0)
			{
				m_pProactor->unbind((HANDLE)sockNew);
				break;
			}
			
			buf = OOBase::HeapAllocate((m_addr_len+16)*2);
			if (!buf)
			{
				m_pProactor->unbind((HANDLE)sockNew);
				err = ERROR_OUTOFMEMORY;
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
			err = WSAGetLastError();
			if (err != ERROR_IO_PENDING)
			{
				m_pProactor->unbind((HANDLE)sockNew);
				break;
			}
			
			// Will complete later...
			++m_pending;
			pOv = NULL;
			buf = NULL;
			err = 0;
						
			continue;
		}
		
		// Call the callback
		if (on_accept(sockNew,false,0,NULL,guard))
		{
			// We have self destructed
			break;
		}
	}
	
	if (sockNew != INVALID_SOCKET)
		closesocket(sockNew);
	
	if (buf)
		OOBase::HeapFree(buf);
	
	delete pOv;
	
	return err;
}

void SocketAcceptor::Acceptor::accept_ready(PVOID lpParameter, BOOLEAN)
{
	SocketAcceptor::Acceptor* pThis = static_cast<SocketAcceptor::Acceptor*>(lpParameter);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	WSANETWORKEVENTS events = {0};
	if (WSAEnumNetworkEvents(pThis->m_socket,pThis->m_hEvent,&events) == 0)
	{
		if ((events.lNetworkEvents & FD_ACCEPT) && events.iErrorCode[FD_ACCEPT] == 0)
		{
			pThis->do_accept(guard);
		}
	}
}

void SocketAcceptor::Acceptor::on_completion(HANDLE /*hSocket*/, DWORD /*dwBytes*/, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{	
	SocketAcceptor::Acceptor* pThis = reinterpret_cast<SocketAcceptor::Acceptor*>(pOv->m_extras[0]);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	// Perform the callback
	pThis->on_accept(static_cast<SOCKET>(pOv->m_extras[1]),true,dwErr,reinterpret_cast<void*>(pOv->m_extras[2]),guard);
	
	delete pOv;
}

bool SocketAcceptor::Acceptor::on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf, OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	bool bSignal = false;
	bool bAgain = false;
	if (bRemove)
	{
		--m_pending;
		
		bAgain = (m_pending < m_backlog);
		bSignal = (m_pending == 0 && m_backlog == 0);
	}
	
	// Keep us alive while the callbacks fire
	++m_refcount;
	
	guard.release();
	
	::AsyncSocket* pSocket = NULL;
	sockaddr* remote_addr = NULL;
	int remote_addr_len = 0;
		
	if (dwErr == 0)
	{
		// Get addresses
		sockaddr* local_addr = NULL;
		int local_addr_len = 0;
		OOBase::Win32::WSAGetAcceptExSockAddrs(m_socket,addr_buf,0,m_addr_len+16,m_addr_len+16,&local_addr,&local_addr_len,&remote_addr,&remote_addr_len);
			
		// Wrap the handle
		pSocket = new (std::nothrow) ::AsyncSocket(m_pProactor,hSocket);
		if (!pSocket)
			dwErr = ERROR_OUTOFMEMORY;
	}
	
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
		if (bRemove)
			OOBase::HeapFree(addr_buf);

		throw;
	}
		
	if (bRemove)
		OOBase::HeapFree(addr_buf);
	
	guard.acquire();
	
	if (bAgain)
	{
		// Try to accept some more connections
		dwErr = do_accept(guard);
		if (dwErr != 0)
		{
			guard.release();
			
			(*m_callback)(m_param,NULL,NULL,0,dwErr);
						
			guard.acquire();
		}
		
		bSignal = (m_pending == 0 && m_backlog == 0);
	}
	
	// See if we have been killed in the callback
	if (--m_refcount == 0)
	{
		guard.release();
		delete this;
		return true;
	}
	
	if (bSignal)
		m_condition.broadcast();
	
	return false;
}

OOSvrBase::Acceptor* OOSvrBase::detail::ProactorWin32::accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err)
{
	OOBase::Win32::WSAStartup();
	
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = new (std::nothrow) SocketAcceptor();
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
	else
	{
		err = pAcceptor->bind(this,param,callback,addr,addr_len);
		if (err != 0)
		{
			pAcceptor->release();
			pAcceptor = NULL;
		}
	}
		
	return pAcceptor;
}

OOSvrBase::AsyncSocket* OOSvrBase::detail::ProactorWin32::connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::timeval_t* timeout)
{
	SOCKET sock = INVALID_SOCKET;
	if ((sock = OOBase::Win32::create_socket(addr->sa_family,SOCK_STREAM,0,err)) == INVALID_SOCKET)
		return NULL;
	
	if ((err = OOBase::Win32::connect(sock,addr,addr_len,timeout)) != 0)
	{
		closesocket(sock);
		return NULL;
	}

	err = bind((HANDLE)sock);
	if (err != 0)
	{
		closesocket(sock);
		return NULL;
	}
	
	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this,sock);
	if (!pSocket)
	{
		unbind((HANDLE)sock);
		closesocket(sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pSocket;
}

OOSvrBase::AsyncSocket* OOSvrBase::detail::ProactorWin32::attach_socket(OOBase::socket_t sock, int& err)
{
	err = bind((HANDLE)sock);
	if (err != 0)
		return NULL;

	// The socket must have been opened as WSA_FLAG_OVERLAPPED!!!
	::AsyncSocket* pSocket = new (std::nothrow) ::AsyncSocket(this,sock);
	if (!pSocket)
	{
		unbind((HANDLE)sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pSocket;
}

#endif // _WIN32
