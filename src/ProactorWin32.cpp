///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#include "../include/OOSvrBase/Proactor.h"

#if defined(_WIN32)

#if !defined(STATUS_PIPE_BROKEN)
#define STATUS_PIPE_BROKEN 0xC000014BL
#endif

#include "../include/OOBase/SmartPtr.h"
#include "../include/OOSvrBase/internal/ProactorImpl.h"
#include "../include/OOSvrBase/internal/ProactorWin32.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4250) // Moans about virtual base classes
#endif

namespace
{
	class IOHelper : public OOSvrBase::detail::AsyncIOHelper
	{
	public:
		IOHelper(HANDLE handle);
		virtual ~IOHelper();

		int bind();
		
		void recv(AsyncOp* recv_op);
		void send(AsyncOp* send_op);
				
	protected:
		OOBase::Win32::SmartHandle m_handle;

	private:
		void do_read();
		void handle_read(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		void do_write();
		void handle_write(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

		struct Completion
		{
			OVERLAPPED      m_ov;
			AsyncOp*        m_op;
			bool            m_is_reading;
			IOHelper*       m_this_ptr;
		};

		Completion m_read_complete;
		Completion m_write_complete;
	};

	class PipeIOHelper : public IOHelper
	{
	public:
		PipeIOHelper(HANDLE handle) : IOHelper(handle)
		{}

		void shutdown()
		{
			DisconnectNamedPipe(m_handle);
		}
	};

	class SocketIOHelper : public IOHelper
	{
	public:
		SocketIOHelper(HANDLE handle) : IOHelper(handle)
		{}

		void shutdown()
		{
			::shutdown((SOCKET)(HANDLE)m_handle,SD_BOTH);
		}
	};

	IOHelper::IOHelper(HANDLE handle) :
			m_handle(handle)
	{
		m_read_complete.m_is_reading = true;
		m_read_complete.m_op = 0;
		m_read_complete.m_this_ptr = this;
		m_write_complete.m_is_reading = false;
		m_write_complete.m_op = 0;
		m_write_complete.m_this_ptr = this;
	}

	IOHelper::~IOHelper()
	{
	}

	int IOHelper::bind()
	{
		if (!OOBase::Win32::BindIoCompletionCallback(m_handle,&completion_fn,0))
			return GetLastError();

		return 0;
	}

	void IOHelper::recv(AsyncOp* recv_op)
	{
		assert(!m_read_complete.m_op);

		// Reset the completion info
		memset(&m_read_complete.m_ov,0,sizeof(OVERLAPPED));
		m_read_complete.m_op = recv_op;
				
		do_read();
	}

	void IOHelper::send(AsyncOp* send_op)
	{
		assert(!m_write_complete.m_op);

		// Reset the completion info
		memset(&m_write_complete.m_ov,0,sizeof(OVERLAPPED));
		m_write_complete.m_op = send_op;
				
		do_write();
	}

	VOID CALLBACK IOHelper::completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
		if (pInfo->m_is_reading)
			pInfo->m_this_ptr->handle_read(dwErrorCode,dwNumberOfBytesTransfered);
		else
			pInfo->m_this_ptr->handle_write(dwErrorCode,dwNumberOfBytesTransfered);
	}

