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

#include "../include/OOBase/Stack.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Condition.h"

#if defined(max)
#undef max
#endif

#if defined(min)
#undef min
#endif

#include <limits>

namespace
{
	int MakePipeName(OOBase::String& str, const char* name)
	{
		int err = str.assign("\\\\.\\pipe\\");
		if (err == 0)
			err = str.append(name);
		
		return err;
	}

	class AsyncPipe : public OOSvrBase::AsyncLocalSocket
	{
	public:
		AsyncPipe(OOSvrBase::detail::ProactorWin32* pProactor, HANDLE hPipe);
		
		int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
		int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);

		int get_uid(uid_t& uid);
	
	private:
		virtual ~AsyncPipe();

		OOSvrBase::detail::ProactorWin32* m_pProactor;
		OOBase::Win32::SmartHandle        m_hPipe;
					
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);

		static void translate_error(int& dwErr);
		static void translate_error(DWORD& dwErr);
	};
}

AsyncPipe::AsyncPipe(OOSvrBase::detail::ProactorWin32* pProactor, HANDLE hPipe) :
		m_pProactor(pProactor),
		m_hPipe(hPipe)
{ }

AsyncPipe::~AsyncPipe()
{ 
	m_pProactor->unbind(m_hPipe);
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
		dwErr = 0;
		break;

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

	if (bytes == 0)
		return 0;
	
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
	
	if (!ReadFile(m_hPipe,buffer->wr_ptr(),static_cast<DWORD>(bytes),NULL,pOv))
	{
		err = GetLastError();
		if (err != ERROR_IO_PENDING)
			on_recv(m_hPipe,static_cast<DWORD>(bytes),err,pOv);
	}
			
	return 0;
}

void AsyncPipe::on_recv(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[1]);

	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));
	buffer->wr_ptr(dwBytes);

	delete pOv;
	
	// Translate error codes...
	translate_error(dwErr);
		
	// Call callback
	(*callback)(param,buffer,dwErr);
}

int AsyncPipe::send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
{
	size_t bytes = buffer->length();
	if (bytes == 0)
		return 0;
	
	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	buffer->addref();
	
	if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(bytes),NULL,pOv))
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
			on_send(m_hPipe,static_cast<DWORD>(bytes),dwErr,pOv);
	}
		
	return 0;
}

void AsyncPipe::on_send(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer, int err);
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));
		
	delete pOv;
	
	// Call callback
	if (callback)
	{
		buffer->rd_ptr(dwBytes);

		// Translate error codes...
		translate_error(dwErr);

		(*callback)(param,buffer,dwErr);
	}
}

int AsyncPipe::send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
{
	// Because scatter-gather just doesn't work on named pipes,
	// we concatenate the buffers and send as one 'atomic' write
	if (count == 0)
		return 0;
	
	// Its more efficient to ask for the total block size up front rather than multiple small reallocs
	size_t total = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		if (std::numeric_limits<size_t>::max() - len < total)
			return WSAENOBUFS;
		
		total += len;
	}
	
	if (total == 0)
		return 0;
	
	OOBase::RefPtr<OOBase::Buffer> buffer = new (std::nothrow) OOBase::Buffer(total);
	if (!buffer)
		return ERROR_OUTOFMEMORY;
	
	for (size_t i=0;i<count;++i)
	{
		size_t len = buffers[i]->length();
		memcpy(buffer->wr_ptr(),buffers[i]->rd_ptr(),len);
		buffer->wr_ptr(len);
	}
			
	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer.addref());
		
	if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(total),NULL,pOv))
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
			on_send_v(m_hPipe,static_cast<DWORD>(total),dwErr,pOv);
	}
	
	return 0;
}

