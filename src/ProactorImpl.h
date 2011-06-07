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

#include "../include/OOBase/CustomNew.h"
#include "../include/OOBase/TimeVal.h"
#include "../include/OOBase/Condition.h"

#include <queue>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4250) // Moans about virtual base classes
#endif

namespace OOSvrBase
{
	namespace detail
	{
		class AsyncHandlerRaw;

		class AsyncIOHelper
		{
		public:
			virtual ~AsyncIOHelper()
			{}

			void bind_handler(AsyncHandlerRaw* handler)
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
			virtual void close() = 0;
			virtual void shutdown(bool bSend, bool bRecv) = 0;
			
		protected:
			AsyncIOHelper() : m_handler(0)
			{}

			AsyncHandlerRaw* m_handler;
		};

		class AsyncHandlerRaw
		{
		public:
			virtual void on_recv(AsyncIOHelper::AsyncOp* recv_op, int err, bool bLast) = 0;
			virtual void on_sent(AsyncIOHelper::AsyncOp* send_op, int err) = 0;
		};

		class AsyncHandler
		{
		public:
			virtual void on_recv(OOBase::Buffer* buffer, int err) = 0;
			virtual void on_sent(OOBase::Buffer* buffer, int err) = 0;
			virtual void on_closed() = 0;
		};

		class AsyncQueued
		{
		public:
			AsyncQueued(bool sender, AsyncIOHelper* helper, AsyncHandler* handler);
			~AsyncQueued();

			int async_op(OOBase::Buffer* buffer, size_t len);
			int sync_op(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout);
			bool notify_async(AsyncIOHelper::AsyncOp* op, int err, bool bLast);
			void shutdown();

			void dispose();
						
		private:
			const bool            m_sender;
			AsyncIOHelper* const  m_helper;
			AsyncHandler*         m_handler;
			
			OOBase::SpinLock                    m_lock;
			bool                                m_closed;

			std::queue<AsyncIOHelper::AsyncOp*> m_ops;
						
			struct BlockingInfo
			{
				OOBase::Event ev;
				int           err;
				bool          cancelled;
			};

			std::vector<AsyncIOHelper::AsyncOp*> m_vecAsyncs;

			int async_op(OOBase::Buffer* buffer, size_t len, BlockingInfo* param);
			void issue_next(OOBase::Guard<OOBase::SpinLock>& guard);
		};

		class AsyncSocketImpl : public AsyncHandlerRaw				
		{
		public:
			AsyncSocketImpl(AsyncIOHelper* helper, AsyncHandler* handler);

			void dispose();
			
			int async_recv(OOBase::Buffer* buffer, size_t len);
			int async_send(OOBase::Buffer* buffer);

			int send(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout);
			int recv(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout);

			void shutdown(bool bSend, bool bRecv);
						
		private:
			void addref();
			void release();

			virtual ~AsyncSocketImpl();

			AsyncQueued            m_receiver;
			AsyncQueued            m_sender;
			OOBase::Atomic<size_t> m_refcount;
			
			void on_recv(AsyncIOHelper::AsyncOp* recv_op, int err, bool bLast);
			void on_sent(AsyncIOHelper::AsyncOp* send_op, int err);
		};

		template <typename SOCKET_TYPE>
		class AsyncSocketTempl : 
				public AsyncHandler,
				public SOCKET_TYPE
		{
		public:
			AsyncSocketTempl(AsyncIOHelper* helper) : 
					m_pImpl(0),
					m_io_handler(0)
			{ 
				m_pImpl = new (OOBase::critical) AsyncSocketImpl(helper,this);
			}

			void close()
			{
				delete this;
			}

			void bind_handler(OOSvrBase::IOHandler* handler)
			{
				assert(!m_io_handler);
				m_io_handler = handler;
			}

			void shutdown(bool bSend, bool bRecv)
			{
				return m_pImpl->shutdown(bSend,bRecv);
			}

			int recv(OOBase::Buffer* buffer, size_t len, const OOBase::timeval_t* timeout = 0)
			{
				return m_pImpl->recv(buffer,len,timeout);
			}

			int send(OOBase::Buffer* buffer, const OOBase::timeval_t* timeout)
			{
				return m_pImpl->send(buffer,timeout);
			}

			int async_recv(OOBase::Buffer* buffer, size_t len = 0)
			{
				assert(m_io_handler);
				return m_pImpl->async_recv(buffer,len);
			}

			int async_send(OOBase::Buffer* buffer)
			{
				assert(m_io_handler);
				return m_pImpl->async_send(buffer);
			}

			void on_recv(OOBase::Buffer* buffer, int err)
			{
				m_io_handler->on_recv(buffer,err);
			}

			void on_sent(OOBase::Buffer* buffer, int err)
			{
				m_io_handler->on_sent(buffer,err);
			}

			void on_closed()
			{
				m_io_handler->on_closed();
			}

		protected:
			virtual ~AsyncSocketTempl()
			{
				m_pImpl->dispose();
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

