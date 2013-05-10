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

#include "../include/OOBase/Mutex.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Socket.h"
#include "../include/OOBase/Win32.h"

#include "Win32Socket.h"

#if defined(_WIN32)

namespace
{
	struct TxDirection
	{
		enum
		{
			Recv = 1,
			Send = 2
		};
	};

	class Pipe :
			public OOBase::Socket,
			public OOBase::AllocatorNew<OOBase::CrtAllocator>
	{
	public:
		explicit Pipe(HANDLE handle);
		virtual ~Pipe();

		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		size_t recv_msg(void* data_buf, size_t data_len, OOBase::Buffer* ctl_buffer, int& err, const OOBase::Timeout& timeout);
		
		size_t send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		size_t send_msg(const void* data_buf, size_t data_len, const void* ctl_buf, size_t ctl_len, int& err, const OOBase::Timeout& timeout);

		int shutdown(bool bSend, bool bRecv);
		int close();
		OOBase::socket_t get_handle() const;
					
	private:
		OOBase::Win32::SmartHandle m_handle;
		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;
		OOBase::Win32::SmartHandle m_recv_event;
		OOBase::Win32::SmartHandle m_send_event;
		bool                       m_send_allowed;
		bool                       m_recv_allowed;
		
		size_t recv_i(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		size_t send_i(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
	};
}

Pipe::Pipe(HANDLE handle) :
		m_handle(handle),
		m_send_allowed(true),
		m_recv_allowed(true)
{
}

Pipe::~Pipe()
{
}


OOBase::socket_t Pipe::get_handle() const
{
	return (OOBase::socket_t)(HANDLE)m_handle;
}

size_t Pipe::send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (!buf)
	{
		err = ERROR_INVALID_PARAMETER;
		return 0;
	}

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
	{
		err = WSAETIMEDOUT;
		return 0;
	}

	return send_i(buf,len,err,timeout);
}

int Pipe::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return ERROR_INVALID_PARAMETER;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
		return WSAETIMEDOUT;
	
	int err = 0;
	for (size_t i=0;i<count && err == 0 ;++i)
	{
		size_t to_send = (buffers[i] ? buffers[i]->length() : 0);
		if (to_send)
		{
			size_t len = send_i(buffers[i]->rd_ptr(),to_send,err,timeout);
			buffers[i]->wr_ptr(len);
		}
	}

	return err;
}

size_t Pipe::send_msg(const void*, size_t, const void*, size_t, int& err, const OOBase::Timeout&)
{
	err = ERROR_NOT_SUPPORTED;
	return 0;
}

size_t Pipe::send_i(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
	if (!m_send_allowed)
	{
		err = WSAESHUTDOWN;
		return 0;
	}

	if (!m_send_event.is_valid())
	{
		m_send_event = CreateEventW(NULL,TRUE,FALSE,NULL);
		if (!m_send_event.is_valid())
		{
			err = GetLastError();
			return 0;
		}
	}

	OVERLAPPED ov = {0};
	ov.hEvent = m_send_event;

	const char* cbuf = static_cast<const char*>(buf);
	size_t to_write = len;
	while (to_write > 0)
	{
		if (!WriteFile(m_handle,cbuf,static_cast<DWORD>(to_write),NULL,&ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING)
			{
				if (!timeout.is_infinite())
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
					if (dwWait == WAIT_TIMEOUT)
						err = WSAETIMEDOUT;
					else if (dwWait != WAIT_OBJECT_0)
						err = GetLastError();
				}
			}
			else
				err = dwErr;

			if (err)
				break;
		}
		
		DWORD dwWritten = 0;
		if (!GetOverlappedResult(m_handle,&ov,&dwWritten,TRUE))
		{
			err = GetLastError();
			break;
		}

		cbuf += dwWritten;
		to_write -= dwWritten;

		if (!m_send_allowed)
		{
			err = WSAESHUTDOWN;
			break;
		}
	}

	return (len - to_write);
}

