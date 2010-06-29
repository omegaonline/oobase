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

#include "../include/OOBase/Proactor.h"

#if defined(_WIN32)

#if !defined(STATUS_PIPE_BROKEN)
#define STATUS_PIPE_BROKEN 0xC000014BL
#endif

#include "../include/OOBase/Win32Socket.h"
#include "../include/OOBase/ProactorWin32.h"

namespace
{
	class AsyncSocket : public OOSvrBase::AsyncSocket
	{
	public:
		AsyncSocket(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle, OOSvrBase::IOHandler* handler);
		int bind();

		int read(OOBase::Buffer* buffer, size_t len);
		int write(OOBase::Buffer* buffer);
		void close();

	private:
		virtual ~AsyncSocket();

		bool do_read(DWORD dwToRead);
		int read_next();
		void handle_read(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		bool do_write();
		int write_next();
		void handle_write(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered);

		static VOID CALLBACK completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

		struct Completion
		{
			OVERLAPPED      m_ov;
			OOBase::Buffer* m_buffer;
			bool            m_is_reading;
			size_t          m_to_read;
			AsyncSocket*    m_this_ptr;
		};

		struct AsyncRead
		{
			OOBase::Buffer* m_buffer;
			size_t          m_to_read;
		};

		OOBase::Mutex                   m_lock;
		OOSvrBase::Win32::ProactorImpl* m_pProactor;
		OOBase::Win32::SmartHandle      m_handle;
		OOSvrBase::IOHandler*           m_handler;
		Completion                      m_read_complete;
		Completion                      m_write_complete;
		std::deque<AsyncRead>           m_async_reads;
		std::deque<OOBase::Buffer*>     m_async_writes;
	};

	AsyncSocket::AsyncSocket(OOSvrBase::Win32::ProactorImpl* pProactor, HANDLE handle, OOSvrBase::IOHandler* handler) :
			m_pProactor(pProactor),
			m_handle(handle),
			m_handler(handler)
	{
		m_read_complete.m_is_reading = true;
		m_read_complete.m_buffer = 0;
		m_read_complete.m_this_ptr = this;
		m_write_complete.m_is_reading = false;
		m_write_complete.m_buffer = 0;
		m_write_complete.m_this_ptr = this;
	}

	AsyncSocket::~AsyncSocket()
	{
		close();
	}

	int AsyncSocket::bind()
	{
		if (!OOBase::Win32::BindIoCompletionCallback(m_handle,&completion_fn,0))
			return GetLastError();

		return 0;
	}

	int AsyncSocket::read(OOBase::Buffer* buffer, size_t len)
	{
		assert(buffer);

		if (len > 0)
		{
			int err = buffer->space(len);
			if (err != 0)
				return err;
		}

		AsyncRead read = { buffer->duplicate(), len };

		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		try
		{
			m_async_reads.push_back(read);
		}
		catch (std::exception&)
		{
			read.m_buffer->release();
			return ERROR_OUTOFMEMORY;
		}

		if (m_read_complete.m_buffer)
			return 0;

		return read_next();
	}

	int AsyncSocket::read_next()
	{
		do
		{
			AsyncRead read;
			try
			{
				if (m_async_reads.empty())
					return 0;

				read = m_async_reads.front();
				m_async_reads.pop_front();
			}
			catch (std::exception&)
			{
				return ERROR_OUTOFMEMORY;
			}

			// Reset the completion info
			memset(&m_read_complete.m_ov,0,sizeof(OVERLAPPED));
			m_read_complete.m_buffer = read.m_buffer;
			m_read_complete.m_to_read = read.m_to_read;
		}
		while (!do_read(static_cast<DWORD>(m_read_complete.m_to_read)));

		return 0;
	}

	int AsyncSocket::write(OOBase::Buffer* buffer)
	{
		assert(buffer);

		if (buffer->length() == 0)
			return 0;

		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		try
		{
			m_async_writes.push_back(buffer->duplicate());
		}
		catch (std::exception&)
		{
			buffer->release();
			return ERROR_OUTOFMEMORY;
		}

		if (m_write_complete.m_buffer)
			return 0;

		return write_next();
	}

	int AsyncSocket::write_next()
	{
		do
		{
			OOBase::Buffer* buffer = 0;
			try
			{
				if (m_async_writes.empty())
					return 0;

				buffer = m_async_writes.front();
				m_async_writes.pop_front();
			}
			catch (std::exception&)
			{
				return ERROR_OUTOFMEMORY;
			}

			// Reset the completion info
			memset(&m_write_complete.m_ov,0,sizeof(OVERLAPPED));
			m_write_complete.m_buffer = buffer;

		}
		while (!do_write());

		return 0;
	}

	VOID CALLBACK AsyncSocket::completion_fn(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		Completion* pInfo = CONTAINING_RECORD(lpOverlapped,Completion,m_ov);
		AsyncSocket* this_ptr = pInfo->m_this_ptr;
		if (pInfo->m_is_reading)
			this_ptr->handle_read(dwErrorCode,dwNumberOfBytesTransfered);
		else
			this_ptr->handle_write(dwErrorCode,dwNumberOfBytesTransfered);

		this_ptr->release();
	}

