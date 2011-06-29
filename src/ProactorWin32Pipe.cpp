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

#include <vector>
#include <algorithm>
#include <functional>

namespace
{
	OOBase::string MakePipeName(const char* name)
	{
		OOBase::string ret("\\\\.\\pipe\\");
		if (strlen(name) > MAX_PATH - 10)
			ret = "\\\\?\\" + ret;

		ret += name;
		return ret;
	}

	class AsyncPipe : public OOSvrBase::AsyncLocalSocket
	{
	public:
		AsyncPipe(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE hPipe);
		virtual ~AsyncPipe();

		int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		void shutdown(bool bSend, bool bRecv);

		int get_uid(uid_t& uid);
	
	private:
		OOSvrBase::Win32::ProactorImpl* m_pProactor;
		OOBase::Win32::SmartHandle      m_hPipe;
		bool                            m_bOpenSend;
		bool                            m_bOpenRecv;
			
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);

		static void translate_error(int& dwErr);
		static void translate_error(DWORD& dwErr);
	};
}

AsyncPipe::AsyncPipe(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE hPipe) :
		m_pProactor(pProactor),
		m_hPipe(hPipe),
		m_bOpenSend(true),
		m_bOpenRecv(true)
{
}

AsyncPipe::~AsyncPipe()
{
}

void AsyncPipe::translate_error(int& dwErr)
{
	DWORD e = dwErr;
	translate_error(e);
	dwErr = e;
}

void AsyncPipe::translate_error(DWORD& dwErr)
{
	switch (dwErr)
	{
	case ERROR_BROKEN_PIPE:
	case ERROR_NO_DATA:
		dwErr = WSAECONNRESET;
		break;
	
	case ERROR_BAD_PIPE:
		dwErr = WSAEBADF;
		break;
		
	case ERROR_PIPE_NOT_CONNECTED:
		dwErr = WSAENOTCONN;
		break;
			
	default:
		break;
	}
}

int AsyncPipe::recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
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
	
	DWORD dwRead = 0;
	if (!m_bOpenRecv)
		dwErr = WSAESHUTDOWN;
	else if (!ReadFile(m_hPipe,buffer->wr_ptr(),static_cast<DWORD>(total),&dwRead,pOv))
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
		
		// Translate error codes...
		translate_error(dwErr);
	}
	
	assert(dwErr || total == dwRead);
	
	on_recv(m_hPipe,dwRead,dwErr,pOv);
		
	return 0;
}

void AsyncPipe::on_recv(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]);
	
	pProactor->delete_overlapped(pOv);
	
	// Translate error codes...
	translate_error(dwErr);
		
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

int AsyncPipe::send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
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
	
	DWORD dwSent = 0;
	if (!m_bOpenSend)
		dwErr = WSAESHUTDOWN;
	else if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(total),&dwSent,pOv))
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
		
		// Translate error codes...
		translate_error(dwErr);
	}
		
	assert(dwErr || total == dwSent);
	
	on_send(m_hPipe,dwSent,dwErr,pOv);
	
	return 0;
}

void AsyncPipe::on_send(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]);
	
	pProactor->delete_overlapped(pOv);
	
	// Translate error codes...
	translate_error(dwErr);
	
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

int AsyncPipe::send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
{
	// Because scatter-gather just doesn't work on named pipes,
	// we concatenate the buffers and send as one 'atomic' write
	
	assert(count > 0);
	
	// Its more efficient to ask for the total block size up front rather than multiple small reallocs
	size_t total = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (std::numeric_limits<size_t>::max() - len < total)
			return WSAENOBUFS;
		
		total += len;
	}
	
	// I imagine you might want to increase length()?
	assert(total > 0);
	
	OOBase::Buffer* buffer = new (std::nothrow) OOBase::Buffer(total);
	if (!buffer)
		return ERROR_OUTOFMEMORY;
	
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		memcpy(buffer->wr_ptr(),buffers[i]->rd_ptr(),len);
		buffer->wr_ptr(len);
	}
			
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(m_pProactor);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(buffer->addref());
	
	DWORD dwSent = 0;
	if (!m_bOpenSend)
		dwErr = WSAESHUTDOWN;
	else if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(total),&dwSent,pOv))
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
		{
			// Will complete later...
			return 0;
		}
		
		// Translate error codes...
		translate_error(dwErr);
	}
		
	assert(dwErr || total == dwSent);
	
	on_send_v(m_hPipe,dwSent,dwErr,pOv);
	
	return 0;
}

