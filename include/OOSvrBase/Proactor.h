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

#include "../OOBase/SmartPtr.h"
#include "../OOBase/Socket.h"

#if !defined(_WIN32)
typedef struct
{
	mode_t mode;
} SECURITY_ATTRIBUTES;
#endif

namespace OOSvrBase
{
	class AsyncSocket
	{
	public:
		virtual ~AsyncSocket() {}
			
		template <typename T>
		int recv(T* param, void (T::*callback)(OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout)
		{
			Thunk<T>* thunk = new Thunk<T>(param,callback);
			return recv(thunk,&Thunk<T>::callback,buffer,bytes,timeout);
		}

		template <typename T>
		int send(T* param, void (T::*callback)(OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer)
		{
			Thunk<T>* thunk = new Thunk<T>(param,callback);
			return send(thunk,&Thunk<T>::callback,buffer);
		}

		template <typename T>
		int send_v(T* param, void (T::*callback)(OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count)
		{
			ThunkV<T>* thunk = new ThunkV<T>(param,callback);
			return send_v(thunk,&ThunkV<T>::callback,buffers,count);
		}

		int recv(OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
		int send(OOBase::Buffer* buffer);

		virtual void shutdown(bool bSend, bool bRecv) = 0;

	protected:
		AsyncSocket() {}
		
		virtual int recv(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout) = 0;
		virtual int send(void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer) = 0;
		virtual int send_v(void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count) = 0;

	private:
		AsyncSocket(const AsyncSocket&);
		AsyncSocket& operator = (const AsyncSocket&);
			
		template <typename T>
		struct Thunk
		{
			T* m_param;
			void (T::*m_callback)(OOBase::Buffer* buffer, int err);
		
			static void callback(void* param, OOBase::Buffer* buffer, int err)
			{
				Thunk thunk = *static_cast<Thunk*>(param);
				delete static_cast<Thunk*>(param);
				
				thunk.m_param->(*thunk.m_callback)(buffer,err);
			}
		};
		
		template <typename T>
		struct ThunkV
		{
			T* m_param;
			void (T::*m_callback)(OOBase::Buffer* buffers[], size_t count, int err);
		
			static void callback(void* param, OOBase::Buffer* buffers[], size_t count, int err)
			{
				ThunkV thunk = *static_cast<ThunkV*>(param);
				delete static_cast<ThunkV*>(param);
				
				thunk.m_param->(*thunk.m_callback)(buffers,count,err);
			}
		};
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
	};
	
	class Acceptor
	{
	public:
		virtual ~Acceptor() {}
			
		virtual int listen(size_t backlog = SOMAXCONN) = 0;
		virtual int stop() = 0;
			
	protected:
		Acceptor() {}
				
	private:
		Acceptor(const Acceptor&);
		Acceptor& operator = (const Acceptor&);
	};

	class Proactor
	{
	public:
		Proactor();
		virtual ~Proactor();

		template <typename T>
		Acceptor* accept_local(T* param, void (T::*callback)(AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa = 0)
		{
			ThunkA<T,AsyncLocalSocket> thunk = { param, callback };
			return accept_local(thunk,&ThunkA<T,AsyncLocalSocket>::callback,path,perr,psa);
		}
		
		template <typename T>
		Acceptor* accept_remote(T* param, void (T::*callback)(AsyncSocket* handler, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err)
		{
			ThunkA<T,AsyncSocket> thunk = { param, callback };
			return accept_remote(thunk,&ThunkA<T,AsyncSocket>::callback,addr,addr_len,perr);
		}

		virtual AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
		virtual AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

		virtual AsyncSocket* connect_socket(const struct sockaddr* addr, size_t addr_len, int& err, const OOBase::timeval_t* timeout = 0);
		virtual AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout = 0);

	protected:
		explicit Proactor(bool);

		virtual Acceptor* accept_local(void* param, void (*callback)(void* param, AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa);
		virtual Acceptor* accept_remote(void* param, void (*callback)(void* param, AsyncSocket* pSocket, const struct sockaddr* addr, size_t addr_len, int err), const struct sockaddr* addr, size_t addr_len, int& err);

	private:
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);

		Proactor* m_impl;

		template <typename T, typename S>
		struct ThunkA
		{
			T* m_param;
			void (T::*m_callback)(S* pSocket, const char* strAddress, int err);
		
			static void callback(void* param, S* pSocket, const char* strAddress, int err)
			{
				ThunkA thunk = *static_cast<ThunkA*>(param);
				delete static_cast<ThunkA*>(param);
				
				thunk.m_param->(*thunk.m_callback)(pSocket,strAddress,err);
			}
		};
	};
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
