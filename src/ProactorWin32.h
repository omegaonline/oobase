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

#ifndef OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
#define OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_

#include "../include/OOSvrBase/Proactor.h"

#if defined(_WIN32)

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Win32.h"
#include "Win32Socket.h"

namespace OOSvrBase
{
	namespace detail
	{
		class ProactorWin32 : public OOSvrBase::Proactor
		{
		public:
			ProactorWin32();
			virtual ~ProactorWin32();

			int init();
			void stop();

			OOSvrBase::AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
			OOSvrBase::AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

			OOSvrBase::AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::Timeout& timeout);
			OOSvrBase::AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::Timeout& timeout);
		
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

			int run(int& err, const OOBase::Timeout& timeout);

		protected:
			OOSvrBase::Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			OOSvrBase::Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

		private:
			HANDLE           m_hPort;
			OOBase::SpinLock m_lock;
			size_t           m_outstanding;
		};
	}
}

#endif // _WIN32

#endif // OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
