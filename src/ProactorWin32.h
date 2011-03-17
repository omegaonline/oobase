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

#if !defined(OOSVRBASE_PROACTOR_H_INCLUDED_)
#error include "Proactor.h" instead
#endif

#if !defined(_WIN32)
#error Includes have got confused, check Proactor.cpp
#endif

namespace OOSvrBase
{
	namespace Win32
	{
		class ProactorImpl : public Proactor, public OOBase::CustomNew
		{
		public:
			ProactorImpl();
			virtual ~ProactorImpl();

			OOBase::Socket* accept_local(Acceptor<AsyncLocalSocket>* handler, const char* path, int* perr, SECURITY_ATTRIBUTES* psa);
			OOBase::Socket* accept_remote(Acceptor<AsyncSocket>* handler, const char* address, const char* port, int* perr);

			AsyncSocketPtr attach_socket(OOBase::Socket::socket_t sock, int* perr);
			AsyncLocalSocketPtr attach_local_socket(OOBase::Socket::socket_t sock, int* perr);

			AsyncLocalSocketPtr connect_local_socket(const char* path, int* perr, const OOBase::timeval_t* wait);

			void addref();
			void release();

		private:
			OOBase::Atomic<size_t> m_refcount;
		};
	}
}

#endif // OOSVRBASE_PROACTOR_WIN32_H_INCLUDED_
