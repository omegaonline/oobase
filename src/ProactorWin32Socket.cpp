///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#include "ProactorWin32.h"

#if defined(_WIN32)

#if !defined(MSG_WAITALL)
#define MSG_WAITALL 0x8
#endif

#include "../include/OOBase/Condition.h"

#include "Win32Socket.h"

namespace
{
	class Win32AsyncSocket : public OOBase::AsyncSocket
	{
		friend class OOBase::AllocatorInstance;

	public:
		Win32AsyncSocket(OOBase::detail::ProactorWin32* pProactor, SOCKET hSocket, OOBase::AllocatorInstance& allocator);
		
		int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes);
		int recv_msg(void* param, recv_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer, size_t data_bytes);
		int send(void* param, send_callback_t callback, OOBase::Buffer* buffer);
		int send_v(void* param, send_v_callback_t callback, OOBase::Buffer* buffers[], size_t count);
		int send_msg(void* param, send_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer);
		int shutdown(bool bSend, bool bRecv);

	protected:
		OOBase::AllocatorInstance& get_allocator()
		{
			return m_allocator;
		}

	private:
		virtual ~Win32AsyncSocket();

		void destroy()
		{
			m_allocator.delete_free(this);
		}

		OOBase::detail::ProactorWin32* m_pProactor;
		OOBase::AllocatorInstance&     m_allocator;
		SOCKET                         m_hSocket;
					
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_recv_msg(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send_msg(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
	};
}

Win32AsyncSocket::Win32AsyncSocket(OOBase::detail::ProactorWin32* pProactor, SOCKET hSocket, OOBase::AllocatorInstance& allocator) :
		m_pProactor(pProactor),
		m_allocator(allocator),
		m_hSocket(hSocket)
{ }

Win32AsyncSocket::~Win32AsyncSocket()
{
	OOBase::Net::close_socket(m_hSocket);

	m_pProactor->unbind();
}

int Win32AsyncSocket::recv(void* param, OOBase::AsyncSocket::recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
{
	int err = 0;
	if (bytes)
	{
		if (!buffer)
			return ERROR_INVALID_PARAMETER;

		err = buffer->space(bytes);
		if (err)
			return err;
	}
	else if (!buffer || !buffer->space())
		return 0;

	if (!callback)
		return ERROR_INVALID_PARAMETER;

	if (bytes > u_long(-1) || bytes > DWORD(-1))
		return ERROR_BUFFER_OVERFLOW;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	err = m_pProactor->new_overlapped(pOv,&on_recv);
	if (err != 0)
		return err;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	buffer->addref();
	
	WSABUF wsa_buf;
	wsa_buf.buf = buffer->wr_ptr();
	wsa_buf.len = static_cast<u_long>(bytes);
	
	DWORD dwFlags = (bytes != 0 ? MSG_WAITALL : 0);
	if (WSARecv(m_hSocket,&wsa_buf,1,NULL,&dwFlags,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	DWORD dwRead = 0;
	if (!WSAGetOverlappedResult(m_hSocket,pOv,&dwRead,TRUE,&dwFlags))
		err = WSAGetLastError();

	on_recv((HANDLE)m_hSocket,dwRead,err,pOv);

	return 0;
}

void Win32AsyncSocket::on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	recv_callback_t callback = reinterpret_cast<recv_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));

	pOv->m_pProactor->delete_overlapped(pOv);
		
	// Call callback
	if (callback)
	{
		buffer->wr_ptr(dwBytes);

		(*callback)(param,buffer,dwErr);
	}
}

int Win32AsyncSocket::recv_msg(void* param, recv_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer, size_t data_bytes)
{
	int err = 0;
	if (data_bytes)
	{
		if (!data_buffer)
			return ERROR_INVALID_PARAMETER;

		err = data_buffer->space(data_bytes);
		if (err)
			return err;
	}
	else if ((!data_buffer || !data_buffer->space()) && (!ctl_buffer || !ctl_buffer->space()))
		return 0;

	if (!data_buffer || !data_buffer->space() || !ctl_buffer || !ctl_buffer->space() || !callback)
		return ERROR_INVALID_PARAMETER;

	if (ctl_buffer->space() > u_long(-1))
		return ERROR_BUFFER_OVERFLOW;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	err = m_pProactor->new_overlapped(pOv,&on_recv);
	if (err != 0)
		return err;

	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(data_buffer);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(ctl_buffer);
	data_buffer->addref();
	ctl_buffer->addref();

	WSABUF wsa_buf;
	wsa_buf.buf = data_buffer->wr_ptr();
	wsa_buf.len = static_cast<u_long>(data_bytes);

	WSAMSG msg = {0};
	msg.lpBuffers = &wsa_buf;
	msg.dwBufferCount = 1;
	msg.Control.buf = ctl_buffer->wr_ptr();
	msg.Control.len = static_cast<u_long>(ctl_buffer->space());
	msg.dwFlags = 0;

	// Increase and zero the ctl_buffer wr_ptr() as we have no way of knowing how much data is read
	ctl_buffer->wr_ptr(msg.Control.len);
	memset(msg.Control.buf,0,msg.Control.len);

	if (OOBase::Win32::WSARecvMsg(m_hSocket,&msg,NULL,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}

	DWORD dwRead = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(m_hSocket,pOv,&dwRead,TRUE,&dwFlags))
		err = WSAGetLastError();

	on_recv_msg((HANDLE)m_hSocket,dwRead,err,pOv);

	return 0;
}

void Win32AsyncSocket::on_recv_msg(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	recv_msg_callback_t callback = reinterpret_cast<recv_msg_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> data_buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));
	OOBase::RefPtr<OOBase::Buffer> ctl_buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]));

	pOv->m_pProactor->delete_overlapped(pOv);

	// Call callback
	if (callback)
	{
		data_buffer->wr_ptr(dwBytes);

		(*callback)(param,data_buffer,ctl_buffer,dwErr);
	}
}