	void IOHelper::do_read()
	{
		DWORD dwToRead = static_cast<DWORD>(m_read_complete.m_op->len);
		if (dwToRead == 0)
			dwToRead = static_cast<DWORD>(m_read_complete.m_op->buffer->space());

		DWORD dwRead = 0;
		if (!ReadFile(m_handle,m_read_complete.m_op->buffer->wr_ptr(),dwToRead,&dwRead,&m_read_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
				handle_read(dwErr,dwRead);
		}
	}

	void IOHelper::handle_read(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered)
	{
		// Update wr_ptr
		m_read_complete.m_op->buffer->wr_ptr(dwNumberOfBytesTransfered);

		if (m_read_complete.m_op->len)
			m_read_complete.m_op->len -= dwNumberOfBytesTransfered;

		if (dwErrorCode == 0 && m_read_complete.m_op->len > 0)
		{
			// More to read
			return do_read();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_read_complete.m_op;
		m_read_complete.m_op = 0;

		// Call handlers
		m_handler->on_recv(op,dwErrorCode);
	}

	void IOHelper::do_write()
	{
		DWORD dwWritten = 0;
		if (!WriteFile(m_handle,m_write_complete.m_op->buffer->rd_ptr(),static_cast<DWORD>(m_write_complete.m_op->buffer->length()),&dwWritten,&m_write_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
				handle_write(dwErr,dwWritten);
		}
	}

	void IOHelper::handle_write(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered)
	{
		// Update rd_ptr
		m_write_complete.m_op->buffer->rd_ptr(dwNumberOfBytesTransfered);

		if (dwErrorCode == 0 && m_write_complete.m_op->buffer->length() > 0)
		{
			// More to send
			return do_write();
		}
		
		// Stash op... another op may be about to occur...
		AsyncOp* op = m_write_complete.m_op;
		m_write_complete.m_op = 0;

		// Call handlers
		m_handler->on_sent(op,dwErrorCode);
	}

	class AsyncLocalSocket : public OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>
	{
	public:
		AsyncLocalSocket(HANDLE hPipe, PipeIOHelper* helper) :
				OOSvrBase::detail::AsyncSocketTempl<OOSvrBase::AsyncLocalSocket>(helper),
				m_hPipe(hPipe)
		{
		}

		static AsyncLocalSocket* Create(HANDLE hPipe, DWORD& dwErr)
		{
			PipeIOHelper* helper = 0;
			OOBASE_NEW(helper,PipeIOHelper(hPipe));
			if (!helper)
			{
				dwErr = ERROR_OUTOFMEMORY;
				return 0;
			}

			AsyncLocalSocket* sock = 0;
			OOBASE_NEW(sock,AsyncLocalSocket(hPipe,helper));
			if (!sock)
			{
				dwErr = ERROR_OUTOFMEMORY;
				delete helper;
				return 0;
			}

			return sock;
		}

	private:
		bool is_close(int err) const
		{
			return  (err == STATUS_PIPE_BROKEN);
		}

		HANDLE m_hPipe;

		int get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid);
	};

	int AsyncLocalSocket::get_uid(OOSvrBase::AsyncLocalSocket::uid_t& uid)
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

	class PipeAcceptor : public OOBase::Socket
	{
	public:
		PipeAcceptor(const std::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler);
		virtual ~PipeAcceptor();

		int send(const void* /*buf*/, size_t /*len*/, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			return ERROR_INVALID_FUNCTION;
		}

		size_t recv(void* /*buf*/, size_t /*len*/, int* perr, const OOBase::timeval_t* /*timeout*/ = 0)
		{
			*perr = ERROR_INVALID_FUNCTION;
			return 0;
		}

		void shutdown();

		int accept_named_pipe(bool bExclusive);

	private:
		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

		struct Completion
		{
			OVERLAPPED                 m_ov;
			OOBase::Win32::SmartHandle m_hPipe;
			PipeAcceptor*              m_this_ptr;
		};

		OOBase::Condition::Mutex                           m_lock;
		OOBase::Condition                                  m_condition;
		bool                                               m_closed;
		Completion                                         m_completion;
		OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>*  m_handler;
		const std::string                                  m_pipe_name;
		LPSECURITY_ATTRIBUTES                              m_psa;
		size_t                                             m_async_count;
		
		void do_accept(OOBase::Win32::SmartHandle& hPipe, DWORD dwErr);
	};

	PipeAcceptor::PipeAcceptor(const std::string& pipe_name, LPSECURITY_ATTRIBUTES psa, OOSvrBase::Acceptor<OOSvrBase::AsyncLocalSocket>* handler) :
			m_closed(false),
			m_handler(handler),
			m_pipe_name(pipe_name),
			m_psa(psa),
			m_async_count(0)
	{
		m_completion.m_this_ptr = this;
	}

	PipeAcceptor::~PipeAcceptor()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		m_closed = true;

		while (m_async_count)
		{
			m_condition.wait(m_lock);
		}
	}

	int PipeAcceptor::accept_named_pipe(bool bExclusive)
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		assert(!m_completion.m_hPipe.is_valid());

		DWORD dwOpenMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;

		if (bExclusive)
			dwOpenMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;

		m_completion.m_hPipe = CreateNamedPipeA(m_pipe_name.c_str(),dwOpenMode,
								   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
								   PIPE_UNLIMITED_INSTANCES,
								   0,
								   4096,
								   4096,
								   m_psa);

		if (!m_completion.m_hPipe.is_valid())
			return GetLastError();

		if (!OOBase::Win32::BindIoCompletionCallback(m_completion.m_hPipe,&completion_fn,0))
		{
			int err = GetLastError();
			m_completion.m_hPipe.close();
			return err;
		}

		memset(&m_completion.m_ov,0,sizeof(OVERLAPPED));

		++m_async_count;

		DWORD dwErr = 0;
		if (ConnectNamedPipe(m_completion.m_hPipe,&m_completion.m_ov))
			dwErr = ERROR_PIPE_CONNECTED;
		else
		{
			dwErr = GetLastError();
			if (dwErr == ERROR_IO_PENDING)
				dwErr = 0;
		}

		if (dwErr == ERROR_PIPE_CONNECTED)
		{
			guard.release();
			do_accept(m_completion.m_hPipe,0);
			dwErr = 0;
		}
		else if (dwErr != 0)
		{
			m_completion.m_hPipe.close();
			--m_async_count;
		}

		return dwErr;
	}

