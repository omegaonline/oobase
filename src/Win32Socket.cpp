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

#include "../include/OOBase/GlobalNew.h"

#if defined(_WIN32)

#include "../include/OOBase/Singleton.h"
#include "Win32Socket.h"

#include <mswsock.h>

#if !defined(MSG_WAITALL)
#define MSG_WAITALL 8
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
    sockaddr **LocalSockaddr,
    LPINT LocalSockaddrLength,
    sockaddr **RemoteSockaddr,
    LPINT RemoteSockaddrLength
    );

#define WSAID_GETACCEPTEXSOCKADDRS {0xb5367df2,0xcbac,0x11cf,{0x95,0xca,0x00,0x80,0x5f,0x48,0xa1,0x92}}

#endif // !defined(WSAID_GETACCEPTEXSOCKADDRS)

#if !defined(WSAID_CONNECTEX)

typedef BOOL (PASCAL * LPFN_CONNECTEX)(
    SOCKET s,
    const sockaddr *name,
    int namelen,
    PVOID lpSendBuffer,
    DWORD dwSendDataLength,
    LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped
    );

#define WSAID_CONNECTEX {0x25a207b9,0xddf3,0x4660,{0x8e,0xe9,0x76,0xe5,0x8c,0x74,0x06,0x3e}}

#if !defined(SO_UPDATE_CONNECT_CONTEXT)
#define SO_UPDATE_CONNECT_CONTEXT   0x7010
#endif

#endif // !defined(WSAID_CONNECTEX)

namespace
{
	class WSAStarter
	{
	public:
		WSAStarter()
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
		}	
		
		~WSAStarter()
		{
			WSACleanup();
		}
	};
}

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

void OOBase::Win32::WSAStartup()
{
	// Use a singleton to ensure WSAStartup() and WSACleanup() are called as a pair, once
	Singleton<WSAStarter,Module>::instance();
}

BOOL OOBase::Win32::WSAAcceptEx(SOCKET sListenSocket, SOCKET sAcceptSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, LPDWORD lpdwBytesReceived, LPOVERLAPPED lpOverlapped)
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

	LPFN_ACCEPTEX lpfnAcceptEx = NULL;
	DWORD dwBytes = 0;
	if (WSAIoctl(sListenSocket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_AcceptEx, 
		sizeof(guid_AcceptEx),
		&lpfnAcceptEx, 
		sizeof(lpfnAcceptEx), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		OOBase_CallCriticalFailure("Failed to load address of AcceptEx");
	}

	return (*lpfnAcceptEx)(sListenSocket,sAcceptSocket,lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,lpdwBytesReceived,lpOverlapped);
}

void OOBase::Win32::WSAGetAcceptExSockAddrs(SOCKET sListenSocket, void* lpOutputBuffer, DWORD dwReceiveDataLength, DWORD dwLocalAddressLength, DWORD dwRemoteAddressLength, sockaddr **LocalSockaddr, int* LocalSockaddrLength, sockaddr **RemoteSockaddr, int* RemoteSockaddrLength)
{
	static const GUID guid_GetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs = NULL;
	DWORD dwBytes = 0;
	if (WSAIoctl(sListenSocket, 
		SIO_GET_EXTENSION_FUNCTION_POINTER, 
		(void*)&guid_GetAcceptExSockAddrs, 
		sizeof(guid_GetAcceptExSockAddrs),
		&lpfnGetAcceptExSockAddrs, 
		sizeof(lpfnGetAcceptExSockAddrs), 
		&dwBytes, 
		NULL, 
		NULL) != 0)
	{
		OOBase_CallCriticalFailure("Failed to load address of GetAcceptExSockAddrs");
	}

	return (*lpfnGetAcceptExSockAddrs)(lpOutputBuffer,dwReceiveDataLength,dwLocalAddressLength,dwRemoteAddressLength,LocalSockaddr,LocalSockaddrLength,RemoteSockaddr,RemoteSockaddrLength);
}

OOBase::socket_t OOBase::Net::open_socket(int family, int socktype, int protocol, int& err)
{
	Win32::WSAStartup();

	err = 0;

	SOCKET sock = WSASocket(family,socktype,protocol,NULL,0,WSA_FLAG_OVERLAPPED);
	if (sock == INVALID_SOCKET)
		err = WSAGetLastError();

	return sock;
}

int OOBase::Net::close_socket(socket_t sock)
{
	return (::closesocket(sock) == 0 ? 0 : WSAGetLastError());
}

