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

#ifndef OOSVRBASE_PROACTOR_H_INCLUDED_
#define OOSVRBASE_PROACTOR_H_INCLUDED_

#include "../OOBase/Socket.h"

#if !defined(_WIN32)
typedef struct
{
	mode_t mode;
} SECURITY_ATTRIBUTES;
#endif

namespace OOSvrBase
{
	class AsyncSocket;

	class IOHandler
	{
	public:
		virtual void on_recv(AsyncSocket* pSocket, OOBase::Buffer* buffer, int err) = 0;
		virtual void on_sent(AsyncSocket* pSocket, OOBase::Buffer* buffer, int err) = 0;
		virtual void on_closed(AsyncSocket* pSocket) = 0;
	};

	class AsyncSocket
	{
	public:
		virtual ~AsyncSocket() {}

		virtual void bind_handler(IOHandler* handler) = 0;
		
		virtual int async_recv(OOBase::Buffer* buffer, size_t len = 0) = 0;
		virtual int async_send(OOBase::Buffer* buffer) = 0;

		virtual int recv(OOBase::Buffer* buffer, size_t len = 0, const OOBase::timeval_t* timeout = 0) = 0;
		virtual int send(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout = 0) = 0;

		virtual void shutdown(bool bSend, bool bRecv) = 0;
	};

	class AsyncLocalSocket : public AsyncSocket
	{
	public:
		/** \typedef uid_t
		 *  The platform specific user id type.
		 */
#if defined(_WIN32)
		typedef HANDLE uid_t;
#elif defined(HAVE_UNISTD_H)
		typedef ::uid_t uid_t;
#else
#error Fix me!
#endif
		virtual int get_uid(uid_t& uid) = 0;

	protected:
		AsyncLocalSocket() {}
	};

	template <typename SOCKET_TYPE>
	class Acceptor
	{
	public:
		virtual bool on_accept(SOCKET_TYPE* pSocket, const std::string& strAddress, int err) = 0;
	};

	class Proactor
	{
	public:
		Proactor();
		virtual ~Proactor();

		virtual OOBase::Socket* accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa = 0);
		virtual OOBase::Socket* accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr);

		virtual AsyncSocket* attach_socket(OOBase::Socket::socket_t sock, int* perr);
		virtual AsyncLocalSocket* attach_local_socket(OOBase::Socket::socket_t sock, int* perr);

		//virtual AsyncLocalSocket* connect_socket(const std::string& address, const std::string& port, int* perr, const OOBase::timeval_t* wait = 0);
		virtual AsyncLocalSocket* connect_local_socket(const std::string& path, int* perr, const OOBase::timeval_t* wait = 0);

	protected:
		explicit Proactor(bool);

	private:
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);

		Proactor* m_impl;
	};
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
