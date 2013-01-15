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

namespace OOBase
{
	class AsyncSocket : public RefCounted
	{
	public:
		template <typename T>
		int recv(T* param, void (T::*callback)(Buffer* buffer, int err), Buffer* buffer, size_t bytes = 0)
		{
			ThunkR<T>* thunk = ThunkR<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return recv(thunk,&ThunkR<T>::fn,buffer,bytes);
		}

		template <typename T>
		int send(T* param, void (T::*callback)(int err), Buffer* buffer)
		{
			ThunkS<T>* thunk = ThunkS<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send(thunk,&ThunkS<T>::fn,buffer);
		}

		template <typename T>
		int send_v(T* param, void (T::*callback)(int err), Buffer* buffers[], size_t count)
		{
			ThunkS<T>* thunk = ThunkS<T>::create(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send_v(thunk,&ThunkS<T>::fn,buffers,count);
		}

		int recv(Buffer* buffer, size_t bytes = 0);
		int send(Buffer* buffer);

		typedef void (*recv_callback_t)(void* param, Buffer* buffer, int err);
		virtual int recv(void* param, recv_callback_t callback, Buffer* buffer, size_t bytes = 0) = 0;

		typedef void (*recv_msg_callback_t)(void* param, Buffer* data_buffer, Buffer* ctl_buffer, int err);
		virtual int recv_msg(void* param, recv_msg_callback_t callback, Buffer* data_buffer, Buffer* ctl_buffer, size_t data_bytes = 0) = 0;

		typedef void (*send_callback_t)(void* param, int err);
		virtual int send(void* param, send_callback_t callback, Buffer* buffer) = 0;
		virtual int send_v(void* param, send_callback_t callback, Buffer* buffers[], size_t count) = 0;
		virtual int send_msg(void* param, send_callback_t callback, Buffer* data_buffer, Buffer* ctl_buffer) = 0;

	protected:
		AsyncSocket() {}
		virtual ~AsyncSocket() {}

	private:
		template <typename T>
		struct ThunkR
		{
			T* m_param;
			void (T::*m_callback)(Buffer* buffer, int err);

			static ThunkR* create(T* param, void (T::*callback)(Buffer*,int))
			{
				ThunkR* t = static_cast<ThunkR*>(CrtAllocator::allocate(sizeof(ThunkR)));
				if (t)
				{
					t->m_callback = callback;
					t->m_param = param;
				}
				return t;
			}
		
			static void fn(void* param, Buffer* buffer, int err)
			{
				ThunkR thunk = *static_cast<ThunkR*>(param);
				CrtAllocator::free(param);
				
				(thunk.m_param->*thunk.m_callback)(buffer,err);
			}
		};
		
		template <typename T>
		struct ThunkS
		{
			T* m_param;
			void (T::*m_callback)(int err);

			static ThunkS* create(T* param, void (T::*callback)(int))
			{
				ThunkS* t = static_cast<ThunkS*>(CrtAllocator::allocate(sizeof(ThunkS)));
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
				CrtAllocator::free(param);
				
				(thunk.m_param->*thunk.m_callback)(err);
			}
		};
	};

	class Acceptor : public RefCounted
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

		typedef void (*accept_pipe_callback_t)(void* param, AsyncSocket* pSocket, int err);
		virtual Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa = NULL) = 0;

		typedef void (*accept_callback_t)(void* param, AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		virtual Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err) = 0;

		virtual AsyncSocket* attach(socket_t sock, int& err) = 0;
#if defined(_WIN32)
		virtual AsyncSocket* attach(HANDLE hPipe, int& err) = 0;
#endif

		virtual AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout = Timeout()) = 0;
		virtual AsyncSocket* connect(const char* path, int& err, const Timeout& timeout = Timeout()) = 0;

		// Returns -1 on error, 0 on timeout, 1 on nothing more to do
		virtual int run(int& err, const Timeout& timeout = Timeout()) = 0;
		virtual void stop() = 0;

	protected:
		Proactor() {}
		virtual ~Proactor() {}

	private:
		// Don't copy or assign proactors, they're just too big
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);
	};

	template <typename LibraryType>
	class Singleton<Proactor,LibraryType>
	{
	public:
		static Proactor* instance_ptr()
		{
			static Once::once_t key = ONCE_T_INIT;
			Once::Run(&key,init);

			return s_instance;
		}

		static Proactor& instance()
		{
			Proactor* i = instance_ptr();
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

		static Proactor* s_instance;

		static void init()
		{
			int err = 0;
			Proactor* i = Proactor::create(err);
			if (err)
				OOBase_CallCriticalFailure(err);

			err = DLLDestructor<LibraryType>::add_destructor(&destroy,i);
			if (err)
			{
				Proactor::destroy(i);
				OOBase_CallCriticalFailure(err);
			}

			s_instance = i;
		}

		static void destroy(void* i)
		{
			if (i == s_instance)
			{
				Proactor::destroy(static_cast<Proactor*>(i));
				s_instance = NULL;
			}
		}
	};

	template <typename LibraryType>
	Proactor* Singleton<Proactor,LibraryType>::s_instance = NULL;
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
