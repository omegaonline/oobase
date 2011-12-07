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

#include "../OOBase/Memory.h"
#include "../OOBase/Socket.h"

#if !defined(_WIN32)
typedef struct
{
	mode_t mode;
} SECURITY_ATTRIBUTES;
#endif

namespace OOSvrBase
{
	class AsyncSocket : public OOBase::RefCounted
	{
	public:
		template <typename T>
		int recv(T* param, void (T::*callback)(OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout = NULL)
		{
			ThunkR<T>* thunk = ThunkR<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return recv(thunk,&ThunkR<T>::fn,buffer,bytes,timeout);
		}

		template <typename T>
		int send(T* param, void (T::*callback)(int err), OOBase::Buffer* buffer)
		{
			ThunkS<T>* thunk = ThunkS<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send(thunk,&ThunkS<T>::fn,buffer);
		}

		template <typename T>
		int send_v(T* param, void (T::*callback)(int err), OOBase::Buffer* buffers[], size_t count)
		{
			ThunkS<T>* thunk = ThunkS<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send_v(thunk,&ThunkS<T>::fn,buffers,count);
		}

		int recv(OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout = NULL);
		int send(OOBase::Buffer* buffer);

		typedef void (*recv_callback_t)(void* param, OOBase::Buffer* buffer, int err);
		virtual int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout = NULL) = 0;

		typedef void (*send_callback_t)(void* param, int err);
		virtual int send(void* param, send_callback_t callback, OOBase::Buffer* buffer) = 0;
		virtual int send_v(void* param, send_callback_t callback, OOBase::Buffer* buffers[], size_t count) = 0;

	protected:
		AsyncSocket() {}
		virtual ~AsyncSocket() {}

	private:
		template <typename T>
		struct ThunkR : public OOBase::CustomNew<OOBase::HeapAllocator>
		{
			T* m_param;
			void (T::*m_callback)(OOBase::Buffer* buffer, int err);

			static ThunkR* create(T* param, void (T::*callback)(OOBase::Buffer*,int))
			{
				ThunkR* t = static_cast<ThunkR*>(OOBase::HeapAllocator::allocate(sizeof(ThunkR)));
				if (t)
				{
					t->m_callback = callback;
					t->m_param = param;
				}
				return t;
			}
		
			static void fn(void* param, OOBase::Buffer* buffer, int err)
			{
				ThunkR thunk = *static_cast<ThunkR*>(param);
				OOBase::HeapAllocator::free(param);
				
				(thunk.m_param->*thunk.m_callback)(buffer,err);
			}
		};
		
		template <typename T>
		struct ThunkS : public OOBase::CustomNew<OOBase::HeapAllocator>
		{
			T* m_param;
			void (T::*m_callback)(int err);

			static ThunkS* create(T* param, void (T::*callback)(int))
			{
				ThunkS* t = static_cast<ThunkS*>(OOBase::HeapAllocator::allocate(sizeof(ThunkS)));
				if (t)
				{
					t->m_callback = callback;
					t->m_param = param;
				}
				return t;
			}
		
			static void fn(void* param, int err)
			{
				ThunkS thunk = *static_cast<ThunkS*>(param);
				OOBase::HeapAllocator::free(param);
				
				(thunk.m_param->*thunk.m_callback)(err);
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

	protected:
		AsyncLocalSocket() {}
		virtual ~AsyncLocalSocket() {}
	};
	
	class Acceptor : public OOBase::RefCounted
	{
	public:
		// No members, just release() to close
			
	protected:
		Acceptor() {}
		virtual ~Acceptor() {}
	};

	class Proactor
	{
	public:
		Proactor();
		virtual ~Proactor();

		typedef void (*accept_local_callback_t)(void* param, AsyncLocalSocket* pSocket, int err);
		virtual Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);

		typedef void (*accept_remote_callback_t)(void* param, AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		virtual Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

		virtual AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
		virtual AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

		virtual AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::timeval_t* timeout = NULL);
		virtual AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout = NULL);

		virtual int run(int& err, const OOBase::timeval_t* timeout = NULL);
		virtual void stop();

	protected:
		explicit Proactor(bool);

		virtual int init();

	private:
		// Don't copy or assign proactors, they're just too big
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);

		Proactor* m_impl;
	};
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