int OOBase::Net::connect(socket_t sock, const sockaddr* addr, socklen_t addrlen, const Timeout& timeout)
{
	static const GUID guid_ConnectEx = WSAID_CONNECTEX;

	LPFN_CONNECTEX lpfnConnectEx = NULL;

	Win32::WSAStartup();

	if (!timeout.is_infinite())
	{
		DWORD dwBytes = 0;
		WSAIoctl(sock, 
			SIO_GET_EXTENSION_FUNCTION_POINTER, 
			(void*)&guid_ConnectEx, 
			sizeof(guid_ConnectEx),
			&lpfnConnectEx, 
			sizeof(lpfnConnectEx), 
			&dwBytes, 
			NULL, 
			NULL);
	}

	if (lpfnConnectEx)
	{
		WSAOVERLAPPED ov = {0};
		ov.hEvent = CreateEventW(NULL,TRUE,FALSE,NULL);
		if (!ov.hEvent)
			return GetLastError();
		
		if (!(*lpfnConnectEx)(sock,addr,addrlen,NULL,0,NULL,&ov))
		{
			DWORD dwErr = WSAGetLastError();
			if (dwErr != ERROR_IO_PENDING)
				return dwErr;

			DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
			if (dwWait == WAIT_TIMEOUT)
			{
				CancelIo((HANDLE)sock);
				return WSAETIMEDOUT;
			}
			else if (dwWait != WAIT_OBJECT_0)
			{
				dwErr = GetLastError();
				CancelIo((HANDLE)sock);
				return dwErr;
			}
		}

		setsockopt(sock,SOL_SOCKET,SO_UPDATE_CONNECT_CONTEXT,NULL,0);

		return 0;
	}
	else
	{
		// Do the connect
		if (::connect(sock,addr,static_cast<int>(addrlen)) != SOCKET_ERROR)
			return 0;
		else
			return WSAGetLastError();
	}
}

/*int OOBase::Net::accept(socket_t accept_sock, socket_t& new_sock, const Timeout& timeout)
{
	return ERROR_NOT_FOUND;
}*/

int OOBase::Net::bind(socket_t sock, const sockaddr* addr, socklen_t addr_len)
{
	return (::bind(sock,addr,addr_len) != 0 ? WSAGetLastError() : 0);
}

namespace
{
	class WinSocket : public OOBase::Socket
	{
	public:
		WinSocket(SOCKET sock);
		virtual ~WinSocket();

		size_t recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout);
		int recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);
		
		size_t send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout);
		int send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout);

		int recv_socket(OOBase::socket_t& /*sock*/, const OOBase::Timeout& /*timeout*/)
		{
			return ERROR_NOT_SUPPORTED;
		}

		int send_socket(OOBase::socket_t /*sock*/, DWORD /*pid*/, const OOBase::Timeout& /*timeout*/)
		{
			return ERROR_NOT_SUPPORTED;
		}

		void close();
					
	private:
		SOCKET                     m_socket;
		OOBase::Mutex              m_recv_lock;  // These are mutexes to enforce ordering
		OOBase::Mutex              m_send_lock;
		OOBase::Win32::SmartHandle m_recv_event;
		OOBase::Win32::SmartHandle m_send_event;

		DWORD send_i(WSABUF* wsabuf, DWORD count, int& err, const OOBase::Timeout& timeout);
		DWORD recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int& err, const OOBase::Timeout& timeout);
	};

	SOCKET connect_i(const char* address, const char* port, int& err, const OOBase::Timeout& timeout)
	{
		// Resolve the passed in addresses...
		addrinfo hints = {0};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		addrinfo* pResults = NULL;
		if (getaddrinfo(address,port,&hints,&pResults) != 0)
		{
			err = WSAGetLastError();
			return INVALID_SOCKET;
		}
		
		SOCKET sock = INVALID_SOCKET;

		// Took too long to resolve...
		if (timeout.has_expired())
			err = WSAETIMEDOUT;
		else
		{
			// Loop trying to connect on each address until one succeeds
			for (addrinfo* pAddr = pResults; pAddr != NULL; pAddr = pAddr->ai_next)
			{
				// Clear error
				sock = OOBase::Net::open_socket(pAddr->ai_family,pAddr->ai_socktype,pAddr->ai_protocol,err);
				if (err)
					break;

				if ((err = OOBase::Net::connect(sock,pAddr->ai_addr,static_cast<socklen_t>(pAddr->ai_addrlen),timeout)) != 0)
					OOBase::Net::close_socket(sock);
				else
					break;

				if (timeout.has_expired())
				{
					err = WSAETIMEDOUT;
					break;
				}
			}
		}

		// Done with address info
		freeaddrinfo(pResults);

		return sock;
	}
}

