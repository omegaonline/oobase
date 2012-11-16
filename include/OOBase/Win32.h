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

#ifndef OOBASE_WIN32_H_INCLUDED_
#define OOBASE_WIN32_H_INCLUDED_

#include "SmartPtr.h"
#include "StackAllocator.h"

#if defined(_WIN32) || defined(DOXYGEN)

namespace OOBase
{
	namespace Win32
	{
		class LocalAllocator
		{
		public:
			static void* allocate(size_t bytes, size_t)
			{
				return ::LocalAlloc(LMEM_FIXED,bytes);
			}

			static void free(void* ptr)
			{
				if (::LocalFree((HLOCAL)ptr) != NULL)
					OOBase_CallCriticalFailure(::GetLastError());
			}
		};

		class LocalAllocDestructor
		{
		public:
			typedef LocalAllocator Allocator;
			
			static void destroy(void* ptr)
			{
				::LocalFree(ptr);
			}
		};

		class SmartHandle
		{
		public:
			SmartHandle(HANDLE h = INVALID_HANDLE_VALUE) :
					m_handle(h)
			{}

			SmartHandle& operator = (HANDLE h)
			{
				m_handle = h;
				return *this;
			}

			~SmartHandle()
			{
				close();
			}

			HANDLE detach()
			{
				HANDLE h = m_handle;
				m_handle = INVALID_HANDLE_VALUE;
				return h;
			}

			DWORD close()
			{
				if (is_valid() && !CloseHandle(detach()))
					return GetLastError();
				else
					return 0;
			}

			bool is_valid() const
			{
				return (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE);
			}

			HANDLE* operator &()
			{
				return &m_handle;
			}

			operator HANDLE() const
			{
				return m_handle;
			}

			operator HANDLE&()
			{
				return m_handle;
			}

		private:
			SmartHandle(const SmartHandle&);
			SmartHandle& operator = (const SmartHandle&);

			HANDLE m_handle;
		};	

#if !defined(DOXYGEN)
		class rwmutex_t
		{
		public:
			rwmutex_t();
			~rwmutex_t();

			void acquire();
			BOOL try_acquire();
			void release();
			void acquire_read();
			BOOL try_acquire_read();
			void release_read();

		private:
			LONG                       m_nReaders;
			OOBase::Win32::SmartHandle m_hReaderEvent;
			OOBase::Win32::SmartHandle m_hEvent;
			OOBase::Win32::SmartHandle m_hWriterMutex;
		};

		class condition_variable_t
		{
		public:
			condition_variable_t();
			~condition_variable_t();

			bool wait(HANDLE hMutex, DWORD dwMilliseconds);
			void signal();
			void broadcast();

		private:
			CRITICAL_SECTION m_waiters_lock;
			unsigned long    m_waiters;
			bool             m_broadcast;
			SmartHandle      m_sema;
			SmartHandle      m_waiters_done;
		};
#endif // !defined(DOXYGEN)
	}
}

#if !defined(DOXYGEN)

#if (_WIN32_WINNT < 0x0600)
	typedef OOBase::Win32::condition_variable_t* CONDITION_VARIABLE;
	typedef OOBase::Win32::rwmutex_t* SRWLOCK;
#endif

namespace OOBase
{
	namespace Win32
	{
		class condition_mutex_t
		{
			// We need private access in this function
			friend BOOL SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds);

		public:
			condition_mutex_t();
			~condition_mutex_t();

			bool try_acquire();
			void acquire();
			void release();

		private:
			union U
			{
				CRITICAL_SECTION m_cs;
				HANDLE           m_mutex;
			} u;
		};

		template <typename S>
		int wchar_t_to_utf8(const wchar_t* wsz, S& str)
		{
			int len = WideCharToMultiByte(CP_UTF8,0,wsz,-1,NULL,0,NULL,NULL);
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_INSUFFICIENT_BUFFER)
				return dwErr;

			OOBase::StackAllocator<256> allocator;
			OOBase::TempPtr<char> ptrBuf(allocator);
			if (!ptrBuf.reallocate(len))
				return ERROR_OUTOFMEMORY;

			len = WideCharToMultiByte(CP_UTF8,0,wsz,-1,ptrBuf,len,NULL,NULL);
			if (len <= 0)
				return GetLastError();

			return str.assign(ptrBuf,len);
		}

		int utf8_to_wchar_t(const char* sz, OOBase::TempPtr<wchar_t>& wsz)
		{
			int len = MultiByteToWideChar(CP_UTF8,0,sz,-1,NULL,0);
			DWORD dwErr = GetLastError();
			if (dwErr != ERROR_INSUFFICIENT_BUFFER)
				return dwErr;

			if (!wsz.reallocate(len + 1))
				return ERROR_OUTOFMEMORY;

			len = MultiByteToWideChar(CP_UTF8,0,sz,-1,wsz,len + 1);
			if (len <= 0)
				return GetLastError();

			wsz[len] = L'\0';
			return ERROR_SUCCESS;
		}
	}
}
#endif // !defined(DOXYGEN)

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_H_INCLUDED_
