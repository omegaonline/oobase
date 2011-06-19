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

#include "../include/OOBase/CustomNew.h"
#include "Win32Socket.h"

#if defined(_WIN32)

#include <Ws2tcpip.h>
#include <mswsock.h>

#if defined(max)
#undef max
#endif

#if !defined(WSAID_ACCEPTEX)

typedef BOOL (PASCAL* LPFN_ACCEPTEX)(
    SOCKET sListenSocket,
    SOCKET sAcceptSocket,
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    LPDWORD lpdwBytesReceived,
    LPOVERLAPPED lpOverlapped);

#define WSAID_ACCEPTEX {0xb5367df1,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}

#endif // !defined(WSAID_ACCEPTEX)

#if !defined(WSAID_GETACCEPTEXSOCKADDRS)

typedef VOID (PASCAL* LPFN_GETACCEPTEXSOCKADDRS)(
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    struct sockaddr **LocalSockaddr,
    LPINT LocalSockaddrLength,
    struct sockaddr **RemoteSockaddr,
    LPINT RemoteSockaddrLength
    );

#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}

#endif // !defined(WSAID_GETACCEPTEXSOCKADDRS)

namespace
{
	class WSA
	{
	public:
		WSA();
		~WSA();

		LPFN_ACCEPTEX             m_lpfnAcceptEx;
		LPFN_GETACCEPTEXSOCKADDRS m_lpfnGetAcceptExSockAddrs;
	};
}

WSA::WSA()
{
	// Start Winsock
	WSADATA wsa;
	int err = WSAStartup(MAKEWORD(2,2),&wsa);
	if (err != 0)
		OOBase_CallCriticalFailure(WSAGetLastError());
	
	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 2)
	{
		WSACleanup();
		OOBase_CallCriticalFailure("Very old Winsock dll");
	}

	//----------------------------------------
	// Load the AcceptEx function into memory using WSAIoctl.
	// The WSAIoctl function is an extension of the ioctlsocket()
	// function that can use overlapped I/O. The function's 3rd
	// through 6th parameters are input and output buffers where
	// we pass the pointer to our AcceptEx function. This is used
	// so that we can call the AcceptEx function directly, rather
	// than refer to the Mswsock.lib library.
	const GUID guid_AcceptEx = WSAID_ACCEPTEX;
	const GUID guid_GetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	// We need a socket
	SOCKET sock = socket(AF_INET,SOCK_STREAM,PF_INET);
	if (sock == INVALID_SOCKET)
	{
		WSACleanup();
		OOBase_CallCriticalFailure("Failed create socket");
	}

	DWORD dwBytes;
	if (WSAIoctl(sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_AcceptEx, 
		sizeof(guid_AcceptEx),
		&m_lpfnAcceptEx, 
		sizeof(m_lpfnAcceptEx), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		closesocket(sock);
		WSACleanup();
		OOBase_CallCriticalFailure("Failed to load address of AcceptEx");
	}

	if (WSAIoctl(sock, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_GetAcceptExSockAddrs, 
		sizeof(guid_GetAcceptExSockAddrs),
		&m_lpfnGetAcceptExSockAddrs, 
		sizeof(m_lpfnGetAcceptExSockAddrs), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		closesocket(sock);
		WSACleanup();
		OOBase_CallCriticalFailure("Failed to load address of GetAcceptExSockAddrs");
	}

	closesocket(sock);
}

WSA::~WSA()
{
	WSACleanup();
}

void OOBase::Win32::WSAStartup()
{
	OOBase::Singleton<WSA>::instance();
}

BOOL OOBase::Win32::WSAAcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
{
	return (*OOBase::Singleton<WSA>::instance().m_lpfnAcceptEx)(sListenSocket,sAcceptSocket,lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,lpdwBytesReceived,lpOverlapped);
}

void OOBase::Win32::WSAGetAcceptExSockAddrs(void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, struct sockaddr **LocalSockaddr, int* LocalSockaddrLength, struct sockaddr **RemoteSockaddr, int* RemoteSockaddrLength)
{
	return (*OOBase::Singleton<WSA>::instance().m_lpfnGetAcceptExSockAddrs)(lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,LocalSockaddr,LocalSockaddrLength,RemoteSockaddr,RemoteSockaddrLength);
}

namespace
{
	class WinSocket : public OOBase::Socket
	{
	public:
		WinSocket(SOCKET sock);
		virtual ~WinSocket();

		size_t recv(void* buf, size_t len, bool bAll, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t recv_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);
		
		size_t send(const void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t send_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);

		void shutdown(bool bSend, bool bRecv);
					
	private:
		SOCKET                     m_socket;
		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;
		OOBase::Win32::SmartHandle m_recv_event;
		OOBase::Win32::SmartHandle m_send_event;

		size_t send_i(WSABUF* wsabuf, DWORD count, int* perr, const OOBase::timeval_t* timeout);
		size_t recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int* perr, const OOBase::timeval_t* timeout);
	};
}

