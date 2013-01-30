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

#include "../include/OOBase/Stack.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Condition.h"

namespace
{
	class AsyncPipe : public OOBase::AsyncSocket
	{
		friend class OOBase::AllocatorInstance;

	public:
		AsyncPipe(OOBase::detail::ProactorWin32* pProactor, HANDLE hPipe, OOBase::AllocatorInstance& allocator);
		
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
		virtual ~AsyncPipe();

		void destroy()
		{
			m_allocator.delete_free(this);
		}

		OOBase::detail::ProactorWin32* m_pProactor;
		OOBase::AllocatorInstance&     m_allocator;
		OOBase::Win32::SmartHandle     m_hPipe;
		bool                           m_send_allowed;
		bool                           m_recv_allowed;
					
		static void on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		static void on_send_v(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		
		static int translate_error(DWORD dwErr);
	};
}

AsyncPipe::AsyncPipe(OOBase::detail::ProactorWin32* pProactor, HANDLE hPipe, OOBase::AllocatorInstance& allocator) :
		m_pProactor(pProactor),
		m_allocator(allocator),
		m_hPipe(hPipe),
		m_send_allowed(true),
		m_recv_allowed(true)
{ }

AsyncPipe::~AsyncPipe()
{ 
	m_pProactor->unbind();
}

int AsyncPipe::translate_error(DWORD dwErr)
{
	switch (dwErr)
	{
	case ERROR_BROKEN_PIPE:
	case ERROR_NO_DATA:
		return 0;

	case ERROR_BAD_PIPE:
		return WSAEBADF;
		
	case ERROR_PIPE_NOT_CONNECTED:
		return WSAENOTCONN;

	default:
		return dwErr;
	}
}

int AsyncPipe::recv(void* param, OOBase::AsyncSocket::recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes)
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

	if (bytes > DWORD(-1))
		return ERROR_BUFFER_OVERFLOW;

	if (!m_recv_allowed)
		return WSAESHUTDOWN;
		
	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	err = m_pProactor->new_overlapped(pOv,&on_recv);
	if (err != 0)
		return err;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	pOv->m_extras[3] = bytes;
	buffer->addref();
	
	DWORD dwRead = 0;
	if (!ReadFile(m_hPipe,buffer->wr_ptr(),static_cast<DWORD>(bytes),&dwRead,pOv))
	{
		err = GetLastError();
		if (err != ERROR_IO_PENDING)
			on_recv(m_hPipe,dwRead,err,pOv);
	}

	return 0;
}

int AsyncPipe::recv_msg(void*, recv_msg_callback_t, OOBase::Buffer*, OOBase::Buffer*, size_t)
{
	return ERROR_NOT_SUPPORTED;
}

void AsyncPipe::on_recv(HANDLE handle, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	OOBase::Buffer* buffer = reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]);
	
	buffer->wr_ptr(dwBytes);

	if (dwErr == 0 && pOv->m_extras[3] != 0 && pOv->m_extras[3] != dwBytes)
	{
		pOv->m_extras[3] -= dwBytes;

		DWORD dwRead = 0;
		if (!ReadFile(handle,buffer->wr_ptr(),(DWORD)pOv->m_extras[3],&dwRead,pOv))
		{
			dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
				on_recv(handle,dwRead,dwErr,pOv);
		}
	}
	else
	{
		void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
		recv_callback_t callback = reinterpret_cast<recv_callback_t>(pOv->m_extras[1]);

		pOv->m_pProactor->delete_overlapped(pOv);

		// Make sure we release() buffer
		OOBase::RefPtr<OOBase::Buffer> buf2 = buffer;

		// Call callback
		if (callback)
			(*callback)(param,buffer,translate_error(dwErr));
	}
}

int AsyncPipe::send(void* param, send_callback_t callback, OOBase::Buffer* buffer)
{
	size_t bytes = (buffer ? buffer->length() : 0);
	if (bytes == 0)
		return 0;
	
	if (!buffer)
		return ERROR_INVALID_PARAMETER;

	if (bytes > DWORD(-1))
		return ERROR_BUFFER_OVERFLOW;

	if (!m_send_allowed)
		return WSAESHUTDOWN;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send);
	if (dwErr != 0)
		return dwErr;
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);
	buffer->addref();
	
	DWORD dwSent = 0;
	if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(bytes),&dwSent,pOv))
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
			on_send(m_hPipe,dwSent,dwErr,pOv);
	}

	return 0;
}

void AsyncPipe::on_send(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_callback_t callback = reinterpret_cast<send_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));

	pOv->m_pProactor->delete_overlapped(pOv);
	
	// Call callback
	if (callback)
	{
		buffer->rd_ptr(dwBytes);

		(*callback)(param,buffer,translate_error(dwErr));
	}
}

