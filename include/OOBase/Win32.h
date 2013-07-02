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
#include "String.h"
#include "Timeout.h"

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
				::LocalFree((HLOCAL)ptr);
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

			HANDLE detach()
			{
				HANDLE h = m_handle;
				m_handle = INVALID_HANDLE_VALUE;
				return h;
			}

			DWORD close()
			{
				if (is_valid() && !CloseHandle(m_handle))
					return GetLastError();

				m_handle = INVALID_HANDLE_VALUE;
				return 0;
			}

			bool is_valid() const
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
			void release();

		private:
			union U
			{
				CRITICAL_SECTION m_cs;
				HANDLE           m_mutex;
			} u;
		};

		int wchar_t_to_utf8(const wchar_t* wsz, OOBase::TempPtr<char>& ptrBuf);
		int utf8_to_wchar_t(const char* sz, OOBase::TempPtr<wchar_t>& wsz);

		template <typename S>
		inline int wchar_t_to_utf8(const wchar_t* wsz, S& str, AllocatorInstance& allocator)
		{
			OOBase::TempPtr<char> ptrBuf(allocator);

			int err = wchar_t_to_utf8(wsz,ptrBuf);
			if (!err)
				err = str.assign(ptrBuf);
			return err;
		}

		inline int wchar_t_to_utf8(const wchar_t* wsz, OOBase::LocalString& str)
		{
			OOBase::TempPtr<char> ptrBuf(str.get_allocator());

			int err = wchar_t_to_utf8(wsz,ptrBuf);
			if (!err)
				err = str.assign(ptrBuf);
			return err;
		}

		void AttachDebugger(DWORD pid);
	}
}
#endif // !defined(DOXYGEN)

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_H_INCLUDED_