	bool AsyncSocket::do_read(DWORD dwToRead)
	{
		addref();

		if (dwToRead == 0)
			dwToRead = static_cast<DWORD>(m_read_complete.m_buffer->space());

		DWORD dwRead = 0;
		if (!ReadFile(m_handle,m_read_complete.m_buffer->wr_ptr(),dwToRead,&dwRead,&m_read_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
			{
				// Update wr_ptr
				m_read_complete.m_buffer->wr_ptr(dwRead);

				bool closed = false;
				if (dwErr == ERROR_BROKEN_PIPE)
					closed = true;

				if (closed)
					dwErr = 0;

				if (m_handler && (m_read_complete.m_buffer->length() != 0 || dwErr != 0))
					m_handler->on_read(this,m_read_complete.m_buffer,dwErr);

				if (m_handler && closed)
					m_handler->on_closed(this);

				m_read_complete.m_buffer->release();
				m_read_complete.m_buffer = 0;

				release();

				return closed;
			}
		}

		return true;
	}

	void AsyncSocket::handle_read(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		// Update wr_ptr
		m_read_complete.m_buffer->wr_ptr(dwNumberOfBytesTransfered);

		if (m_read_complete.m_to_read)
			m_read_complete.m_to_read -= dwNumberOfBytesTransfered;

		bool closed = false;
		if (dwErrorCode == 0 && m_read_complete.m_to_read > 0)
		{
			// More to read
			if (do_read(static_cast<DWORD>(m_read_complete.m_to_read)))
				return;
		}
		else
		{
			// Work out if we are a close or an 'actual' error
			if (dwErrorCode == STATUS_PIPE_BROKEN)
				closed = true;

			if (closed)
				dwErrorCode = 0;

			// Call handlers
			if (m_handler && (m_read_complete.m_buffer->length() != 0 || dwErrorCode != 0))
				m_handler->on_read(this,m_read_complete.m_buffer,dwErrorCode);

			if (m_handler && closed)
				m_handler->on_closed(this);

			// Release the completed buffer
			m_read_complete.m_buffer->release();
			m_read_complete.m_buffer = 0;
		}

		// Read the next
		if (!closed)
			read_next();
	}

	bool AsyncSocket::do_write()
	{
		addref();

		DWORD dwWritten = 0;
		if (!WriteFile(m_handle,m_write_complete.m_buffer->rd_ptr(),static_cast<DWORD>(m_write_complete.m_buffer->length()),&dwWritten,&m_write_complete.m_ov))
		{
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
			{
				// Update rd_ptr
				m_write_complete.m_buffer->rd_ptr(dwWritten);

				bool closed = false;
				if (dwErr == ERROR_BROKEN_PIPE)
					closed = true;

				if (closed)
					dwErr = 0;

				if (m_handler && (dwWritten != 0 || dwErr != 0))
					m_handler->on_write(this,m_write_complete.m_buffer,dwErr);

				if (m_handler && closed)
					m_handler->on_closed(this);

				m_write_complete.m_buffer->release();
				m_write_complete.m_buffer = 0;

				release();

				return closed;
			}
		}

		return true;
	}

	void AsyncSocket::handle_write(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered)
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		// Update rd_ptr
		m_write_complete.m_buffer->rd_ptr(dwNumberOfBytesTransfered);

		bool closed = false;
		if (dwErrorCode == 0 && m_write_complete.m_buffer->length() > 0)
		{
			// More to send
			if (do_write())
				return;
		}
		else
		{
			// Work out if we are a close or an 'actual' error
			if (dwErrorCode == STATUS_PIPE_BROKEN)
				closed = true;

			if (closed)
				dwErrorCode = 0;

			// Call handlers
			if (m_handler && (dwNumberOfBytesTransfered != 0 || dwErrorCode != 0))
				m_handler->on_write(this,m_write_complete.m_buffer,dwErrorCode);

			if (m_handler && closed)
				m_handler->on_closed(this);

			// Release the completed buffer
			m_write_complete.m_buffer->release();
			m_write_complete.m_buffer = 0;
		}

		// Write the next
		if (!closed)
			write_next();
	}

	void AsyncSocket::close()
	{
		OOBase::Guard<OOBase::Mutex> guard(m_lock);

		m_handler = 0;

		// Empty the queues
		for (std::deque<AsyncRead>::iterator i=m_async_reads.begin(); i!=m_async_reads.end(); ++i)
			i->m_buffer->release();

		m_async_reads.clear();

		for (std::deque<OOBase::Buffer*>::iterator i=m_async_writes.begin(); i!=m_async_writes.end(); ++i)
			(*i)->release();

		m_async_writes.clear();

		if (m_handle.is_valid())
		{
			CancelIo(m_handle);
			CloseHandle(m_handle.detach());
		}
	}
}

OOSvrBase::Win32::ProactorImpl::ProactorImpl()
{
}

OOSvrBase::Win32::ProactorImpl::~ProactorImpl()
{
}

OOSvrBase::AsyncSocket* OOSvrBase::Win32::ProactorImpl::attach_socket(IOHandler* handler, int* perr, OOBase::Socket* sock)
{
	assert(perr);
	assert(sock);

	// Cast to our known base
	OOBase::Win32::Socket* pOrigSock = static_cast<OOBase::Win32::Socket*>(sock);

	// Duplicate the contained handle
	HANDLE hClone;
	if (!DuplicateHandle(GetCurrentProcess(),pOrigSock->peek_handle(),GetCurrentProcess(),&hClone,0,FALSE,DUPLICATE_SAME_ACCESS))
	{
		*perr = GetLastError();
		return 0;
	}

	// Alloc a new async socket
	::AsyncSocket* pSock;
	OOBASE_NEW(pSock,::AsyncSocket(this,hClone,handler));
	if (!pSock)
	{
		CloseHandle(hClone);
		*perr = ERROR_OUTOFMEMORY;
		return 0;
	}

	*perr = pSock->bind();
	if (*perr != 0)
	{
		pSock->release();
		return 0;
	}

	return pSock;
}


#endif // _WIN32