void AsyncPipe::on_send_v(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{
	typedef void (*pfnCallback)(void* param, OOBase::Buffer* buffer[], size_t count, int err);
	
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	pfnCallback callback = reinterpret_cast<pfnCallback>(pOv->m_extras[1]);
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]);
	
	delete pOv;
	
	// Call callback
	if (callback)
	{
		// Translate error codes...
		translate_error(dwErr);

		buffer->rd_ptr(dwBytes);

		try
		{
			(*callback)(param,&buffer,1,dwErr);
		}
		catch (...)
		{
			buffer->release();
			throw;
		}
	}

	buffer->release();
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
		typedef void (*callback_t)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err);
	
		PipeAcceptor();
		
		int listen(size_t backlog);
		int stop();
		int bind(OOSvrBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, callback_t callback);
		
	private:
		virtual ~PipeAcceptor();

		class Acceptor
		{
		public:
			Acceptor(OOSvrBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, PipeAcceptor::callback_t callback);
			
			int listen(size_t backlog);
			int stop(bool destroy);
					
		private:
			OOSvrBase::detail::ProactorWin32* m_pProactor;
			OOBase::Condition::Mutex          m_lock;
			OOBase::Condition                 m_condition;
			OOBase::String                    m_pipe_name;
			SECURITY_ATTRIBUTES*              m_psa;
			size_t                            m_backlog;
			size_t                            m_refcount;
			OOBase::Stack<HANDLE>             m_stkPending;
			void*                             m_param;
			PipeAcceptor::callback_t          m_callback;
		
			~Acceptor();
			
			static void on_completion(HANDLE hPipe, DWORD dwBytes, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv);
			bool on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
			int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, bool bFirst);
		};
		Acceptor* m_pAcceptor;
	};
}

PipeAcceptor::PipeAcceptor() : 
		m_pAcceptor(NULL)
{ }

PipeAcceptor::~PipeAcceptor()
{
	if (m_pAcceptor)
		m_pAcceptor->stop(true);
}

int PipeAcceptor::listen(size_t backlog)
{
	if (!m_pAcceptor)
		return WSAEBADF;
	
	return m_pAcceptor->listen(backlog);
}

int PipeAcceptor::stop()
{
	if (!m_pAcceptor)
		return WSAEBADF;
	
	return m_pAcceptor->stop(false);
}

int PipeAcceptor::bind(OOSvrBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, callback_t callback)
{
	m_pAcceptor = new (std::nothrow) PipeAcceptor::Acceptor(pProactor,pipe_name,psa,param,callback);
	if (!m_pAcceptor)
		return ERROR_OUTOFMEMORY;
	
	return 0;
}

PipeAcceptor::Acceptor::Acceptor(OOSvrBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, PipeAcceptor::callback_t callback) :
		m_pProactor(pProactor),
		m_pipe_name(pipe_name),
		m_psa(psa),
		m_backlog(0),
		m_refcount(1),
		m_param(param),
		m_callback(callback)
{ }

PipeAcceptor::Acceptor::~Acceptor()
{ }

int PipeAcceptor::Acceptor::listen(size_t backlog)
{	
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (backlog > 3 || backlog == 0)
		backlog = 3;
	
	if (m_backlog != 0)
	{
		m_backlog = backlog;
		return 0;
	}
	else
	{
		m_backlog = backlog;
		
		return do_accept(guard,true);
	}
}

int PipeAcceptor::Acceptor::stop(bool destroy)
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		// Close all the pending handles
		for (size_t i=0;i<m_stkPending.size();++i)
			CloseHandle(*m_stkPending.at(i));
	}
	
	// Wait for all pending operations to complete
	while (!m_stkPending.empty() && m_backlog == 0)
		m_condition.wait(m_lock);
		
	if (destroy && --m_refcount == 0)
	{
		guard.release();
		delete this;
	}

	return 0;
}

int PipeAcceptor::Acceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, bool bFirst)
{
	DWORD dwErr = 0;
	OOSvrBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	
	while (m_stkPending.size() < m_backlog)
	{
		DWORD dwFlags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
		if (bFirst)
		{
			dwFlags |= FILE_FLAG_FIRST_PIPE_INSTANCE;
			bFirst = false;
		}

		OOBase::Win32::SmartHandle hPipe(CreateNamedPipeA(m_pipe_name.c_str(),dwFlags,
												PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
												PIPE_UNLIMITED_INSTANCES,
												0,0,0,m_psa));

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
			{
				m_pProactor->unbind(hPipe);
				break;
			}
			
			// Set this pointer
			pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(this);
		}
		
		if (!ConnectNamedPipe(hPipe,pOv))
		{
			dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING)
			{
				// Will complete later...
				dwErr = m_stkPending.push(hPipe);
				if (dwErr == 0)
				{
					hPipe.detach();
					pOv = NULL;
					continue;
				}
			}
			
			if (dwErr != ERROR_PIPE_CONNECTED)
			{
				m_pProactor->unbind(hPipe);
				break;
			}
			
			dwErr = 0;
		}
		
		// Call the callback
		if (on_accept(hPipe.detach(),false,0,guard))
		{
			// We have self destructed
			break;
		}
	}
	
	delete pOv;
	
	return dwErr;
}