int AsyncPipe::send_v(void* param, send_v_callback_t callback, OOBase::Buffer* buffers[], size_t count)
{
	// Because scatter-gather just doesn't work on named pipes,
	// we concatenate the buffers and send as one 'atomic' write

	if (count == 0)
		return 0;
	
	if (!buffers)
		return ERROR_INVALID_PARAMETER;

	if (!m_send_allowed)
		return WSAESHUTDOWN;

	// Its more efficient to ask for the total block size up front rather than multiple small reallocs
	size_t actual_count = 0;
	size_t total = 0;
	for (size_t i=0;i<count;++i)
	{
		size_t len = (buffers[i] ? buffers[i]->length() : 0);
		if (len)
		{
			if (len > DWORD(-1) || total > DWORD(-1) - len)
				return ERROR_BUFFER_OVERFLOW;

			total += len;

			if (++actual_count > ULONG_PTR(-1))
				return ERROR_BUFFER_OVERFLOW;
		}
	}
	
	if (total == 0)
		return 0;

	OOBase::Buffer* buffer = new (std::nothrow) OOBase::Buffer(total);
	if (!buffer)
		return ERROR_OUTOFMEMORY;
	
	for (size_t i=0;i<count;++i)
	{
		size_t len = (buffers[i] ? buffers[i]->length() : 0);
		if (len)
		{
			memcpy(buffer->wr_ptr(),buffers[i]->rd_ptr(),len);
			buffer->wr_ptr(len);
		}
	}
			
	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	int dwErr = m_pProactor->new_overlapped(pOv,&on_send_v);
	if (dwErr != 0)
	{
		buffer->release();
		return dwErr;
	}
	
	pOv->m_extras[0] = reinterpret_cast<ULONG_PTR>(param);
	pOv->m_extras[1] = reinterpret_cast<ULONG_PTR>(callback);
	pOv->m_extras[2] = reinterpret_cast<ULONG_PTR>(buffer);

	if (callback)
	{
		OOBase::Buffer** ov_buffers = static_cast<OOBase::Buffer**>(m_pProactor->internal_allocate((actual_count+1) * sizeof(OOBase::Buffer*),OOBase::alignof<OOBase::Buffer*>::value));
		if (!ov_buffers)
		{
			m_pProactor->delete_overlapped(pOv);
			buffer->release();
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
		ov_buffers[actual_count] = NULL;

		pOv->m_extras[3] = reinterpret_cast<ULONG_PTR>(ov_buffers);
	}

	DWORD dwSent = 0;
	if (!WriteFile(m_hPipe,buffer->rd_ptr(),static_cast<DWORD>(total),&dwSent,pOv))
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
			on_send_v(m_hPipe,dwSent,dwErr,pOv);
	}

	return 0;
}

void AsyncPipe::on_send_v(HANDLE /*handle*/, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{
	void* param = reinterpret_cast<void*>(pOv->m_extras[0]);
	send_v_callback_t callback = reinterpret_cast<send_v_callback_t>(pOv->m_extras[1]);
	OOBase::RefPtr<OOBase::Buffer> buffer(reinterpret_cast<OOBase::Buffer*>(pOv->m_extras[2]));
	OOBase::Buffer** buffers = reinterpret_cast<OOBase::Buffer**>(pOv->m_extras[3]);

	// Call callback
	if (callback)
	{
		// Update rd_ptrs and count buffers
		size_t count = 0;
		for (;buffers[count];++count)
		{
			size_t len = buffers[count]->length();
			if (dwBytes >= len)
			{
				buffers[count]->rd_ptr(len);
				dwBytes -= len;
			}
			else if (dwBytes)
			{
				buffers[count]->rd_ptr(dwBytes);
				dwBytes = 0;
			}
		}

#if defined(OOBASE_HAVE_EXCEPTIONS)
		try
		{
#endif
			(*callback)(param,buffers,count,translate_error(dwErr));
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

		for (size_t i=0;i<count;++i)
			buffers[i]->release();

		pOv->m_pProactor->internal_free(buffers);
	}

	pOv->m_pProactor->delete_overlapped(pOv);
}

int AsyncPipe::send_msg(void*, send_msg_callback_t, OOBase::Buffer*, OOBase::Buffer*)
{
	return ERROR_NOT_SUPPORTED;
}

int AsyncPipe::shutdown(bool bSend, bool bRecv)
{
	if (bSend)
	{
		// Attempt to flush and disconnect (this may fail if we are not the 'server')
		::FlushFileBuffers(m_hPipe);
		::DisconnectNamedPipe(m_hPipe);

		m_send_allowed = false;
	}

	if (bRecv)
		m_recv_allowed = false;

	return 0;
}

/*int AsyncPipe::get_uid(OOBase::AsyncLocalSocket::uid_t& uid)
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
}*/

namespace
{	
	class InternalAcceptor
	{
		friend class OOBase::AllocatorInstance;

	public:
		InternalAcceptor(OOBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, OOBase::Proactor::accept_pipe_callback_t callback, OOBase::AllocatorInstance& allocator);
		
		int start();
		int stop();
				
	private:
		OOBase::detail::ProactorWin32*           m_pProactor;
		OOBase::AllocatorInstance&               m_allocator;
		OOBase::Condition::Mutex                 m_lock;
		OOBase::Condition                        m_condition;
		OOBase::String                           m_pipe_name;
		SECURITY_ATTRIBUTES*                     m_psa;
		size_t                                   m_backlog;
		size_t                                   m_refcount;
		OOBase::Stack<HANDLE>                    m_stkPending;
		void*                                    m_param;
		OOBase::Proactor::accept_pipe_callback_t m_callback;
	
		~InternalAcceptor();
		
		static void on_completion(HANDLE hPipe, DWORD dwBytes, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv);
		bool on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard);
		int do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, bool bFirst);
	};

	class PipeAcceptor : public OOBase::Acceptor
	{
	public:
		PipeAcceptor();
		
		int bind(OOBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, OOBase::Proactor::accept_pipe_callback_t callback, OOBase::AllocatorInstance& allocator);
		
	private:
		virtual ~PipeAcceptor();

		InternalAcceptor* m_pAcceptor;
	};
}