WinSocket::WinSocket(SOCKET sock) :
		m_socket(sock)
{
	OOBase::Win32::WSAStartup();
}

WinSocket::~WinSocket()
{
	closesocket(m_socket);
}

size_t WinSocket::send(const void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout)
{
	if (len > 0xFFFFFFFF)
	{
		*perr = E2BIG;
		return 0;
	}
	else
	{
		WSABUF wsabuf;
		wsabuf.buf = const_cast<char*>(static_cast<const char*>(buf));
		wsabuf.len = len;

		return send_i(&wsabuf,1,perr,timeout);
	}
}

size_t WinSocket::send_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	WSABUF static_bufs[4];
	WSABUF* bufs = static_bufs;
	
	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		bufs = static_cast<WSABUF*>(OOBase::Allocate(count * sizeof(WSABUF),1,__FILE__,__LINE__));
		if (!bufs)
		{
			*perr = ERROR_OUTOFMEMORY;
			return 0;
		}
	}

	*perr = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (len > 0xFFFFFFFF)
		{
			*perr = E2BIG;
			break;
		}
		
		bufs[i].len = static_cast<ULONG>(len);
		bufs[i].buf = const_cast<char*>(buffers[i]->rd_ptr());
	}

	size_t total = 0;
	if (*perr == 0)
	{
		size_t len = send_i(bufs,count,perr,timeout);

		total = len;
		
		for (size_t i=0;i<count && len > 0;++i)
		{
			size_t l = buffers[i]->length();
			if (len >= l)
			{
				buffers[i]->rd_ptr(l);
				len -= l;
			}
			else
			{
				buffers[i]->rd_ptr(len);
				len = 0;
			}
		}
	}

	if (bufs != static_bufs)
		OOBase::Free(bufs,1);

	return total;
}

size_t WinSocket::send_i(WSABUF* wsabuf, DWORD count, int* perr, const OOBase::timeval_t* timeout)
{
	OOBase::Guard<OOBase::Mutex> guard(m_send_lock);

	WSAOVERLAPPED ov = {0};
	if (timeout)
	{
		if (!m_send_event.is_valid())
		{
			m_send_event = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_send_event.is_valid())
			{
				*perr = GetLastError();
				return 0;
			}
		}
		ov.hEvent = m_send_event;
	}

	size_t total = 0;
	while (count > 0)
	{
		DWORD dwWritten = 0;
		if (!timeout)
		{
			if (WSASend(m_socket,wsabuf,count,&dwWritten,0,NULL,NULL) != 0)
			{
				*perr = WSAGetLastError();
				if (*perr != ERROR_OPERATION_ABORTED)
					return total;
			}
		}
		else
		{
			if (WSASend(m_socket,wsabuf,count,&dwWritten,0,&ov,NULL) != 0)
			{
				*perr = WSAGetLastError();
				if (*perr != ERROR_OPERATION_ABORTED)
				{
					if (*perr != WSA_IO_PENDING)
						return total;

					*perr = 0;
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(reinterpret_cast<HANDLE>(m_socket));
						*perr = WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						*perr = GetLastError();
						CancelIo(reinterpret_cast<HANDLE>(m_socket));
					}
				
					if (!WSAGetOverlappedResult(m_socket,&ov,&dwWritten,TRUE,NULL))
					{
						int dwErr = WSAGetLastError();
						if (*perr == 0 && dwErr != ERROR_OPERATION_ABORTED)
							*perr = dwErr;

						if (*perr != 0)
							return total;
					}
				}
			}
		}

		total += dwWritten;

		// Update buffers
		do
		{
			if (wsabuf->len > dwWritten)
			{
				wsabuf->len -= dwWritten;
				wsabuf->buf += dwWritten;
				break;
			}

			dwWritten -= wsabuf->len;
			++wsabuf;
			--count;
		} 
		while (count > 0);
	}

	*perr = 0;
	return total;
}