WinSocket::WinSocket(SOCKET sock) :
		m_socket(sock)
{
	OOBase::Win32::WSAStartup();
}

WinSocket::~WinSocket()
{
	OOBase::Net::close_socket(m_socket);
}

size_t WinSocket::send(const void* buf, size_t len, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (len > 0xFFFFFFFF)
	{
		err = ERROR_BUFFER_OVERFLOW;
		return 0;
	}

	WSABUF wsabuf;
	wsabuf.buf = const_cast<char*>(static_cast<const char*>(buf));
	wsabuf.len = static_cast<ULONG>(len);

	return send_i(&wsabuf,1,err,timeout);
}

int WinSocket::send_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	size_t total_len = 0;
	DWORD actual_count = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();

		total_len += len;
		if (total_len > 0xFFFFFFFF)
			return ERROR_BUFFER_OVERFLOW;

		if (len > 0)
		{
			if (actual_count == 0xFFFFFFFF)
				return ERROR_BUFFER_OVERFLOW;

			++actual_count;
		}
	}

	OOBase::StackArrayPtr<WSABUF,8> wsa_bufs(actual_count);
	if (!wsa_bufs)
		return ERROR_OUTOFMEMORY;

	DWORD j = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (len > 0)
		{
			wsa_bufs[j].len = static_cast<ULONG>(len);
			wsa_bufs[j].buf = const_cast<char*>(static_cast<const char*>(buffers[i]->rd_ptr()));
			++j;
		}
	}

	int err = 0;
	DWORD dwWritten = send_i(wsa_bufs,actual_count,err,timeout);

	// Update buffers...
	for (size_t first_buffer = 0;dwWritten;)
	{
		if (dwWritten >= wsa_bufs->len)
		{
			buffers[first_buffer]->rd_ptr(wsa_bufs->len);
			++first_buffer;
			dwWritten -= wsa_bufs->len;
			++wsa_bufs;
			if (--actual_count == 0)
				break;
		}
		else
		{
			buffers[first_buffer]->rd_ptr(dwWritten);
			wsa_bufs->len -= dwWritten;
			wsa_bufs->buf += dwWritten;
			dwWritten = 0;
		}
	}

	return err;
}

DWORD WinSocket::send_i(WSABUF* wsabuf, DWORD count, int& err, const OOBase::Timeout& timeout)
{
	WSAOVERLAPPED ov = {0};
	OOBase::Guard<OOBase::Mutex> guard(m_send_lock,false);

	if (!timeout.is_infinite())
	{
		if (!guard.acquire(timeout))
		{
			err = WSAETIMEDOUT;
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

		ov.hEvent = m_send_event;
	}

	DWORD dwWritten = 0;
	if (timeout.is_infinite())
	{
		if (WSASend(m_socket,wsabuf,static_cast<DWORD>(count),&dwWritten,0,NULL,NULL) != 0)
			err = WSAGetLastError();
	}
	else
	{
		if (WSASend(m_socket,wsabuf,static_cast<DWORD>(count),&dwWritten,0,&ov,NULL) != 0)
		{
			err = WSAGetLastError();
			if (err == WSA_IO_PENDING)
			{
				err = 0;
				DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
				if (dwWait == WAIT_TIMEOUT)
				{
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
					err = WAIT_TIMEOUT;
				}
				else if (dwWait != WAIT_OBJECT_0)
				{
					err = GetLastError();
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
				}

				DWORD dwFlags = 0;
				if (!WSAGetOverlappedResult(m_socket,&ov,&dwWritten,TRUE,&dwFlags))
				{
					int dwErr = WSAGetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;
				}
			}
		}
	}

	return dwWritten;
}

size_t WinSocket::recv(void* buf, size_t len, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	err = 0;
	if (len == 0)
		return 0;

	if (len > 0xFFFFFFFF)
	{
		err = ERROR_BUFFER_OVERFLOW;
		return 0;
	}

	WSABUF wsabuf;
	wsabuf.buf = static_cast<char*>(buf);
	wsabuf.len = static_cast<ULONG>(len);

	return recv_i(&wsabuf,1,bAll,err,timeout);
}

int WinSocket::recv_v(OOBase::Buffer* buffers[], size_t count, const OOBase::Timeout& timeout)
{
	if (count == 0)
		return 0;

	size_t total_len = 0;
	DWORD actual_count = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->space();
		if (len > 0)
		{
			if (len > 0xFFFFFFFF || total_len > 0xFFFFFFFF - len)
				return ERROR_BUFFER_OVERFLOW;

			total_len += len;

			if (actual_count == 0xFFFFFFFF)
				return ERROR_BUFFER_OVERFLOW;

			++actual_count;
		}
	}

	OOBase::StackArrayPtr<WSABUF,8> wsa_bufs(actual_count);
	if (!wsa_bufs)
		return ERROR_OUTOFMEMORY;

	DWORD j = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->space();
		if (len > 0)
		{
			wsa_bufs[j].len = static_cast<ULONG>(len);
			wsa_bufs[j].buf = const_cast<char*>(buffers[i]->wr_ptr());
			++j;
		}
	}

	int err = 0;
	DWORD dwRead = recv_i(wsa_bufs,actual_count,true,err,timeout);

	// Update buffers...
	for (size_t first_buffer = 0;dwRead;)
	{
		if (dwRead >= wsa_bufs->len)
		{
			buffers[first_buffer]->wr_ptr(wsa_bufs->len);
			++first_buffer;
			dwRead -= wsa_bufs->len;
			++wsa_bufs;
			if (--actual_count == 0)
				break;
		}
		else
		{
			buffers[first_buffer]->wr_ptr(dwRead);
			wsa_bufs->len -= dwRead;
			wsa_bufs->buf += dwRead;
			dwRead = 0;
		}
	}

	return err;
}

