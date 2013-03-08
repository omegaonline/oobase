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

#ifndef OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
#define OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_

#include "../include/OOBase/Proactor.h"

#if defined(_WIN32)

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Win32.h"
#include "Win32Socket.h"

namespace OOBase
{
	namespace detail
	{
		class ProactorWin32 : public Proactor
		{
		// Proactor public members
		public:
			Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);
			Acceptor* accept_unique_pipe(void* param, accept_pipe_callback_t callback, /*(out)*/ char path[64], int& err, SECURITY_ATTRIBUTES* psa);

			AsyncSocket* attach(socket_t sock, int& err);
			AsyncSocket* attach(HANDLE hPipe, int& err);

			AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout);
			AsyncSocket* connect(const char* path, int& err, const Timeout& timeout);

			Acceptor* wait_for_object(void* param, wait_object_callback_t callback, HANDLE hObject, int& err, ULONG dwMilliseconds);

			// 'Internal' public members
		public:
			ProactorWin32();
			virtual ~ProactorWin32();

			void stop();
			int restart();
		
			struct Overlapped : public OVERLAPPED
			{
				LONG m_refcount;
				ProactorWin32* m_pProactor;
				void (*m_callback)(HANDLE handle, DWORD dwBytes, DWORD dwErr, Overlapped* pOv);
				ULONG_PTR m_extras[4];
			};
			typedef void (*pfnCompletion_t)(HANDLE handle, DWORD dwBytes, DWORD dwErr, Overlapped* pOv);
		
			int new_overlapped(Overlapped*& pOv, pfnCompletion_t callback);
			void delete_overlapped(Overlapped* pOv);

			int post_completion(DWORD dwBytes, ULONG_PTR dwKey, Overlapped* pOv);
									
			int bind(HANDLE hFile);
			void unbind();

			int run(int& err, const Timeout& timeout);

			AllocatorInstance& get_internal_allocator()
			{
				return m_allocator;
			}

		private:
			HANDLE   m_hPort;
			SpinLock m_lock;
			size_t   m_bound;

			LockedAllocator<4096> m_allocator;
		};
	}
}

#endif // _WIN32

#endif // OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