/*int Pipe::send_socket(OOBase::socket_t sock, DWORD pid, const OOBase::Timeout& timeout)
{
	OOBase::Win32::WSAStartup();

	WSAPROTOCOL_INFOW pi = {0};
	if (WSADuplicateSocketW(sock,pid,&pi) != 0)
		return WSAGetLastError();

	int err = 0;
	send(&pi,sizeof(pi),err,timeout);
	return err;
}*/

size_t Pipe::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (!buf)
	{
		err = ERROR_INVALID_PARAMETER;
		return 0;
	}

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
	{
		err = WSAETIMEDOUT;
		return 0;
	}

	return recv_i(buf,len,bAll,err,timeout);
}

int Pipe::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	if (!buffers)
		return ERROR_INVALID_PARAMETER;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
		return WSAETIMEDOUT;

	int err = 0;
	for (size_t i=0;i<count && err == 0 ;++i)
	{
		if (buffers[i] && buffers[i]->space() > 0)
		{
			size_t len = recv_i(buffers[i]->wr_ptr(),buffers[i]->space(),true,err,timeout);
			buffers[i]->wr_ptr(len);
		}
	}
	
	return err;
}

size_t Pipe::recv_msg(void*, size_t, OOBase::Buffer*, int& err, const OOBase::Timeout&)
{
	err = ERROR_NOT_SUPPORTED;
	return 0;
}

size_t Pipe::recv_i(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	if (!m_recv_allowed)
	{
		err = WSAESHUTDOWN;
		return 0;
	}

	if (!m_recv_event.is_valid())
	{
		m_recv_event = CreateEventW(NULL,TRUE,FALSE,NULL);
		if (!m_recv_event.is_valid())
		{
			err = GetLastError();
			return 0;
		}
	}

	OVERLAPPED ov = {0};
	ov.hEvent = m_recv_event;
		
	char* cbuf = static_cast<char*>(buf);
	size_t to_read = len;
	while (to_read > 0)
	{
		if (!ReadFile(m_handle,cbuf,static_cast<DWORD>(to_read),NULL,&ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_MORE_DATA)
			{
				if (dwErr == ERROR_IO_PENDING)
				{
					if (!timeout.is_infinite())
					{
						DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
						if (dwWait == WAIT_TIMEOUT)
							err = WAIT_TIMEOUT;
						else if (dwWait != WAIT_OBJECT_0)
							err = GetLastError();
					}
				}
				else
					err = dwErr;

				if (err)
					break;
			}
		}

		DWORD dwRead = 0;
		if (!GetOverlappedResult(m_handle,&ov,&dwRead,TRUE))
		{
			err = GetLastError();
			break;
		}

		cbuf += dwRead;
		to_read -= dwRead;

		if (!bAll && dwRead > 0)
			break;

		if (!m_recv_allowed)
		{
			err = WSAESHUTDOWN;
			break;
		}
	}

	return (len - to_read);
}

/*int Pipe::recv_socket(OOBase::socket_t& sock, const OOBase::Timeout& timeout)
{
	OOBase::Win32::WSAStartup();

	int err = 0;
	WSAPROTOCOL_INFOW pi = {0};
	recv(&pi,sizeof(pi),true,err,timeout);
	if (err)
		return err;

	sock = WSASocketW(pi.iAddressFamily,pi.iSocketType,pi.iProtocol,&pi,0,WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
		return WSAGetLastError();

	return 0;
}*/

int Pipe::shutdown(bool bSend, bool bRecv)
{
	if (bSend)
	{
		// Attempt to flush and disconnect (this may fail if we are not the 'server')
		::FlushFileBuffers(m_handle);
		::DisconnectNamedPipe(m_handle);

		m_send_allowed = false;

		// This will halt all send's
		if (m_send_event.is_valid())
			::SetEvent(m_send_event);

		// This ensures there are no other sends in progress...
		OOBase::Guard<OOBase::Mutex> guard(m_send_lock);
	}

	if (bRecv)
	{
		m_recv_allowed = false;

		// This will halt all recv's
		if (m_recv_event.is_valid())
			::SetEvent(m_recv_event);

		// This ensures there are no other recvs in progress...
		OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);
	}

	return 0;
}

