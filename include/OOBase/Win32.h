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

#include "SharedPtr.h"
#include "String.h"
#include "Timeout.h"

#if defined(_WIN32) || defined(DOXYGEN)

namespace OOBase
{
	namespace Win32
	{
		class LocalAllocator : public AllocateNewStatic<LocalAllocator>
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

		class SmartHandle : public NonCopyable
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

			void swap(SmartHandle& rhs)
			{
				OOBase::swap(m_handle,rhs.m_handle);
			}

			HANDLE detach()
			{
				HANDLE h = m_handle;
				m_handle = INVALID_HANDLE_VALUE;
				return h;
			}

			DWORD close()
			{
				if (valid() && !CloseHandle(m_handle))
					return GetLastError();

				m_handle = INVALID_HANDLE_VALUE;
				return 0;
			}

			bool valid() const
			{
				return (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE);
			}

			HANDLE* operator &()
			{
				close();
				return &m_handle;
			}

			operator HANDLE() const
			{
				return m_handle;
			}

		private:
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
			LONG        m_nReaders;
			SmartHandle m_hReaderEvent;
			SmartHandle m_hEvent;
			SmartHandle m_hWriterMutex;
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
			bool acquire(const Timeout& timeout);
			void release();

		private:
			union U
			{
				CRITICAL_SECTION m_cs;
				HANDLE           m_mutex;
			} u;
		};

		namespace detail
		{
			int wchar_t_to_utf8(const wchar_t* wsz, char* sz, int& len);
			int utf8_to_wchar_t(const char* sz, wchar_t* wsz, int& len);
		}

		template <typename A, size_t S>
		int utf8_to_wchar_t(const char* sz, ScopedArrayPtr<wchar_t,A,S>& ptrBuf)
		{
			int len = static_cast<int>(ptrBuf.count());
			int err = detail::utf8_to_wchar_t(sz,ptrBuf.get(),len);
			if (!err)
				return 0;

			if (!ptrBuf.resize(len))
				return system_error();

			return detail::utf8_to_wchar_t(sz,ptrBuf.get(),len);
		}

		template <typename A, size_t S>
		int wchar_t_to_utf8(const wchar_t* wsz, ScopedArrayPtr<char,A,S>& ptrBuf)
		{
			int len = static_cast<int>(ptrBuf.count());
			int err = detail::wchar_t_to_utf8(wsz,ptrBuf.get(),len);
			if (!err)
				return 0;

			if (!ptrBuf.resize(len))
				return system_error();

			return detail::wchar_t_to_utf8(wsz,ptrBuf.get(),len);
		}

		void AttachDebugger(DWORD pid);
	}
}
#endif // !defined(DOXYGEN)

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_H_INCLUDED_
