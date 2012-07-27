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

#include "../include/OOBase/GlobalNew.h"

#if defined(_WIN32)

#include "../include/OOBase/String.h"
#include "../include/OOBase/Socket.h"
#include "../include/OOBase/Win32.h"
#include "Win32Socket.h"

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

	class Pipe : public OOBase::Socket
	{
	public:
		Pipe(HANDLE handle);
		virtual ~Pipe();

		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		
		size_t send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);

		int recv_socket(OOBase::socket_t& sock, const OOBase::Timeout& timeout);
		int send_socket(OOBase::socket_t sock, DWORD pid, const OOBase::Timeout& timeout);
		
		void close();
					
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

size_t Pipe::send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
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

	int err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);
	if (!guard.acquire(timeout))
	{
		err = WSAETIMEDOUT;
		return 0;
	}
	
	for (size_t i=0;i<count && err == 0 ;++i)
	{
		if (buffers[i]->length() > 0)
		{
			size_t len = send_i(buffers[i]->rd_ptr(),buffers[i]->length(),err,timeout);
			buffers[i]->wr_ptr(len);
		}
	}

	return err;
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
		err = 0;
		DWORD dwWritten = 0;
		if (!WriteFile(m_handle,cbuf,static_cast<DWORD>(to_write),&dwWritten,&ov))
		{
			err = GetLastError();
			if (err != ERROR_OPERATION_ABORTED)
			{
				if (err != ERROR_IO_PENDING)
					break;

				err = 0;
				if (!timeout.is_infinite())
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(m_handle);
						err = WSAETIMEDOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						err = GetLastError();
						CancelIo(m_handle);
					}
				}
			
				if (!GetOverlappedResult(m_handle,&ov,&dwWritten,TRUE))
				{
					int dwErr = GetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;

					if (err != 0)
						break;
				}
			}
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

int Pipe::send_socket(OOBase::socket_t sock, DWORD pid, const OOBase::Timeout& timeout)
{
	OOBase::Win32::WSAStartup();

	WSAPROTOCOL_INFOW pi = {0};
	if (WSADuplicateSocketW(sock,pid,&pi) != 0)
		return WSAGetLastError();

	int err = 0;
	send(&pi,sizeof(pi),err,timeout);
	return err;
}

size_t Pipe::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
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

	int err = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);
	if (!guard.acquire(timeout))
	{
		err = WSAETIMEDOUT;
		return 0;
	}

	for (size_t i=0;i<count && err == 0 ;++i)
	{
		if (buffers[i]->space() > 0)
		{
			size_t len = recv_i(buffers[i]->wr_ptr(),buffers[i]->space(),true,err,timeout);
			buffers[i]->wr_ptr(len);
		}
	}
	
	return err;
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
		err = 0;
		DWORD dwRead = 0;
		if (!ReadFile(m_handle,cbuf,static_cast<DWORD>(to_read),&dwRead,&ov))
		{
			err = GetLastError();
			if (err == ERROR_MORE_DATA)
				dwRead = static_cast<DWORD>(to_read);
			else if (err != ERROR_OPERATION_ABORTED)
			{
				if (err != ERROR_IO_PENDING)
					break;

				err = 0;
				if (!timeout.is_infinite())
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(m_handle);
						err = WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						err = GetLastError();
						CancelIo(m_handle);
					}
				}
					
				if (!GetOverlappedResult(m_handle,&ov,&dwRead,TRUE))
				{
					int dwErr = GetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;

					if (err != 0)
						break;
				}
			}
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

int Pipe::recv_socket(OOBase::socket_t& sock, const OOBase::Timeout& timeout)
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
}

void Pipe::close()
{
	m_send_allowed = false;
	m_recv_allowed = false;
	
	// This will halt all recv's
	if (m_recv_event.is_valid())
		SetEvent(m_recv_event);

	// This will halt all send's
	if (m_send_event.is_valid())
		SetEvent(m_send_event);
	
	// This ensures there are no other sends or recvs in progress...
	OOBase::Guard<OOBase::Mutex> guard1(m_recv_lock);
	OOBase::Guard<OOBase::Mutex> guard2(m_send_lock);
}

OOBase::Socket* OOBase::Socket::attach_local(socket_t sock, int& err)
{
	OOBase::Socket* pSocket = new (std::nothrow) Pipe((HANDLE)sock);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int& err, const Timeout& timeout)
{
	OOBase::LocalString pipe_name;
	err = pipe_name.assign("\\\\.\\pipe\\");
	if (err == 0)
		err = pipe_name.append(path);
	if (err != 0)
		return NULL;

	Win32::SmartHandle hPipe;
	for (;;)
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

		if (!WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			err = GetLastError();
			if (err == ERROR_SEM_TIMEOUT)
				err = WSAETIMEDOUT;

			return 0;
		}
	}
	
	OOBase::Socket* pPipe = new (std::nothrow) Pipe(hPipe);
	if (!pPipe)
		err = ERROR_OUTOFMEMORY;
	else
		hPipe.detach();

	return pPipe;
}

#endif // _WIN32