int Win32AsyncSocket::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	size_t bytes = (buffer ? buffer->length() : 0);
	if (bytes == 0)
		return 0;

	if (!buffer)
		return EINVAL;

	if (bytes > u_long(-1) || bytes > DWORD(-1))
		return ERROR_BUFFER_OVERFLOW;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
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
	
	if (WSASend(m_hSocket,&wsa_buf,1,NULL,0,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
		
	DWORD dwSent = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(m_hSocket,pOv,&dwSent,TRUE,&dwFlags))
		err = WSAGetLastError();
	
	on_send((HANDLE)m_hSocket,dwSent,err,pOv);

	return 0;
}

void Win32AsyncSocket::on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_callback_t callback = reinterpret_cast<send_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));

	pOv->m_pProactor->delete_overlapped(pOv);

	// Call callback
	if (callback)
	{
		buffer->rd_ptr(dwBytes);

		(*callback)(param,buffer,dwErr);
	}
}

int Win32AsyncSocket::send_v(void* param, send_v_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return ERROR_INVALID_PARAMETER;

	if (count > DWORD(-1))
		return ERROR_BUFFER_OVERFLOW;

	OOBase::StackArrayPtr<WSABUF,8> wsa_bufs(count);
	if (!wsa_bufs)
		return ERROR_OUTOFMEMORY;
	
	DWORD buf_count = 0;
	size_t total = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = (buffers[i] ? buffers[i]->length() : 0);
		if (len)
		{
			if (len > u_long(-1) || total > DWORD(-1) - len)
				return ERROR_BUFFER_OVERFLOW;

			total += len;

			wsa_bufs[buf_count].len = static_cast<u_long>(len);
			wsa_bufs[buf_count].buf = const_cast<char*>(buffers[i]->rd_ptr());

			if (++buf_count == DWORD(-1))
				return ERROR_BUFFER_OVERFLOW;
		}
	}

	if (total == 0)
		return 0;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int err = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (err != 0)
		return err;

	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);

	OOBase::Buffer** ov_buffers = static_cast<OOBase::Buffer**>(m_pProactor->internal_allocate((buf_count) * sizeof(OOBase::Buffer*),OOBase::alignof<OOBase::Buffer*>::value));
	if (!ov_buffers)
	{
		m_pProactor->delete_overlapped(pOv);
		return ERROR_OUTOFMEMORY;
	}

	size_t j = 0;
	for (size_t i=0;i<count;++i)
	{
		if (buffers[i] && buffers[i]->length())
		{
			ov_buffers[j++] = buffers[i];
			buffers[i]->addref();
		}
	}

	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(ov_buffers);
	pOv->m_extras[3] = buf_count;

	if (WSASend(m_hSocket,wsa_bufs,buf_count,NULL,0,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}
	
	DWORD dwSent = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(m_hSocket,pOv,&dwSent,TRUE,&dwFlags))
		err = WSAGetLastError();

	on_send_v((HANDLE)m_hSocket,dwSent,err,pOv);
	
	return 0;
}

