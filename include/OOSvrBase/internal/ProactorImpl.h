///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#ifndef OOSVRBASE_PROACTOR_IMPL_H_INCLUDED_
#define OOSVRBASE_PROACTOR_IMPL_H_INCLUDED_

#include "../../OOBase/TimeVal.h"
#include "../../OOBase/Condition.h"

#include <queue>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4250) // Moans about virtual base classes
#endif

namespace OOSvrBase
{
	namespace detail
	{
		class AsyncHandler;

		class AsyncIOHelper
		{
		public:
			virtual ~AsyncIOHelper()
			{}

			void bind_handler(AsyncHandler* handler)
			{
				m_handler = handler;
			}

			struct AsyncOp
			{
				OOBase::Buffer* buffer;
				size_t          len;
				void*           param;
			};

			virtual void recv(AsyncOp* recv_op) = 0;
			virtual void send(AsyncOp* send_op) = 0;
			virtual void shutdown() = 0;
			
		protected:
			AsyncIOHelper() : m_handler(0)
			{}

			AsyncHandler* m_handler;
		};

		class AsyncHandler
		{
		public:
			virtual void on_recv(AsyncIOHelper::AsyncOp* recv_op, int err) = 0;
			virtual void on_sent(AsyncIOHelper::AsyncOp* send_op, int err) = 0;
		};

		class AsyncSocketImpl;

		class AsyncQueued
		{
		public:
			AsyncQueued(bool sender, AsyncSocketImpl* parent);
			~AsyncQueued();

			void close();
			int async_op(OOBase::Buffer* buffer, size_t len);
			int sync_op(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout);
			void notify_async(AsyncIOHelper::AsyncOp* op, int err, void* param, void (*callback)(void*,OOBase::Buffer*,int));
			
		private:
			bool             m_sender;
			AsyncSocketImpl* m_parent;
			OOBase::SpinLock m_lock;

			std::queue<AsyncIOHelper::AsyncOp*> m_ops;
			
			struct BlockingInfo
			{
				OOBase::Event ev;
				int           err;
				bool          cancelled;
			};

			int async_op(OOBase::Buffer* buffer, size_t len, BlockingInfo* param);
			void issue_next();
		};

		class AsyncSocketImpl : public AsyncHandler				
		{
			friend class AsyncQueued;

		public:
			AsyncSocketImpl(AsyncIOHelper* helper);
			
			int async_recv(OOBase::Buffer* buffer, size_t len);
			int async_send(OOBase::Buffer* buffer);

			int send(const void* buf, size_t len, const OOBase::timeval_t* timeout);
			int send_buffer(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout);

			size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout);
			int recv_buffer(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout);

			void drop();

		protected:
			virtual void on_async_recv(OOBase::Buffer* buffer, int err) = 0;
			virtual void on_async_sent(OOBase::Buffer* buffer, int err) = 0;
			virtual void on_async_closed() = 0;
			virtual bool is_close(int err) const = 0;

		private:
			void addref();
			void release();

			~AsyncSocketImpl();

			AsyncQueued               m_receiver;
			AsyncQueued               m_sender;
			OOBase::AtomicInt<size_t> m_close_count;

			void on_recv(AsyncIOHelper::AsyncOp* recv_op, int err);
			void on_sent(AsyncIOHelper::AsyncOp* send_op, int err);
			void on_closed();

			static void on_async_recv_i(void* param, OOBase::Buffer* buffer, int err);
			static void on_async_sent_i(void* param, OOBase::Buffer* buffer, int err);
		};

		template <typename SOCKET_TYPE>
		class AsyncSocketTempl : public SOCKET_TYPE
		{
		public:
			AsyncSocketTempl(AsyncIOHelper* helper) : 
					m_pImpl(0),
					m_io_handler(0)
			{
				AsyncSocketImpl
			}

			virtual ~AsyncSocketTempl()
			{
				m_pImpl->release();
			}

			void bind_handler(OOSvrBase::IOHandler* handler)
			{
				assert(!m_io_handler);
				m_io_handler = handler;
			}

			size_t recv(void* buf, size_t len, int* perr, const OOBase::timeval_t* timeout = 0)
			{
				return AsyncSocketImpl::recv(buf,len,perr,timeout);
			}

			int send(const void* buffer, size_t len, const OOBase::timeval_t* timeout = 0)
			{
				return AsyncSocketImpl::send(buffer,len,timeout);
			}

			int send_buffer(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout)
			{
				return AsyncSocketImpl::send_buffer(buffer,timeout);
			}

			void close()
			{
				return AsyncSocketImpl::close();
			}

			int async_recv(OOBase::Buffer* buffer, size_t len = 0)
			{
				assert(m_io_handler);
				return AsyncSocketImpl::async_recv(buffer,len);
			}

			int async_send(OOBase::Buffer* buffer)
			{
				assert(m_io_handler);
				return AsyncSocketImpl::async_send(buffer);
			}

			void on_async_recv(OOBase::Buffer* buffer, int err)
			{
				m_io_handler->on_recv(this,buffer,err);
			}

			void on_async_sent(OOBase::Buffer* buffer, int err)
			{
				m_io_handler->on_sent(this,buffer,err);
			}

			void on_async_closed()
			{
				m_io_handler->on_closed(this);
			}

		private:
			AsyncSocketImpl*      m_pImpl;
			OOSvrBase::IOHandler* m_io_handler;
		};
	}
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif // OOSVRBASE_PROACTOR_IMPL_H_INCLUDED_