DWORD WinSocket::recv_i(WSABUF* wsabuf, DWORD count, bool bAll, int& err, const OOBase::Timeout& timeout)
{
	WSAOVERLAPPED ov = {0};

	OOBase::Guard<OOBase::Mutex> guard(m_recv_lock,false);

	if (!timeout.is_infinite())
	{
		if (!guard.acquire(timeout))
		{
			err = WSAETIMEDOUT;
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

		ov.hEvent = m_recv_event;
	}

	DWORD dwFlags = (bAll ? MSG_WAITALL : 0);
	DWORD dwRead = 0;
	if (timeout.is_infinite())
	{
		if (WSARecv(m_socket,wsabuf,static_cast<DWORD>(count),&dwRead,&dwFlags,NULL,NULL) != 0)
			err = WSAGetLastError();
	}
	else
	{
		if (WSARecv(m_socket,wsabuf,static_cast<DWORD>(count),&dwRead,&dwFlags,&ov,NULL) != 0)
		{
			err = WSAGetLastError();
			if (err == WSA_IO_PENDING)
			{
				err = 0;
				DWORD dwWait = WaitForSingleObject(ov.hEvent,timeout.millisecs());
				if (dwWait == WAIT_TIMEOUT)
				{
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
					err = WAIT_TIMEOUT;
				}
				else if (dwWait != WAIT_OBJECT_0)
				{
					err = GetLastError();
					CancelIo(reinterpret_cast<HANDLE>(m_socket));
				}

				if (!WSAGetOverlappedResult(m_socket,&ov,&dwRead,TRUE,&dwFlags))
				{
					int dwErr = WSAGetLastError();
					if (err == 0 && dwErr != ERROR_OPERATION_ABORTED)
						err = dwErr;
				}
			}
		}
	}

	return dwRead;
}

void WinSocket::close()
{
	// This may not cancel any outstanding recvs
	void* CHECK_ME;

	::shutdown(m_socket,SD_SEND);
}

OOBase::Socket* OOBase::Socket::attach(socket_t sock, int& err)
{
	OOBase::Socket* pSocket = new (std::nothrow) WinSocket(sock);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;

	return pSocket;
}

OOBase::Socket* OOBase::Socket::connect(const char* address, const char* port, int& err, const Timeout& timeout)
{
	// Ensure we have winsock loaded
	Win32::WSAStartup();

	SOCKET sock = connect_i(address,port,err,timeout);
	if (sock == INVALID_SOCKET)
		return NULL;

	OOBase::Socket* pSocket = new (std::nothrow) WinSocket(sock);
	if (!pSocket)
	{
		err = ERROR_OUTOFMEMORY;
		OOBase::Net::close_socket(sock);
	}

	return pSocket;
}

#endif // _WIN32
