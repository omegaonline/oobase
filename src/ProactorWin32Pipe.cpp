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

#include "../include/OOBase/Allocator.h"

#if defined(_WIN32)

#if !defined(STATUS_PIPE_BROKEN)
#define STATUS_PIPE_BROKEN 0xC000014BL
#endif

namespace
{
	class PipeAcceptor : public OOBase::Socket
	{
	public:
		PipeAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, const OOBase::string& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, const char* strAddress, int err));

		size_t send(const void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t send_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t recv(void* buf, size_t len, bool bAll, int* perr, const OOBase::timeval_t* timeout = 0);
		size_t recv_v(OOBase::Buffer* buffers[], size_t count, int* perr, const OOBase::timeval_t* timeout = 0);
		void shutdown(bool bSend, bool bRecv);

		int start();
	};

	class AsyncPipe : public OOSvrBase::AsyncLocalSocket
	{
	public:
		static OOSvrBase::AsyncLocalSocket* Create(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE hPipe, int* perr);

		int recv(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, bool bAll);
		int send(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int recv_v(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);
		int send_v(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		void shutdown(bool bSend, bool bRecv);

		int get_uid(uid_t& uid);
	};
}

size_t PipeAcceptor::send(const void*, size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t PipeAcceptor::recv(void*, size_t, bool, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t PipeAcceptor::send_v(OOBase::Buffer*[], size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

size_t PipeAcceptor::recv_v(OOBase::Buffer*[], size_t, int* perr, const OOBase::timeval_t*)
{
	*perr = WSAEOPNOTSUPP;
	return 0;
}

OOBase::SmartPtr<OOBase::Socket> OOSvrBase::Win32::ProactorImpl::accept_local(void* param, void (*callback)(void* param, AsyncLocalSocket* pSocket, const char* strAddress, int err), const char* path, int* perr, SECURITY_ATTRIBUTES* psa)
{
	OOBase::SmartPtr<PipeAcceptor> pAcceptor;
	OOBASE_NEW_T(PipeAcceptor,pAcceptor,PipeAcceptor(this,OOBase::string("\\\\.\\pipe\\") + path,psa,param,callback));
	if (!pAcceptor)
	{
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}
		
	*perr = pAcceptor->start();
	if (*perr != 0)
		return 0;

	return pAcceptor.detach();
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::attach_local_socket(OOBase::socket_t sock, int* perr)
{
	return AsyncPipe::Create(this,(HANDLE)sock,perr);
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::connect_local_socket(const char* path, int* perr, const OOBase::timeval_t* timeout)
{
	assert(perr);
	*perr = 0;

	OOBase::local_string pipe_name("\\\\.\\pipe\\");
	pipe_name += path;

	OOBase::timeval_t timeout2 = (timeout ? *timeout : OOBase::timeval_t::MaxTime);
	OOBase::Countdown countdown(&timeout2);

	OOBase::Win32::SmartHandle hPipe;
	while (timeout2 != OOBase::timeval_t::Zero)
	{
		hPipe = ::CreateFileA(pipe_name.c_str(),
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
		if (timeout)
		{
			countdown.update();

			dwWait = timeout2.msec();
		}

		if (!::WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			*perr = GetLastError();
			return 0;
		}
	}

	// Wrap socket
	OOSvrBase::AsyncLocalSocket* pSocket = AsyncPipe::Create(this,hPipe,perr);
	if (pSocket)
		hPipe.detach();
	else
		hPipe.close();
	
	return pSocket;
}

#endif // _WIN32