size_t WinSocket::recv(void* buf, size_t len, bool bAll, int* perr, const OOBase::timeval_t* timeout)
{
	if (len > 0xFFFFFFFF)
	{
		*perr = E2BIG;
		return 0;
	}
	else
	{
		WSABUF wsabuf;
		wsabuf.buf = const_cast<char*>(static_cast<const char*>(buf));
		wsabuf.len = len;

	std::string pipe_name;
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

size_t WinSocket::recv_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout)
{
	if (count == 0)
		return 0;

	WSABUF static_bufs[4];
	WSABUF* bufs = static_bufs;
	
	if (count > sizeof(static_bufs)/sizeof(static_bufs[0]))
	{
		bufs = static_cast<WSABUF*>(OOBase::Allocate(count * sizeof(WSABUF),1,__FILE__,__LINE__));
		if (!bufs)
		{
			*perr = ERROR_OUTOFMEMORY;
			return 0;
		}
	}

	*perr = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->space();
		if (len > 0xFFFFFFFF)
		{
			*perr = E2BIG;
			break;
		}
		
		bufs[i].len = static_cast<ULONG>(len);
		bufs[i].buf = const_cast<char*>(buffers[i]->wr_ptr());
	}

	size_t total = 0;
	if (*perr == 0)
	{
		size_t len = recv_i(bufs,count,true,perr,timeout);

		total = len;
		
		for (size_t i=0;i<count && len > 0;++i)
		{
			size_t l = buffers[i]->space();
			if (len >= l)
			{
				buffers[i]->wr_ptr(l);
				len -= l;
			}
			else
			{
				buffers[i]->wr_ptr(len);
				len = 0;
			}
		}
	}

	if (bufs != static_bufs)
		OOBase::Free(bufs,1);

	return total;
}

size_t WinSocket::recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int* perr, const OOBase::timeval_t* timeout)
{
	assert(perr);
	*perr = 0;

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock);

	WSAOVERLAPPED ov = {0};
	if (timeout)
	{
		if (!m_recv_event.is_valid())
		{
			m_recv_event = CreateEventW(NULL,TRUE,FALSE,NULL);
			if (!m_recv_event.is_valid())
			{
				*perr = GetLastError();
				return 0;
			}
		}
		ov.hEvent = m_recv_event;
	}
	
	size_t total = 0;
	while (count > 0)
	{
		DWORD dwRead = 0;
		if (!timeout)
		{
			if (WSARecv(m_socket,wsabuf,count,&dwRead,NULL,NULL,NULL) != 0)
			{
				*perr = WSAGetLastError();
				if (*perr != ERROR_OPERATION_ABORTED)
					return total;
			}
		}
		else
		{
			if (WSARecv(m_socket,wsabuf,count,&dwRead,NULL,&ov,NULL) != 0)
			{
				*perr = WSAGetLastError();
				if (*perr != ERROR_OPERATION_ABORTED)
				{
					if (*perr != WSA_IO_PENDING)
						return total;

					*perr = 0;
					DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout->msec());
					if (dwWait == WAIT_TIMEOUT)
					{
						CancelIo(reinterpret_cast<HANDLE>(m_socket));
						*perr = WAIT_TIMEOUT;
					}
					else if (dwWait != WAIT_OBJECT_0)
					{
						*perr = GetLastError();
						CancelIo(reinterpret_cast<HANDLE>(m_socket));
					}
					
					if (!WSAGetOverlappedResult(m_socket,&ov,&dwRead,TRUE,NULL))
					{
						int dwErr = WSAGetLastError();
						if (*perr == 0 && dwErr != ERROR_OPERATION_ABORTED)
							*perr = dwErr;

						if (*perr != 0)
							return total;
					}
				}
			}
		}

		total += dwRead;

		// Update buffers
		do
		{
			if (wsabuf->len > dwRead)
			{
				wsabuf->len -= dwRead;
				wsabuf->buf += dwRead;
				break;
			}

			dwRead -= wsabuf->len;
			++wsabuf;
			--count;
		} 
		while (count > 0);

		if (!bAll && total > 0)
			break;
	}

	*perr = 0;
	return total;
}

void WinSocket::shutdown(bool bSend, bool bRecv)
{
	int how = 0;
	if (bRecv && bSend)
		how = SD_BOTH;
	else if (bRecv)
		how = SD_RECEIVE;
	else if (bSend)
		how = SD_SEND;

	if (bSend || bRecv)
		::shutdown(m_socket,how);
}

#if 0
OOBase::SmartPtr<OOBase::Socket> OOBase::Socket::connect(const char* address, const char* port, int* perr, const timeval_t* timeout)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	SOCKET create_socket(int family, int socktype, int protocol, int* perr)
	{
		*perr = 0;
		SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
		if (sock == INVALID_SOCKET)
			*perr = WSAGetLastError();

		return sock;
	}

	OOBase::socket_t sock = OOBase::BSD::connect(address,port,perr,wait);
	if (sock == INVALID_SOCKET)
		return 0;

	OOBase::SmartPtr<OOBase::Socket> ptrSocket;
	OOBASE_NEW_T(WinSocket,ptrSocket,WinSocket(sock));
	if (!ptrSocket)
	{
		*perr = ERROR_OUTOFMEMORY;
		closesocket(sock);
		return 0;
	}

	return ptrSocket;
}
#endif

#endif // _WIN32
