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

#include "../config-base.h"

#if defined(_WIN32)

namespace OOBase
{
	namespace Win32
	{
		template <typename T>
		class LocalAllocDestructor
		{
		public:
			static void destroy(T* ptr)
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

		class rwmutex_t
		{
		public:
			rwmutex_t();
			~rwmutex_t();

			void acquire();
			void release();
			void acquire_read();
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

		class condition_mutex_t
		{
			// We need private access in this function
			friend BOOL SleepConditionVariable(CONDITION_VARIABLE* ConditionVariable, condition_mutex_t* Mutex, DWORD dwMilliseconds);

		public:
			condition_mutex_t();
			~condition_mutex_t();

			bool tryacquire();
			void acquire();
			void release();

		private:
			union U
			{
				CRITICAL_SECTION m_cs;
				HANDLE           m_mutex;
			} u;
		};
	}
}

#endif // defined(_WIN32)

#endif // OOBASE_WIN32_H_INCLUDED_
