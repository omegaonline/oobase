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

namespace OOSvrBase
{
	namespace Win32
	{
		class ProactorImpl : public Proactor
		{
		public:
			ProactorImpl();
			virtual ~ProactorImpl();

			AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
			AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

			AsyncSocket* connect_socket(const struct sockaddr* addr, size_t addr_len, int& err, const OOBase::timeval_t* timeout);
			AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout);
		
			struct Overlapped : public OVERLAPPED
			{
				void (*m_callback)(HANDLE handle, DWORD dwBytes, DWORD dwErr, Overlapped* pOv);
				ULONG_PTR m_extras[5];				
			};
			typedef void (*pfnCompletion_t)(HANDLE handle, DWORD dwBytes, DWORD dwErr, Overlapped* pOv);
		
			int new_overlapped(Overlapped*& pOv, pfnCompletion_t callback);
			void delete_overlapped(Overlapped* pOv);
			
			int bind(HANDLE hFile);

		protected:
			Acceptor* accept_local(void* param, void (*callback)(void* param, AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			Acceptor* accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err);

		private:
			size_t m_refcount;
		};
	}
}

#endif // OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