PipeAcceptor::PipeAcceptor() : 
		m_pAcceptor(NULL)
{ }

PipeAcceptor::~PipeAcceptor()
{
	if (m_pAcceptor)
		m_pAcceptor->stop();
}

int PipeAcceptor::bind(OOBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, OOBase::Proactor::accept_pipe_callback_t callback, OOBase::AllocatorInstance& allocator)
{
	m_pAcceptor = allocator.allocate_new<InternalAcceptor>(pProactor,pipe_name,psa,param,callback,allocator);
	if (!m_pAcceptor)
		return ERROR_OUTOFMEMORY;

	int err = m_pAcceptor->start();
	if (err != 0)
	{
		m_pAcceptor->stop();
		m_pAcceptor = NULL;
	}

	return err;
}

InternalAcceptor::InternalAcceptor(OOBase::detail::ProactorWin32* pProactor, const OOBase::String& pipe_name, SECURITY_ATTRIBUTES* psa, void* param, OOBase::Proactor::accept_pipe_callback_t callback, OOBase::AllocatorInstance& allocator) :
		m_pProactor(pProactor),
		m_allocator(allocator),
		m_pipe_name(pipe_name),
		m_psa(psa),
		m_backlog(3),
		m_refcount(1),
		m_param(param),
		m_callback(callback)
{ }

InternalAcceptor::~InternalAcceptor()
{ }

int InternalAcceptor::start()
{	
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	return do_accept(guard,true);
}

int InternalAcceptor::stop()
{
	OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
	
	if (m_backlog != 0)
	{
		m_backlog = 0;
		
		// Close all the pending handles
		for (size_t i=0;i<m_stkPending.size();++i)
			::CloseHandle(*m_stkPending.at(i));
	}
	
	// Wait for all pending operations to complete
	while (!m_stkPending.empty() && m_backlog == 0)
		m_condition.wait(m_lock);
		
	if (--m_refcount == 0)
	{
		guard.release();

		m_allocator.delete_free(this);
	}

	return 0;
}