void AsyncPipe::on_send_v(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer[], size_t count, int err);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = reinterpret_cast<OOSvrBase::Win32::ProactorImpl*>(pOv->m_extras[0]);
	void* param = reinterpret_cast<void*>(pOv->m_extras[1]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[2]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[3]);
	
	pProactor->delete_overlapped(pOv);
	
	// Translate error codes...
	translate_error(dwErr);
	
	buffer->rd_ptr(dwBytes);
	
	// Call callback
	if (callback)
	{
		try
		{
			(*callback)(param,&buffer,1,dwErr);
		}
		catch (...)
		{
			assert((0,"Unhandled exception"));
		}
	}

	buffer->release();
}

void AsyncPipe::shutdown(bool bSend, bool bRecv)
{
	if (bSend)
		m_bOpenSend = false;
	
	if (bRecv)
		m_bOpenRecv = false;
}

int AsyncPipe::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
{
	if (!ImpersonateNamedPipeClient(m_hPipe))
		return GetLastError();

	OOBase::Win32::SmartHandle ptruid;
	BOOL bRes = OpenThreadToken(GetCurrentThread(),TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE,FALSE,&ptruid);
	DWORD err = 0;
	if (!bRes)
		err = GetLastError();

	if (!RevertToSelf())
		OOBase_CallCriticalFailure(GetLastError());
	
	if (!bRes)
		return err;

	uid = ptruid.detach();
	
	return 0;
}

namespace
{	
	class PipeAcceptor : public OOSvrBase::Acceptor
	{
	public:
		PipeAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, const OOBase::string& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err));
		virtual ~PipeAcceptor();

		int listen(size_t backlog);
		int stop();
	
	private:
		OOSvrBase::Win32::ProactorImpl*  m_pProactor;
		OOBase::Condition::Mutex         m_lock;
		OOBase::Condition                m_condition;
		OOBase::string                   m_pipe_name;
		SECURITY_ATTRIBUTES*             m_psa;
		size_t                           m_backlog;
		std::vector<HANDLE>              m_vecPending;
		void*                            m_param;
		void (*m_callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err);
		
		static void on_completion(HANDLE hPipe, DWORD dwBytes, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv);
		void on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr);
		int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard);
	};
}

PipeAcceptor::PipeAcceptor(OOSvrBase::Win32::ProactorImpl* pProactor, const OOBase::string& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err)) :
		m_pProactor(pProactor),
		m_pipe_name(pipe_name),
		m_psa(psa),
		m_backlog(0),
		m_param(param),
		m_callback(callback)
{ }

PipeAcceptor::~PipeAcceptor()
{
	stop();
}

int PipeAcceptor::listen(size_t backlog)
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
		
		return do_accept(guard);
	}
}

int PipeAcceptor::stop()
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		// Close all the pending handles
		try
		{
			std::for_each(m_vecPending.begin(),m_vecPending.end(),std::ptr_fun(CloseHandle));
		}
		catch (std::exception& e)
		{
			OOBase_CallCriticalFailure(e.what());
		}
	}
	
	// Wait for all pending operations to complete
	while (!m_vecPending.empty() && m_backlog == 0)
	{
		m_condition.wait(m_lock);
	}

	return 0;
}

int PipeAcceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	DWORD dwErr = 0;
	OOSvrBase::Win32::ProactorImpl::Overlapped* pOv = NULL;
	
	do
	{
		OOBase::Win32::SmartHandle hPipe = CreateNamedPipeA(m_pipe_name.c_str(),
		                                        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
												PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
												PIPE_UNLIMITED_INSTANCES,
												0,0,0,m_psa);

		if (!hPipe.is_valid())
		{
			dwErr = GetLastError();
			break;
		}
		
		dwErr = m_pProactor->bind(hPipe);
		if (dwErr != 0)
			break;
					
		if (!pOv)
		{
			dwErr = m_pProactor->new_overlapped(pOv,&on_completion);
			if (dwErr != 0)
				break;
			
			// Set this pointer
			pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(this);
		}
		
		if (!ConnectNamedPipe(hPipe,pOv))
		{
			dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING)
			{
				// Will complete later...
				try
				{
					m_vecPending.push_back(hPipe);
				}
				catch (std::exception&)
				{
					dwErr = ERROR_OUTOFMEMORY;
				}
				
				if (dwErr == ERROR_IO_PENDING)
				{
					hPipe.detach();
					pOv = NULL;
					dwErr = 0;
					continue;
				}
			}
			
			if (dwErr != ERROR_PIPE_CONNECTED)
				break;
			
			dwErr = 0;
		}
		
		guard.release();
		
		// Call the callback
		on_accept(hPipe.detach(),false,0);
				
		guard.acquire();
	}
	while (m_vecPending.size() < m_backlog);
	
	if (pOv)
		m_pProactor->delete_overlapped(pOv);
	
	return dwErr;
}

