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
		public:
			ProactorWin32();
			virtual ~ProactorWin32();

			void stop();

			AsyncSocket* attach_socket(socket_t sock, int& err);
			AsyncLocalSocket* attach_local_socket(socket_t sock, int& err);

			AsyncLocalSocket* accept_local_socket(HANDLE hPipe, int& err, const Timeout& timeout);

			AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout);
			AsyncLocalSocket* connect_local_socket(const char* path, int& err, const Timeout& timeout);
		
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
									
			int bind(HANDLE hFile);
			void unbind(HANDLE hFile);

			int run(int& err, const Timeout& timeout);

		protected:
			Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

		private:
			HANDLE   m_hPort;
			SpinLock m_lock;
			size_t   m_bound;
		};
	}
}

#endif // _WIN32

#endif // OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