int Pipe::close()
{
	// Attempt to flush and disconnect (this may fail if we are not the 'server')
	::FlushFileBuffers(m_handle);
	::DisconnectNamedPipe(m_handle);

	m_send_allowed = false;
	m_recv_allowed = false;

	// This will halt all send's
	if (m_send_event.is_valid())
		SetEvent(m_send_event);
	
	// This will halt all recv's
	if (m_recv_event.is_valid())
		SetEvent(m_recv_event);

	// This ensures there are no other sends in progress...
	OOBase::Guard<OOBase::Mutex> guard1(m_send_lock);

	// This ensures there are no other recvs in progress...
	OOBase::Guard<OOBase::Mutex> guard2(m_recv_lock);

	return m_handle.close();
}

int OOBase::Net::accept(HANDLE hPipe, const Timeout& timeout)
{
	OVERLAPPED ov = {0};
	ov.hEvent = CreateEventW(NULL,TRUE,TRUE,NULL);
	if (!ov.hEvent)
		return GetLastError();

	// Control handle lifetime
	OOBase::Win32::SmartHandle ev(ov.hEvent);

	DWORD dwErr = 0;
	if (ConnectNamedPipe(hPipe,&ov))
		dwErr = ERROR_PIPE_CONNECTED;
	else
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
			dwErr = 0;
	}

	if (dwErr == ERROR_PIPE_CONNECTED)
	{
		dwErr = 0;
		if (!SetEvent(ov.hEvent))
			return GetLastError();
	}

	if (dwErr != 0)
		return dwErr;

	if (!timeout.is_infinite())
	{
		DWORD dwRes = WaitForSingleObject(ov.hEvent,timeout.millisecs());
		if (dwRes == WAIT_TIMEOUT)
			return ERROR_TIMEOUT;
		else if (dwRes != WAIT_OBJECT_0)
			return GetLastError();
	}

	DWORD dw = 0;
	if (!GetOverlappedResult(hPipe,&ov,&dw,TRUE))
		return GetLastError();

	return 0;
}

OOBase::Socket* OOBase::Socket::attach(HANDLE hPipe, int& err)
{
	Pipe* pSocket = new Pipe(hPipe);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::connect(const char* path, int& err, const Timeout& timeout)
{
	OOBase::StackAllocator<256> allocator;
	OOBase::LocalString pipe_name(allocator);
	err = pipe_name.assign("\\\\.\\pipe\\");
	if (err == 0)
		err = pipe_name.append(path);
	if (err != 0)
		return NULL;

	OOBase::TempPtr<wchar_t> wname(allocator);
	err = OOBase::Win32::utf8_to_wchar_t(pipe_name.c_str(),wname);
	if (err)
		return NULL;

	Win32::SmartHandle hPipe;
	for (;;)
	{
		hPipe = CreateFileW(wname,
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
			err = dwErr;
			return 0;
		}

		DWORD dwWait = NMPWAIT_USE_DEFAULT_WAIT;
		if (!timeout.is_infinite())
		{
			dwWait = timeout.millisecs();
			if (dwWait == NMPWAIT_USE_DEFAULT_WAIT)
				dwWait = 1;
		}

		if (!WaitNamedPipeW(wname,dwWait))
		{
			err = GetLastError();
			if (err == ERROR_SEM_TIMEOUT)
				err = WSAETIMEDOUT;

			return 0;
		}
	}
	
	Pipe* pPipe = new Pipe(hPipe);
	if (!pPipe)
		err = ERROR_OUTOFMEMORY;
	else
		hPipe.detach();

	return pPipe;
}

#endif // _WIN32