void PipeAcceptor::Acceptor::on_completion(HANDLE hPipe, DWORD /*dwBytes*/, DWORD dwErr, OOSvrBase::detail::ProactorWin32::Overlapped* pOv)
{	
	PipeAcceptor::Acceptor* pThis = reinterpret_cast<PipeAcceptor::Acceptor*>(pOv->m_extras[0]);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	// Perform the callback
	pThis->on_accept(hPipe,true,dwErr,guard);
	
	delete pOv;
}

bool PipeAcceptor::Acceptor::on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	bool bSignal = false;
	bool bAgain = false;
	if (bRemove)
	{		
		// Remove from pending list
		m_stkPending.erase(hPipe);
				
		bAgain = (m_stkPending.size() < m_backlog);
		bSignal = (m_stkPending.empty() && m_backlog == 0);
	}

	// If we close the pipe with outstanding connects, we get a load of ERROR_BROKEN_PIPE,
	// just ignore them
	if (dwErr == ERROR_BROKEN_PIPE)
		m_pProactor->unbind(hPipe);
	else
	{
		// Keep us alive while the callbacks fire
		++m_refcount;
		
		guard.release();
		
		// Wrap the handle
		OOBase::RefPtr<OOSvrBase::AsyncLocalSocket> pSocket;

		if (dwErr == 0)
		{
			pSocket = new (std::nothrow) AsyncPipe(m_pProactor,hPipe);
			if (!pSocket)
			{
				dwErr = ERROR_OUTOFMEMORY;
				m_pProactor->unbind(hPipe);
				CloseHandle(hPipe);
			}
		}
	
		(*m_callback)(m_param,pSocket,dwErr);
			
		guard.acquire();
		
		if (bAgain)
		{
			// Try to accept some more connections
			dwErr = do_accept(guard,false);
			if (dwErr != 0)
			{
				guard.release();
				
				(*m_callback)(m_param,NULL,dwErr);
								
				guard.acquire();
			}
			
			bSignal = (m_stkPending.empty() && m_backlog == 0);
		}
		
		// See if we have been killed in the callback
		if (--m_refcount == 0)
		{
			guard.release();
			delete this;
			return true;
		}
	}
	
	if (bSignal)
		m_condition.broadcast();
	
	return false;
}

OOSvrBase::Acceptor* OOSvrBase::detail::ProactorWin32::accept_local(void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	PipeAcceptor* pAcceptor = new (std::nothrow) PipeAcceptor();
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
	else
	{
		OOBase::String strPipe;
		err = MakePipeName(strPipe,path);
		if (err == 0)
			err = pAcceptor->bind(this,strPipe,psa,param,callback);

		if (err != 0)
		{
			pAcceptor->release();
			pAcceptor = NULL;
		}
	}		
		
	return pAcceptor;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::detail::ProactorWin32::attach_local_socket(OOBase::socket_t sock, int& err)
{
	err = bind((HANDLE)sock);
	if (err != 0)
		return NULL;

	AsyncPipe* pPipe = new (std::nothrow) AsyncPipe(this,(HANDLE)sock);
	if (!pPipe)
	{
		unbind((HANDLE)sock);
		err = ERROR_OUTOFMEMORY;
	}
		
	return pPipe;
}

OOSvrBase::AsyncLocalSocket* OOSvrBase::detail::ProactorWin32::connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout)
{
	OOBase::String pipe_name;
	err = MakePipeName(pipe_name,path);
	if (err != 0)
		return NULL;
	
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

	err = bind(hPipe);
	if (err != 0)
		return NULL;

	// Wrap socket
	OOSvrBase::AsyncLocalSocket* pSocket = new (std::nothrow) AsyncPipe(this,hPipe);
	if (!pSocket)
	{
		unbind(hPipe);
		err = ERROR_OUTOFMEMORY;
	}
	else
		hPipe.detach();
	
	return pSocket;
}

#endif // _WIN32