void Win32AsyncSocket::on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_v_callback_t callback = reinterpret_cast<send_v_callback_t>(pOv->m_extras[1]);
	OOBase::Buffer** buffers = reinterpret_cast<OOBase::Buffer**>(pOv->m_extras[3]);
	size_t count = pOv->m_extras[3];

	// Call callback
	if (callback)
	{
		// Update rd_ptrs
		for (size_t i=0;i<count && dwBytes;++i)
		{
			size_t len = buffers[i]->length();
			if (dwBytes >= len)
			{
				buffers[i]->rd_ptr(len);
				dwBytes -= len;
			}
			else
			{
				buffers[i]->rd_ptr(dwBytes);
				dwBytes = 0;
			}
		}

#if defined(OOBASE_HAVE_EXCEPTIONS)
		try
		{
#endif
			(*callback)(param,buffers,count,dwErr);
#if defined(OOBASE_HAVE_EXCEPTIONS)
		}
		catch (...)
		{
			for (size_t i=0;i<count;++i)
				buffers[i]->release();

			pOv->m_pProactor->internal_free(buffers);
			throw;
		}
#endif
	}

	for (size_t i=0;i<count;++i)
		buffers[i]->release();

	pOv->m_pProactor->internal_free(buffers);

	pOv->m_pProactor->delete_overlapped(pOv);
}

int Win32AsyncSocket::send_msg(void* param, send_msg_callback_t callback, OOBase::Buffer* data_buffer, OOBase::Buffer* ctl_buffer)
{
	size_t data_len = (data_buffer ? data_buffer->length() : 0);
	size_t ctl_len = (ctl_buffer ? ctl_buffer->length() : 0);

	if (!data_len && !ctl_len)
		return 0;

	if (!data_len || !ctl_len)
		return ERROR_INVALID_PARAMETER;

	if (data_len > u_long(-1) || ctl_len > u_long(-1))
		return ERROR_BUFFER_OVERFLOW;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int err = m_pProactor->new_overlapped(pOv,&on_send_msg);
	if (err != 0)
		return err;

	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(data_buffer);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(ctl_buffer);
	data_buffer->addref();
	ctl_buffer->addref();

	WSABUF wsa_buf;
	wsa_buf.buf = const_cast<char*>(data_buffer->rd_ptr());
	wsa_buf.len = static_cast<u_long>(data_len);

	WSAMSG msg = {0};
	msg.lpBuffers = &wsa_buf;
	msg.dwBufferCount = 1;
	msg.Control.buf = const_cast<char*>(ctl_buffer->rd_ptr());
	msg.Control.len = static_cast<u_long>(ctl_len);
	msg.dwFlags = 0;

	if (OOBase::Win32::WSASendMsg(m_hSocket,&msg,0,NULL,pOv,NULL) == SOCKET_ERROR)
	{
		err = GetLastError();
		if (err == WSA_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
	}

	DWORD dwSent = 0;
	DWORD dwFlags = 0;
	if (!WSAGetOverlappedResult(m_hSocket,pOv,&dwSent,TRUE,&dwFlags))
		err = WSAGetLastError();

	on_send_msg((HANDLE)m_hSocket,dwSent,err,pOv);

	return 0;
}

void Win32AsyncSocket::on_send_msg(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_msg_callback_t callback = reinterpret_cast<send_msg_callback_t>(pOv->m_extras[1]);
	OOBase::Buffer* buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));
	OOBase::Buffer* ctl_buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]));

	pOv->m_pProactor->delete_overlapped(pOv);

	// Call callback
	if (callback)
	{
		buffer->rd_ptr(dwBytes);

		(*callback)(param,buffer,ctl_buffer,dwErr);
	}
}

int Win32AsyncSocket::shutdown(bool bSend, bool bRecv)
{
	int how = -1;
	if (bSend && bRecv)
		how = SD_BOTH;
	else if (bSend)
		how = SD_SEND;
	else if (bRecv)
		how = SD_RECEIVE;

	return (how != -1 ? ::shutdown(m_hSocket,how) : 0);
}

namespace
{
	class InternalAcceptor
	{
	public:
		InternalAcceptor(OOBase::detail::ProactorWin32* pProactor, void* param, OOBase::Proactor::accept_callback_t callback, OOBase::AllocatorInstance& allocator);
		
		int listen(size_t backlog);
		int stop(bool destroy);
		int bind(const sockaddr* addr, socklen_t addr_len);
		
