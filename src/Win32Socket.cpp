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

#include "../include/OOBase/STLAllocator.h"
#include "Win32Socket.h"

#if defined(_WIN32)

namespace
{
	class Socket : public OOBase::Win32::Socket
	{
	public:
		Socket(HANDLE hPipe) : OOBase::Win32::Socket(hPipe)
		{}

		void shutdown(bool, bool)
		{
			m_handle.close();
		}
	};
}

OOBase::Win32::Socket::Socket(HANDLE handle) :
		m_handle(handle)
{
}

OOBase::Win32::Socket::~Socket()
{
}

int OOBase::Win32::Socket::send(const void* buf, size_t len, const OOBase::timeval_t* timeout)
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

size_t OOBase::Win32::Socket::recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout)
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

OOBase::Socket* OOBase::Socket::connect_local(const char* path, int* perr, const timeval_t* wait)
{
	assert(perr);
	*perr = 0;

	OOBase::string pipe_name;
	try
	{
		pipe_name.assign("\\\\.\\pipe\\");
		pipe_name += path;
	}
	catch (std::exception&)
	{
		*perr = ERROR_OUTOFMEMORY;
		return NULL;
	}

	timeval_t wait2 = (wait ? *wait : timeval_t::MaxTime);
	Countdown countdown(&wait2);

	Win32::SmartHandle hPipe;
	while (wait2 != timeval_t::Zero)
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
			if (*perr == ERROR_SEM_TIMEOUT)
				*perr = WSAETIMEDOUT;

			return 0;
		}
	}

	::Socket* pSocket;
	OOBASE_NEW_T(::Socket,pSocket,::Socket(hPipe));
	if (!pSocket)
	{
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}

	hPipe.detach();

	return pSocket;
}

#endif // _WIN32