int InternalAcceptor::do_accept(OOBase::Guard<OOBase::Condition::Mutex>& guard, bool bFirst)
{
	OOBase::StackAllocator<256> allocator;
	OOBase::TempPtr<wchar_t> wname(allocator);
	DWORD dwErr = OOBase::Win32::utf8_to_wchar_t(m_pipe_name.c_str(),wname);
	if (dwErr)
		return dwErr;

	OOBase::detail::ProactorWin32::Overlapped* pOv = NULL;
	
	while (m_stkPending.size() < m_backlog)
	{
		DWORD dwFlags = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
		if (bFirst)
		{
			dwFlags |= FILE_FLAG_FIRST_PIPE_INSTANCE;
			bFirst = false;
		}

		OOBase::Win32::SmartHandle hPipe(CreateNamedPipeW(wname,dwFlags,
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
				m_pProactor->unbind();
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
				m_pProactor->unbind();
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
	
	if (pOv)
		m_pProactor->delete_overlapped(pOv);
	
	return dwErr;
}

void InternalAcceptor::on_completion(HANDLE hPipe, DWORD /*dwBytes*/, DWORD dwErr, OOBase::detail::ProactorWin32::Overlapped* pOv)
{	
	InternalAcceptor* pThis = reinterpret_cast<InternalAcceptor*>(pOv->m_extras[0]);

	pOv->m_pProactor->delete_overlapped(pOv);
	
	OOBase::Guard<OOBase::Condition::Mutex> guard(pThis->m_lock);
	
	// Perform the callback
	pThis->on_accept(hPipe,true,dwErr,guard);
}

bool InternalAcceptor::on_accept(HANDLE hPipe, bool bRemove, DWORD dwErr, OOBase::Guard<OOBase::Condition::Mutex>& guard)
{
	bool bSignal = false;
	bool bAgain = false;
	if (bRemove)
	{		
		// Remove from pending list
		m_stkPending.remove(hPipe);
				
		bAgain = (m_stkPending.size() < m_backlog);
		bSignal = (m_stkPending.empty() && m_backlog == 0);
	}

	// If we close the pipe with outstanding connects, we get a load of ERROR_BROKEN_PIPE,
	// just ignore them
	if (dwErr == ERROR_BROKEN_PIPE)
		m_pProactor->unbind();
	else
	{
		// Keep us alive while the callbacks fire
		++m_refcount;
		
		guard.release();
		
		// Wrap the handle
		AsyncPipe* pSocket = NULL;
		if (dwErr == 0)
		{
			pSocket = m_allocator.allocate_new<AsyncPipe>(m_pProactor,hPipe,m_allocator);
			if (!pSocket)
			{
				dwErr = ERROR_OUTOFMEMORY;
				m_pProactor->unbind();
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

			m_allocator.delete_free(this);

			return true;
		}
	}
	
	if (bSignal)
		m_condition.broadcast();
	
	return false;
}

OOBase::Acceptor* OOBase::detail::ProactorWin32::accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa, AllocatorInstance& allocator)
{
	// Make sure we have valid inputs
	if (!callback || !path)
	{
		err = ERROR_INVALID_PARAMETER;
		return NULL;
	}
	
	PipeAcceptor* pAcceptor = allocator.allocate_new<PipeAcceptor>();
	if (!pAcceptor)
		err = ERROR_OUTOFMEMORY;
	else
	{
		String strPipe;
		err = strPipe.assign("\\\\.\\pipe\\");
		if (err == 0)
			err = strPipe.append(path);

		if (err == 0)
			err = pAcceptor->bind(this,strPipe,psa,param,callback,allocator);

		if (err != 0)
		{
			pAcceptor->release();
			pAcceptor = NULL;
		}
	}		
		
	return pAcceptor;
}

OOBase::AsyncSocket* OOBase::detail::ProactorWin32::attach(HANDLE hPipe, int& err, AllocatorInstance& allocator)
{
	err = bind(hPipe);
	if (err != 0)
		return NULL;

	AsyncPipe* pPipe = allocator.allocate_new<AsyncPipe>(this,hPipe,allocator);
	if (!pPipe)
	{
		unbind();
		err = ERROR_OUTOFMEMORY;
	}

	return pPipe;
}

OOBase::AsyncSocket* OOBase::detail::ProactorWin32::connect(const char* path, int& err, const Timeout& timeout, AllocatorInstance& allocator)
{
	LocalString strPipe(allocator);
	err = strPipe.assign("\\\\.\\pipe\\");
	if (err == 0)
		err = strPipe.append(path);
	if (err != 0)
		return NULL;
	
	TempPtr<wchar_t> wname(allocator);
	err = Win32::utf8_to_wchar_t(strPipe.c_str(),wname);
	if (err)
		return NULL;

	Win32::SmartHandle hPipe;
	for (;;)
	{
		hPipe = ::CreateFileW(wname,
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
		if (!timeout.is_infinite())
		{
			dwWait = timeout.millisecs();
			if (dwWait == NMPWAIT_USE_DEFAULT_WAIT)
				dwWait = 1;
		}

		if (!::WaitNamedPipeW(wname,dwWait))
		{
			err = GetLastError();
			return NULL;
		}
	}

	err = bind(hPipe);
	if (err != 0)
		return NULL;

	// Wrap socket
	AsyncSocket* pSocket = allocator.allocate_new<AsyncPipe>(this,hPipe,allocator);
	if (!pSocket)
	{
		unbind();
		err = ERROR_OUTOFMEMORY;
	}
	else
		hPipe.detach();

	return pSocket;
}

#endif // _WIN32