	private:
		OOBase::detail::ProactorWin32*                   m_pProactor;
		OOBase::AllocatorInstance&                       m_allocator;
		OOBase::Condition::Mutex                         m_lock;
		OOBase::Condition                                m_condition;
		sockaddr_storage                                 m_addr;
		socklen_t                                        m_addr_len;
		SOCKET                                           m_socket;
		size_t                                           m_backlog;
		size_t                                           m_pending;
		size_t                                           m_refcount;
		OOBase::Win32::SmartHandle                       m_hEvent;
		HANDLE                                           m_hWait;
		void*                                            m_param;
		OOBase::Proactor::accept_callback_t              m_callback;

		static void on_completion(HANDLE hSocket, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void CALLBACK accept_ready(PVOID lpParameter, BOOLEAN TimerOrWaitFired);

		int init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
		bool on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf, OOBase::Guard<OOBase::Condition::Mutex>& guard);
		int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
	};

	class SocketAcceptor : public OOBase::Acceptor
	{
	public:
		SocketAcceptor();
		
		int listen(size_t backlog);
		int stop();
		int bind(OOBase::detail::ProactorWin32* pProactor, void* param, OOBase::Proactor::accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, OOBase::AllocatorInstance& allocator);
	
	private:
		virtual ~SocketAcceptor();

		InternalAcceptor* m_pAcceptor;
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

int SocketAcceptor::bind(OOBase::detail::ProactorWin32* pProactor, void* param, OOBase::Proactor::accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, OOBase::AllocatorInstance& allocator)
{
	m_pAcceptor = allocator.allocate_new<InternalAcceptor>(pProactor,param,callback,allocator);
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

InternalAcceptor::InternalAcceptor(OOBase::detail::ProactorWin32* pProactor, void* param, OOBase::Proactor::accept_callback_t callback, OOBase::AllocatorInstance& allocator) :
		m_pProactor(pProactor),
		m_allocator(allocator),
		m_addr_len(0),
		m_socket(INVALID_SOCKET),
		m_backlog(0),
		m_pending(0),
		m_refcount(1),
		m_hWait(NULL),
		m_param(param),
		m_callback(callback)
{ }

int InternalAcceptor::bind(const sockaddr* addr, socklen_t addr_len)
{
	memcpy(&m_addr,addr,addr_len);
	m_addr_len = addr_len;
	
	// Create an event to wait on
	m_hEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	if (!m_hEvent.is_valid())
		return GetLastError();

	// Set up a registered wait
	if (!RegisterWaitForSingleObject(&m_hWait,m_hEvent,&accept_ready,this,INFINITE,WT_EXECUTEDEFAULT))
		return GetLastError();

	return 0;
}

int InternalAcceptor::listen(size_t backlog)
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

int InternalAcceptor::stop(bool destroy)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (destroy && m_hWait != NULL)
	{
		guard.release();
		
		if (!UnregisterWaitEx(m_hWait,INVALID_HANDLE_VALUE))
			return GetLastError();
	
		guard.acquire();
		
		m_hWait = NULL;
	}
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		OOBase::Net::close_socket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	
	// Wait for all pending operations to complete
	while (m_pending != 0 && m_backlog == 0)
		m_condition.wait(m_lock);
		
	if (destroy && --m_refcount == 0)
	{
		guard.release();

		m_allocator.delete_free(this);
	}

	return 0;
}

int InternalAcceptor::init_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	// Create a new socket
	int err = 0;
	m_socket = OOBase::Net::open_socket(m_addr.ss_family,SOCK_STREAM,0,err);
	if (err)
		return err;
		
	// Bind to the address
	if (::bind(m_socket,(struct sockaddr*)&m_addr,m_addr_len) == SOCKET_ERROR)
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
				m_pProactor->unbind();
		}
	}
	
	if (err != 0)
	{
		OOBase::Net::close_socket(m_socket);
		m_socket = INVALID_SOCKET;
	}
	
	return err;
}

int InternalAcceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	int err = 0;
	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
		
	SOCKET sockNew = INVALID_SOCKET;
	void* buf = NULL;
	
	while (m_pending < m_backlog)
	{
		sockNew = OOBase::Net::open_socket(m_addr.ss_family,SOCK_STREAM,0,err);
		if (err)
			break;

		err = m_pProactor->bind((HANDLE)sockNew);
		if (err != 0)
			break;
							
		if (!pOv)
		{
			buf = m_allocator.allocate((m_addr_len+16)*2,OOBase::alignof<struct sockaddr>::value);
			if (!buf)
			{
				m_pProactor->unbind();
				err = ERROR_OUTOFMEMORY;
				break;
			}

			err = m_pProactor->new_overlapped(pOv,&on_completion);
			if (err != 0)
			{
				m_pProactor->unbind();
				m_allocator.free(buf);
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
				m_pProactor->unbind();
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
		if (on_accept(sockNew,false,0,buf,guard))
		{
			// We have self destructed
			break;
		}
	}
	
	if (sockNew != INVALID_SOCKET)
		OOBase::Net::close_socket(sockNew);
	
	if (buf)
		m_allocator.free(buf);
	
	if (pOv)
		m_pProactor->delete_overlapped(pOv);
	
	return err;
}

void InternalAcceptor::accept_ready(PVOID lpParameter, BOOLEAN)
{
	InternalAcceptor* pThis = static_cast<InternalAcceptor*>(lpParameter);
	
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

void InternalAcceptor::on_completion(HANDLE /*hSocket*/, DWORD /*dwBytes*/, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{	
	InternalAcceptor* pThis = reinterpret_cast<InternalAcceptor*>(pOv->m_extras[0]);
	SOCKET s = static_cast<SOCKET>(pOv->m_extras[1]);
	void* p = reinterpret_cast<void*>(pOv->m_extras[2]);
	
	pOv->m_pProactor->delete_overlapped(pOv);

	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	// Perform the callback
	pThis->on_accept(s,true,dwErr,p,guard);
}

bool InternalAcceptor::on_accept(SOCKET hSocket, bool bRemove, DWORD dwErr, void* addr_buf, OOBase::Guard<OOBase::Condition::Mutex>& guard)
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
	
	Win32AsyncSocket* pSocket = NULL;
	sockaddr* remote_addr = NULL;
	int remote_addr_len = 0;
		
	if (dwErr == 0)
	{
		// Get addresses
		sockaddr* local_addr = NULL;
		INT local_addr_len = 0;
		OOBase::Win32::WSAGetAcceptExSockAddrs(m_socket,addr_buf,0,m_addr_len+16,m_addr_len+16,&local_addr,&local_addr_len,&remote_addr,&remote_addr_len);
			
		// Wrap the handle
		pSocket = m_allocator.allocate_new<Win32AsyncSocket>(m_pProactor,hSocket,m_allocator);
		if (!pSocket)
			dwErr = ERROR_OUTOFMEMORY;

	}
	
	if (dwErr != 0)
	{
		remote_addr = NULL;
		remote_addr_len = 0;
		OOBase::Net::close_socket(hSocket);
	}
	
	if (m_callback)
		(*m_callback)(m_param,pSocket,remote_addr,remote_addr_len,dwErr);
			
	if (bRemove)
		m_allocator.free(addr_buf);
	
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

		m_allocator.delete_free(this);

		return true;
	}
	
	if (bSignal)
		m_condition.broadcast();
	
	return false;
}

OOBase::Acceptor* OOBase::detail::ProactorWin32::accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err, OOBase::AllocatorInstance& allocator)
{
	Win32::WSAStartup();
	
	// Make sure we have valid inputs
	if (!callback || !addr || addr_len == 0)
	{
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	SocketAcceptor* pAcceptor = allocator.allocate_new<SocketAcceptor>();
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
	else
	{
		err = pAcceptor->bind(this,param,callback,addr,addr_len,allocator);
		if (err != 0)
		{
			pAcceptor->release();
			pAcceptor = NULL;
		}
	}
		
	return pAcceptor;
}

OOBase::AsyncSocket* OOBase::detail::ProactorWin32::connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout, OOBase::AllocatorInstance& allocator)
{
	SOCKET sock = Net::open_socket(addr->sa_family,SOCK_STREAM,0,err);
	if (err)
		return NULL;
	
	if ((err = Net::connect(sock,addr,addr_len,timeout)) != 0)
	{
		Net::close_socket(sock);
		return NULL;
	}

	err = bind((HANDLE)sock);
	if (err != 0)
	{
		Net::close_socket(sock);
		return NULL;
	}
	
	Win32AsyncSocket* pSocket = allocator.allocate_new<Win32AsyncSocket>(this,sock,allocator);
	if (!pSocket)
	{
		unbind();
		Net::close_socket(sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pSocket;
}

OOBase::AsyncSocket* OOBase::detail::ProactorWin32::attach(socket_t sock, int& err, OOBase::AllocatorInstance& allocator)
{
	err = bind((HANDLE)sock);
	if (err != 0)
		return NULL;

	// The socket must have been opened as WSA_FLAG_OVERLAPPED!!!
	Win32AsyncSocket* pSocket = allocator.allocate_new<Win32AsyncSocket>(this,sock,allocator);
	if (!pSocket)
	{
		unbind();
		err = ERROR_OUTOFMEMORY;
	}

	return pSocket;
}

#endif // _WIN32