	VOID CALLBACK PipeAcceptor::completion_fn(DWORD dwErrorCode, DWORD, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
		
		pInfo->m_this_ptr->do_accept(pInfo->m_hPipe,dwErrorCode);
	}

	void PipeAcceptor::do_accept(OOBase::Win32::SmartHandle& hPipe, DWORD dwErr)
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		--m_async_count;

		if (m_closed)
		{
			hPipe.close();
			m_condition.signal();
		}
		else
		{
			AsyncLocalSocket* pSocket = 0;
			if (dwErr == 0)
			{
				pSocket = AsyncLocalSocket::Create(hPipe,dwErr);
				if (pSocket)
					hPipe.detach();
				else
					hPipe.close();
			}

			guard.release();

			// Call the acceptor
			bool again = m_handler->on_accept(pSocket,dwErr);

			// Submit another accept if we want
			while (again)
			{
				dwErr = accept_named_pipe(false);
				if (dwErr == 0)
					break;
					
				again = m_handler->on_accept(0,dwErr);
			}
		}
	}

	void PipeAcceptor::shutdown()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		if (m_completion.m_hPipe.is_valid())
			DisconnectNamedPipe(m_completion.m_hPipe);
	}
}

OOSvrBase::Win32::ProactorImpl::ProactorImpl() :
		OOSvrBase::Proactor(false)
{
}

OOSvrBase::Win32::ProactorImpl::~ProactorImpl()
{
}

OOBase::Socket* OOSvrBase::Win32::ProactorImpl::accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa)
{
	OOBase::SmartPtr<PipeAcceptor> pAcceptor = 0;
	OOBASE_NEW(pAcceptor,PipeAcceptor("\\\\.\\pipe\\" + path,psa,handler));
	if (!pAcceptor)
		*perr = ERROR_OUTOFMEMORY;
	else
		*perr = pAcceptor->accept_named_pipe(true);
	
	if (*perr != 0)
		return 0;

	return pAcceptor.detach();
}

OOBase::Socket* OOSvrBase::Win32::ProactorImpl::accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr)
{
	void* TODO;
	return 0;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // _WIN32
