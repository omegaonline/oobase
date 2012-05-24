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
#include "../OOBase/Singleton.h"

#if !defined(_WIN32)
typedef struct
{
	mode_t mode;
} SECURITY_ATTRIBUTES;
#endif

namespace OOSvrBase
{
	class AsyncSocket : public OOBase::RefCounted<OOBase::HeapAllocator>
	{
	public:
		template <typename T>
		int recv(T* param, void (T::*callback)(OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes)
		{
			ThunkR<T>* thunk = ThunkR<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return recv(thunk,&ThunkR<T>::fn,buffer,bytes);
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

		int recv(OOBase::Buffer* buffer, size_t bytes);
		int send(OOBase::Buffer* buffer);

		typedef void (*recv_callback_t)(void* param, OOBase::Buffer* buffer, int err);
		virtual int recv(void* param, recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes) = 0;

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
	
	class Acceptor : public OOBase::RefCounted<OOBase::HeapAllocator>
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
		// Factory creation functions
		static Proactor* create(int& err);
		static void destroy(Proactor* proactor);

		typedef void (*accept_local_callback_t)(void* param, AsyncLocalSocket* pSocket, int err);
		virtual Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa = NULL) = 0;

		typedef void (*accept_remote_callback_t)(void* param, AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		virtual Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err) = 0;

		virtual AsyncSocket* attach_socket(OOBase::socket_t sock, int& err) = 0;
		virtual AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err) = 0;

		virtual AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::Timeout& timeout = OOBase::Timeout()) = 0;
		virtual AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::Timeout& timeout = OOBase::Timeout()) = 0;

		virtual int run(int& err, const OOBase::Timeout& timeout = OOBase::Timeout()) = 0;
		virtual void stop() = 0;

	protected:
		Proactor() {}
		virtual ~Proactor() {}

	private:
		// Don't copy or assign proactors, they're just too big
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);
	};
}

namespace OOBase
{
	template <typename DLL>
	class Singleton<OOSvrBase::Proactor,DLL>
	{
	public:
		static OOSvrBase::Proactor* instance_ptr()
		{
			static Once::once_t key = ONCE_T_INIT;
			Once::Run(&key,init);

			return s_instance;
		}

		static OOSvrBase::Proactor& instance()
		{
			OOSvrBase::Proactor* i = instance_ptr();
			if (!i)
				OOBase_CallCriticalFailure("Null instance pointer");

			return *i;
		}

	private:
		// Prevent creation
		Singleton();
		Singleton(const Singleton&);
		Singleton& operator = (const Singleton&);
		~Singleton();

		static OOSvrBase::Proactor* s_instance;

		static void init()
		{
			int err = 0;
			OOSvrBase::Proactor* i = OOSvrBase::Proactor::create(err);
			if (err)
				OOBase_CallCriticalFailure(err);

			err = DLLDestructor<DLL>::add_destructor(&destroy,i);
			if (err)
			{
				OOSvrBase::Proactor::destroy(i);
				OOBase_CallCriticalFailure(err);
			}

			s_instance = i;
		}

		static void destroy(void* i)
		{
			if (i == s_instance)
			{
				OOSvrBase::Proactor::destroy(static_cast<OOSvrBase::Proactor*>(i));
				s_instance = NULL;
			}
		}
	};

	template <typename DLL>
	OOSvrBase::Proactor* Singleton<OOSvrBase::Proactor,DLL>::s_instance = NULL;
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
