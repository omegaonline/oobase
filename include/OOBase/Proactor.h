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
		friend class CDRIO;

	public:
		template <typename T>
		int recv(T* param, void (T::*callback)(Buffer* buffer, int err), Buffer* buffer, size_t bytes = 0)
		{
			ThunkR<T>* thunk = this->thunk_allocate<ThunkR<T>,T*,void (T::*)(Buffer*,int)>(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return recv(thunk,&ThunkR<T>::fn,buffer,bytes);
		}

		template <typename T>
		int recv_msg(T* param, void (T::*callback)(Buffer* data_buffer, Buffer* ctl_buffer, int err), Buffer* data_buffer, Buffer* ctl_buffer, size_t data_bytes = 0)
		{
			ThunkRM<T>* thunk = this->thunk_allocate<ThunkRM<T>,T*,void (T::*)(Buffer*,Buffer*,int)>(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;

			return recv_msg(thunk,&ThunkRM<T>::fn,data_buffer,ctl_buffer,data_bytes);
		}

		template <typename T>
		int send(T* param, void (T::*callback)(Buffer* buffer, int err), Buffer* buffer)
		{
			ThunkS<T>* thunk = this->thunk_allocate<ThunkS<T>,T*,void (T::*)(Buffer*,int)>(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send(thunk,&ThunkS<T>::fn,buffer);
		}

		template <typename T>
		int send_v(T* param, void (T::*callback)(Buffer* buffers[], size_t count, int err), Buffer* buffers[], size_t count)
		{
			ThunkSV<T>* thunk = this->thunk_allocate<ThunkSV<T>,T*,void (T::*)(Buffer*[],size_t, int)>(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;
			
			return send_v(thunk,&ThunkSV<T>::fn,buffers,count);
		}

		// These are blocking calls
		int recv(Buffer* buffer, size_t bytes = 0);
		int send(Buffer* buffer);
		int send_msg(Buffer* buffer, Buffer* ctl_buffer);

		typedef void (*recv_callback_t)(void* param, Buffer* buffer, int err);
		virtual int recv(void* param, recv_callback_t callback, Buffer* buffer, size_t bytes = 0) = 0;

		typedef void (*recv_msg_callback_t)(void* param, Buffer* data_buffer, Buffer* ctl_buffer, int err);
		virtual int recv_msg(void* param, recv_msg_callback_t callback, Buffer* data_buffer, Buffer* ctl_buffer, size_t data_bytes = 0) = 0;

		typedef void (*send_callback_t)(void* param, Buffer* buffer, int err);
		virtual int send(void* param, send_callback_t callback, Buffer* buffer) = 0;

		typedef void (*send_v_callback_t)(void* param, Buffer* buffers[], size_t count, int err);
		virtual int send_v(void* param, send_v_callback_t callback, Buffer* buffers[], size_t count) = 0;

		typedef void (*send_msg_callback_t)(void* param, Buffer* data_buffer, Buffer* ctl_buffer, int err);
		virtual int send_msg(void* param, send_msg_callback_t callback, Buffer* data_buffer, Buffer* ctl_buffer) = 0;

		virtual int shutdown(bool bSend = true, bool bRecv = true) = 0;

	protected:
		AsyncSocket() {}
		virtual ~AsyncSocket() {}

		template <typename TThunk, typename TP, typename TC>
		TThunk* thunk_allocate(TP param, TC callback)
		{
			AllocatorInstance& allocator = get_allocator();
			return allocator.allocate_new<TThunk>(param,callback,allocator);
		}

		virtual AllocatorInstance& get_allocator() = 0;

	private:
		template <typename T>
		struct ThunkR
		{
			ThunkR(T* param, void (T::*callback)(Buffer*,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(Buffer*,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, Buffer* buffer, int err)
			{
				ThunkR thunk = *static_cast<ThunkR*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkR*>(param));
				(thunk.m_param->*thunk.m_callback)(buffer,err);
			}
		};
		
		template <typename T>
		struct ThunkRM
		{
			ThunkRM(T* param, void (T::*callback)(Buffer*,Buffer*,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(Buffer*,Buffer*,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, Buffer* data_buffer, Buffer* ctl_buffer, int err)
			{
				ThunkRM thunk = *static_cast<ThunkRM*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkRM*>(param));
				(thunk.m_param->*thunk.m_callback)(data_buffer,ctl_buffer,err);
			}
		};

		template <typename T>
		struct ThunkS
		{
			ThunkS(T* param, void (T::*callback)(Buffer*,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(Buffer*,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, Buffer* buffer, int err)
			{
				ThunkS thunk = *static_cast<ThunkS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkS*>(param));
				(thunk.m_param->*thunk.m_callback)(buffer,err);
			}
		};

		template <typename T>
		struct ThunkSV
		{
			ThunkSV(T* param, void (T::*callback)(Buffer*[],size_t,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(Buffer*[],size_t,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, Buffer* buffers[], size_t count, int err)
			{
				ThunkSV thunk = *static_cast<ThunkSV*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkSV*>(param));
				(thunk.m_param->*thunk.m_callback)(buffers,count,err);
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
		virtual Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa, AllocatorInstance& allocator) = 0;
		Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa = NULL)
		{
			return accept(param,callback,path,err,psa,get_internal_allocator());
		}

		typedef void (*accept_callback_t)(void* param, AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		virtual Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err, AllocatorInstance& allocator) = 0;
		Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err)
		{
			return accept(param,callback,addr,addr_len,err,get_internal_allocator());
		}

		virtual AsyncSocket* attach(socket_t sock, int& err, AllocatorInstance& allocator) = 0;
		AsyncSocket* attach(socket_t sock, int& err)
		{
			return attach(sock,err,get_internal_allocator());
		}
#if defined(_WIN32)
		virtual AsyncSocket* attach(HANDLE hPipe, int& err, AllocatorInstance& allocator) = 0;
		AsyncSocket* attach(HANDLE hPipe, int& err)
		{
			return attach(hPipe,err,get_internal_allocator());
		}
#endif

		virtual AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout, AllocatorInstance& allocator) = 0;
		AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout = Timeout())
		{
			return connect(addr,addr_len,err,timeout,get_internal_allocator());
		}
		virtual AsyncSocket* connect(const char* path, int& err, const Timeout& timeout, AllocatorInstance& allocator) = 0;
		AsyncSocket* connect(const char* path, int& err, const Timeout& timeout = Timeout())
		{
			return connect(path,err,timeout,get_internal_allocator());
		}

		// Returns -1 on error, 0 on timeout, 1 on nothing more to do
		virtual int run(int& err, const Timeout& timeout = Timeout()) = 0;
		virtual void stop() = 0;
		virtual int restart() = 0;

		AllocatorInstance& get_internal_allocator()
		{
			return m_allocator;
		}

	protected:
		Proactor() : m_allocator(this) {}
		virtual ~Proactor() {}

		virtual void* internal_allocate(size_t bytes, size_t align) = 0;
		virtual void* internal_reallocate(void* ptr, size_t bytes, size_t align) = 0;
		virtual void internal_free(void* ptr) = 0;

	private:
		// Don't copy or assign proactors, they're just too big
		Proactor(const Proactor&);
		Proactor& operator = (const Proactor&);

		class InternalAllocator : public AllocatorInstance
		{
		public:
			InternalAllocator(Proactor* p) : m_pProactor(p)
			{}

			void* allocate(size_t bytes, size_t align)
			{
				return m_pProactor->internal_allocate(bytes,align);
			}

			void* reallocate(void* ptr, size_t bytes, size_t align)
			{
				return m_pProactor->internal_reallocate(ptr,bytes,align);
			}

			void free(void* ptr)
			{
				m_pProactor->internal_free(ptr);
			}

			Proactor* m_pProactor;
		};
		friend class InternalAllocator;

		InternalAllocator m_allocator;
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