void PipeAcceptor::on_completion(HANDLE hPipe, DWORD /*dwBytes*/, DWORD dwErr, OOSvrBase::Win32::ProactorImpl::Overlapped* pOv)
{	
	PipeAcceptor* pThis = reinterpret_cast<PipeAcceptor*>(pOv->m_extras[0]);
	
	OOSvrBase::Win32::ProactorImpl* pProactor = pThis->m_pProactor;
	
	// Perform the callback
	pThis->on_accept(hPipe,true,dwErr);
	
	// Return the overlapped to the proactor
	pProactor->delete_overlapped(pOv);
}

void PipeAcceptor::on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock,false);
	
	bool bSignal = false;
	bool bAgain = false;
	if (bRemove)
	{
		guard.acquire();		
		
		// Remove from pending list
		try
		{
			std::remove(m_vecPending.begin(),m_vecPending.end(),hPipe);
		}
		catch (std::exception& e)
		{
			OOBase_CallCriticalFailure(e.what());
		}		
		
		bAgain = (m_vecPending.size() < m_backlog);
		bSignal = (m_vecPending.empty() && m_backlog == 0);
		
		guard.release();
	}
	
	// Wrap the handle
	OOSvrBase::AsyncLocalSocket* pSocket = NULL;
	if (dwErr == 0)
	{
		pSocket = new (std::nothrow) AsyncPipe(m_pProactor,hPipe);
		if (!pSocket)
		{
			dwErr = ERROR_OUTOFMEMORY;
			CloseHandle(hPipe);
		}
	}
	
	try
	{
		(*m_callback)(m_param,pSocket,dwErr);
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
				(*m_callback)(m_param,NULL,dwErr);
			}
			catch (...)
			{
				assert((0,"Unhandled exception"));
			}
			
			guard.acquire();
		}
		
		bSignal = (m_vecPending.empty() && m_backlog == 0);
	}
	
	if (bSignal)
		m_condition.broadcast();
}

OOSvrBase::Acceptor* OOSvrBase::Win32::ProactorImpl::accept_local(void* param, void (*callback)(void* param, AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		assert((0,"Invalid parameters"));
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	PipeAcceptor* pAcceptor = new (std::nothrow) PipeAcceptor(this,MakePipeName(path),psa,param,callback);
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
				
	return pAcceptor;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::attach_local_socket(OOBase::socket_t sock, int& err)
{
	AsyncPipe* pPipe = new (std::nothrow) AsyncPipe(this,(HANDLE)sock);
	if (!pPipe)
		err = ERROR_OUTOFMEMORY;
		
	return pPipe;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::Win32::ProactorImpl::connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout)
{
	OOBase::string pipe_name(MakePipeName(path));
	
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

		if (hPipe.is_valid())
			break;

		err = GetLastError();
		if (err != ERROR_PIPE_BUSY)
			return NULL;
		
		err = 0;
		
		DWORD dwWait = NMPWAIT_USE_DEFAULT_WAIT;
		if (timeout)
		{
			countdown.update();

			dwWait = timeout2.msec();
		}

		if (!::WaitNamedPipeA(pipe_name.c_str(),dwWait))
		{
			err = GetLastError();
			return NULL;
		}
	}

	// Wrap socket
	OOSvrBase::AsyncLocalSocket* pSocket = new (std::nothrow) AsyncPipe(this,hPipe);
	if (!pSocket)
		err = ERROR_OUTOFMEMORY;
	else
		hPipe.detach();
	
	return pSocket;
}

#endif // _WIN32
