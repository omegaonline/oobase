///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#ifndef OOBASE_CDR_IO_H_INCLUDED_
#define OOBASE_CDR_IO_H_INCLUDED_

#include "Proactor.h"
#include "CDRStream.h"

namespace OOBase
{
	// This is a class purely to allow friends
	class CDRIO
	{
	public:
		template <typename H, typename S>
		static int recv_with_header_blocking(CDRStream& stream, S pSocket)
		{
			int err = pSocket->recv(stream.buffer(),sizeof(H));
			if (!err)
			{
				H msg_len = 0;
				if (!stream.read(msg_len))
					err = stream.last_error();
				else if (msg_len > sizeof(H))
					err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
			}
			return err;
		}

		template <typename H, typename T>
		static int recv_with_header_sync(H expected_len, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, int err))
		{
			CDRStream stream(expected_len);
			if (stream.last_error())
				return stream.last_error();

			ThunkRHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->recv(thunk,&ThunkRHS<T,H>::fn1,stream.buffer(),sizeof(H));
		}

		template <typename H, typename T>
		static int recv_msg_with_header_sync(H expected_len, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, Buffer* ctl_buffer, int err), Buffer* ctl_buffer)
		{
			CDRStream stream(expected_len);
			if (stream.last_error())
				return stream.last_error();

			ThunkRMHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->recv_msg(thunk,&ThunkRMHS<T,H>::fn1,stream.buffer(),ctl_buffer,sizeof(H));
		}

		template <typename H, typename S>
		static int send_and_recv_with_header_blocking(CDRStream& stream, S pSocket)
		{
			int err = pSocket->send(stream.buffer());
			if (!err)
			{
				stream.reset();
				err = pSocket->recv(stream.buffer(),sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_and_recv_msg_with_header_blocking(CDRStream& stream, Buffer* ctl_buffer, S pSocket)
		{
			int err = pSocket->send(stream.buffer());
			if (!err)
			{
				stream.reset();
				err = pSocket->recv_msg(stream.buffer(),ctl_buffer,sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_msg_and_recv_with_header_blocking(CDRStream& stream, Buffer* ctl_buffer, S pSocket)
		{
			int err = pSocket->send_msg(stream.buffer(),ctl_buffer);
			if (!err)
			{
				stream.reset();
				err = pSocket->recv(stream.buffer(),sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_msg_and_recv_msg_with_header_blocking(CDRStream& stream, Buffer* ctl_buffer, S pSocket)
		{
			int err = pSocket->send_msg(stream.buffer(),ctl_buffer);
			if (!err)
			{
				stream.reset();
				ctl_buffer->reset();
				err = pSocket->recv_msg(stream.buffer(),ctl_buffer,sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename T>
		static int send_and_recv_with_header_sync(CDRStream& stream, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, int err))
		{
			ThunkSRHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->send(thunk,&ThunkSRHS<T,H>::fn1,stream.buffer());
		}

	private:
		template <typename T, typename H>
		struct ThunkRHS
		{
			ThunkRHS(T* param, void (T::*callback)(CDRStream&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(CDRStream&,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;

			static void fn1(void* param, Buffer* buffer, int err)
			{
				CDRStream stream(buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = static_cast<ThunkRHS*>(param)->m_ptrSocket->recv(param,&fn2,buffer,msg_len - sizeof(H));
					else
						done = true;
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn2(void* param, Buffer* buffer, int err)
			{
				ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkRHS*>(param));
				CDRStream stream(buffer);
				(thunk.m_param->*thunk.m_callback)(stream,err);
			}
		};

		template <typename T, typename H>
		struct ThunkRMHS
		{
			ThunkRMHS(T* param, void (T::*callback)(CDRStream&,Buffer*,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(CDRStream&,Buffer*,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;
			RefPtr<Buffer>      m_ctl_buffer;

			static void fn1(void* param, Buffer* data_buffer, Buffer* ctl_buffer, int err)
			{
				CDRStream stream(data_buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else
					{
						static_cast<ThunkRMHS*>(param)->m_ctl_buffer = ctl_buffer;
						ctl_buffer->addref();

						if (msg_len > sizeof(H))
							err = static_cast<ThunkRMHS*>(param)->m_ptrSocket->recv(param,&fn2,data_buffer,msg_len - sizeof(H));
						else
							done = true;
					}
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkRMHS thunk = *static_cast<ThunkRMHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkRMHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,ctl_buffer,err);
				}
			}

			static void fn2(void* param, Buffer* data_buffer, int err)
			{
				ThunkRMHS thunk = *static_cast<ThunkRMHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkRMHS*>(param));
				CDRStream stream(data_buffer);
				(thunk.m_param->*thunk.m_callback)(stream,thunk.m_ctl_buffer,err);
			}
		};

		template <typename T, typename H>
		struct ThunkSRHS
		{
			ThunkSRHS(T* param, void (T::*callback)(CDRStream&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			T* m_param;
			void (T::*m_callback)(CDRStream&,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;

			static void fn1(void* param, Buffer* buffer, int err)
			{
				CDRStream stream(buffer);
				if (!err)
				{
					err = stream.reset();
					if (!err)
						err = static_cast<ThunkSRHS*>(param)->m_ptrSocket->recv(param,&fn2,buffer,sizeof(H));
				}
				if (err)
				{
					ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn2(void* param, Buffer* buffer, int err)
			{
				CDRStream stream(buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = static_cast<ThunkSRHS*>(param)->m_ptrSocket->recv(param,&fn3,buffer,msg_len - sizeof(H));
					else
						done = true;
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn3(void* param, Buffer* buffer, int err)
			{
				ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
				CDRStream stream(buffer);
				(thunk.m_param->*thunk.m_callback)(stream,err);
			}
		};
	};

	namespace detail
	{
		template <typename T>
		struct non_ref
		{
			typedef T value;
		};

		template <typename T>
		struct non_ref<T&>
		{
			typedef T value;
		};
	}

	template <typename H, typename Allocator = CrtAllocator>
	class AsyncResponseDispatcher : public Allocating<Allocator>
	{
		typedef Allocating<Allocator> baseClass;
	public:
		AsyncResponseDispatcher() : baseClass()
		{}

		AsyncResponseDispatcher(AllocatorInstance& instance) : baseClass(instance)
		{}

		template <typename T>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&), H& handle)
		{
			Delegate0<T>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		template <typename T, typename P1, typename PP1>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&, P1 p1), PP1 p1, H& handle)
		{
			Delegate1<T,P1,PP1>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback,p1))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		template <typename T, typename P1, typename P2, typename PP1, typename PP2>
		int add_response(T* pThis, bool (T::*callback)(OOBase::CDRStream&, P1 p1, P2 p2), PP1 p1, PP2 p2, H& handle)
		{
			Delegate2<T,P1,P2,PP1,PP2>* d = NULL;
			if (!baseClass::allocate_new(d,pThis,callback,p1,p2))
				return ERROR_OUTOFMEMORY;

			int err = add_response(d,handle);
			if (err)
				baseClass::delete_free(d);
			return err;
		}

		void drop_response(H handle)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			DelegateV* resp = NULL;
			if (m_response_table.remove(handle,&resp) && resp)
				resp->destroy(this);
		}

		bool handle_response(OOBase::CDRStream& stream)
		{
			H handle = 0;
			if (!stream.read(handle))
				return false;

			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			DelegateV* resp;
			if (!m_response_table.remove(handle,&resp) || !resp)
				return false;

			guard.release();

			bool ret = resp->call(stream);

			resp->destroy(this);

			return ret;
		}

	private:
		struct DelegateV
		{
			virtual bool call(OOBase::CDRStream& stream) = 0;

			void destroy(Allocating<Allocator>* alloc)
			{
				alloc->delete_free(this);
			}

			virtual ~DelegateV() {}
		};

		template <typename T>
		struct Delegate0 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&);
			T* m_this;
			callback_t m_callback;

			Delegate0(T* pThis, callback_t callback) : m_this(pThis), m_callback(callback)
			{}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream);
			}
		};

		template <typename T, typename P1, typename PP1>
		struct Delegate1 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&, P1 p1);
			T* m_this;
			callback_t m_callback;
			typename detail::non_ref<P1>::value m_p1;

			Delegate1(T* pThis, callback_t callback, PP1 p1) : m_this(pThis), m_callback(callback), m_p1(p1)
			{}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream,m_p1);
			}
		};

		template <typename T, typename P1, typename P2, typename PP1, typename PP2>
		struct Delegate2 : public DelegateV
		{
			typedef bool (T::*callback_t)(OOBase::CDRStream&, P1 p1, P2 p2);
			T* m_this;
			callback_t m_callback;
			typename detail::non_ref<P1>::value m_p1;
			typename detail::non_ref<P2>::value m_p2;

			Delegate2(T* pThis, callback_t callback, PP1 p1, PP2 p2) : m_this(pThis), m_callback(callback), m_p1(p1), m_p2(p2)
			{}

			bool call(OOBase::CDRStream& stream)
			{
				return (m_this->*m_callback)(stream,m_p1,m_p2);
			}
		};

		OOBase::SpinLock                    m_lock;
		HandleTable<H,DelegateV*,Allocator> m_response_table;

		int add_response(DelegateV* delegate, H& handle)
		{
			OOBase::Guard<OOBase::SpinLock> guard(m_lock);

			return m_response_table.insert(delegate,handle);
		}
	};
}

#endif // OOBASE_CDR_IO_H_INCLUDED_
