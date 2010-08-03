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

#include "../include/OOBase/Socket.h"

#if defined(_WIN32)

namespace
{
	class Socket : public OOBase::Socket
	{
	public:
		explicit Socket(SOCKET hSocket);
		explicit Socket(HANDLE hPipe);
		virtual ~Socket();

		size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* wait = 0);
		int send(const void* buf, size_t len, const OOBase::timeval_t* wait = 0);
		void shutdown(bool bSend, bool bRecv);
		
	private:
		OOBase::Win32::SmartHandle m_handle;
		OOBase::Win32::SmartHandle m_hReadEvent;
		OOBase::Win32::SmartHandle m_hWriteEvent;
	};

	Socket::Socket(SOCKET hSocket) :
			m_handle((HANDLE)hSocket)
	{
	}

	Socket::Socket(HANDLE hPipe) :
			m_handle(hPipe)
	{
	}

	Socket::~Socket()
	{
	}

	int Socket::send(const void* buf, size_t len, const OOBase::timeval_t* timeout)
	{
		if (!m_hWriteEvent.is_valid())
		{
			m_hWriteEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_hWriteEvent.is_valid())
				return GetLastError();
		}

		OVERLAPPED ov = {0};
		ov.hEvent = m_hWriteEvent;

		const char* cbuf = reinterpret_cast<const char*>(buf);
		while (len > 0)
		{
			DWORD dwWritten = 0;
			if (WriteFile(m_handle,cbuf,static_cast<DWORD>(len),&dwWritten,&ov))
			{
				if (!SetEvent(m_hWriteEvent))
					return GetLastError();
			}
			else
			{
				DWORD ret = GetLastError();
				if (ret != ERROR_IO_PENDING)
					return ret;

				// Wait...
				if (timeout)
				{
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(m_handle);
						return WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						ret = GetLastError();
						CancelIo(m_handle);
						return ret;
					}
				}
			}

			if (!GetOverlappedResult(m_handle,&ov,&dwWritten,TRUE))
			{
				DWORD ret = GetLastError();
				CancelIo(m_handle);
				return ret;
			}

			cbuf += dwWritten;
			len -= dwWritten;
		}

		return 0;
	}

	size_t Socket::recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout)
	{
		assert(perr);
		*perr = 0;

		if (!m_hReadEvent.is_valid())
		{
			m_hReadEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_hReadEvent.is_valid())
			{
				*perr = GetLastError();
				return 0;
			}
		}

		OVERLAPPED ov = {0};
		ov.hEvent = m_hReadEvent;

		char* cbuf = reinterpret_cast<char*>(buf);
		size_t total = len;
		while (total > 0)
		{
			DWORD dwRead = 0;
			if (ReadFile(m_handle,cbuf,static_cast<DWORD>(total),&dwRead,&ov))
			{
				if (!SetEvent(m_hReadEvent))
				{
					*perr = GetLastError();
					return 0;
				}
			}
			else
			{
				*perr = GetLastError();
				if (*perr == ERROR_MORE_DATA)
					dwRead = static_cast<DWORD>(total);
				else
				{
					if (*perr != ERROR_IO_PENDING)
						return (len - total);

					// Wait...
					if (timeout)
					{
						DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
						if (dwWait == WAIT_TIMEOUT)
						{
							CancelIo(m_handle);
							*perr = WAIT_TIMEOUT;
							return (len - total);
						}
						else if (dwWait != WAIT_OBJECT_0)
						{
							*perr = GetLastError();
							CancelIo(m_handle);
							return (len - total);
						}
					}
				}
			}

			if (!GetOverlappedResult(m_handle,&ov,&dwRead,TRUE))
			{
				*perr = GetLastError();
				CancelIo(m_handle);
				return (len - total);
			}

			cbuf += dwRead;
			total -= dwRead;
		}

		*perr = 0;
		return len;
	}

	void Socket::shutdown(bool bSend, bool bRecv)
	{
		int how = -1;
		if (bSend)
			how = (bRecv ? SD_BOTH : SD_SEND);
		else if (bRecv)
			how = SD_RECEIVE;

		if (how != -1)
			::shutdown((SOCKET)(HANDLE)m_handle,how);
	}
}

OOBase::Socket* OOBase::Socket::connect_local(const std::string& path, int* perr, const timeval_t* wait)
{
	assert(perr);
	*perr = 0;

	std::string pipe_name = "\\\\.\\pipe\\" + path;

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
			*perr = dwErr;
			return 0;
		}

		DWORD dwWait = NMPWAIT_USE_DEFAULT_WAIT;
		if (wait)
			dwWait = wait->msec();

		if (!WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			*perr = GetLastError();
			return 0;
		}
	}

	::Socket* pSocket = 0;
	OOBASE_NEW(pSocket,::Socket(hPipe));
	if (!pSocket)
	{
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}

	hPipe.detach();

	return pSocket;
}

#if 0
OOBase::Socket* OOBase::Socket::connect(const std::string& address, const std::string& port, int* perr, const timeval_t* wait)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	OOBase::SOCKET sock = OOBase::connect(address,port,perr,wait);
	if (sock == INVALID_SOCKET)
		return 0;

	OOBase::Win32::Socket* pSocket = 0;
	OOBASE_NEW(pSocket,OOBase::Win32::Socket(sock));
	if (!pSocket)
	{
		*perr = ERROR_OUTOFMEMORY;
		closesocket(sock);
		return 0;
	}

	return pSocket;
}
#endif

#endif // _WIN32
