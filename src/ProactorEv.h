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

#ifndef OOSVRBASE_PROACTOR_EV_H_INCLUDED_
#define OOSVRBASE_PROACTOR_EV_H_INCLUDED_

#include "../include/OOSvrBase/Proactor.h"

#if defined(HAVE_EV_H) && !defined(_WIN32)

#include "../include/OOBase/HandleTable.h"
#include "../include/OOBase/Queue.h"
#include "../include/OOBase/Condition.h"

#define EV_COMPAT3 1
#define EV_STANDALONE 1
#include <ev.h>

namespace OOSvrBase
{
	namespace detail
	{
		class ProactorEv : public Proactor
		{
		public:
			ProactorEv();
			virtual ~ProactorEv();

			int init();

			AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
			AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

			AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::timeval_t* timeout);
			AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout);
				
			void* new_acceptor(int fd, void* param, bool (*callback)(void* param, int fd), int& err);
			int close_acceptor(void* handle);

			void* bind(int fd, int& err);
			int unbind(void* handle);
			int get_fd(void* handle);
				
			int recv(void* handle, void* param, OOSvrBase::AsyncSocket::recv_callback_t callback, OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
			int send(void* handle, void* param, OOSvrBase::AsyncSocket::send_callback_t callback, OOBase::Buffer* buffer);
			int send_v(void* handle, void* param, OOSvrBase::AsyncSocket::send_callback_t callback, OOBase::Buffer* buffers[], size_t count);
		
			OOSvrBase::Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			OOSvrBase::Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

			int run(int& err, const OOBase::timeval_t* timeout = NULL);
			void stop();

			struct IOOp
			{
				size_t           m_count;
				void*            m_param;
				union
				{
					OOBase::Buffer** m_buffers;
					OOBase::Buffer*  m_buffer;
				};
				union
				{
					OOSvrBase::AsyncSocket::send_callback_t m_send_callback;
					OOSvrBase::AsyncSocket::recv_callback_t m_recv_callback;
				};
			};
			
			static bool copy_queue(OOBase::Queue<IOOp>& dest, OOBase::Queue<IOOp>& src);

		private:
			OOBase::SpinLock m_lock;
			struct ev_loop*  m_pLoop;
			struct ev_io     m_pipe_watcher;
			int              m_pipe_fds[2];
			size_t           m_outstanding;

			struct Wait
			{
				bool                     m_finished;
				OOBase::Condition::Mutex m_mutex;
				OOBase::Condition        m_condition;
			};

			struct AcceptWatcher : public ev_io
			{
				OOBase::Atomic<size_t> m_refcount;
				bool (*m_callback)(void* param, int fd);
				void* m_param;
				Wait* m_wait;
			};
					
			struct IOWatcher : public ev_io
			{
				OOBase::Atomic<size_t> m_refcount;
				OOBase::Queue<IOOp>    m_queueSendPending;
				OOBase::Queue<IOOp>    m_queueSend;
				OOBase::Queue<IOOp>    m_queueRecvPending;
				OOBase::Queue<IOOp>    m_queueRecv;
				bool                   m_busy_recv;
				bool                   m_busy_send;
			};
					
			struct IOEvent
			{
				enum
				{
					Acceptor,
					IOWatcher,
					Timer
					
				}     m_type;
				int   m_revents;
				int   m_fd;
				void* m_watcher;
			};
			
			OOBase::Queue<IOEvent,OOBase::LocalAllocator>* m_pIOQueue;
					
			bool deref(AcceptWatcher* watcher);
			bool deref(IOWatcher* watcher);

			void io_start(struct ev_io* io);
			void io_stop(struct ev_io* io);

			void pipe_callback();

			static void pipe_callback(struct ev_loop*, struct ev_io* watcher, int);
			static void acceptor_callback(struct ev_loop* pLoop, struct ev_io* watcher, int revents);
			static void iowatcher_callback(struct ev_loop* pLoop, struct ev_io* watcher, int revents);
			static void process_accept(int send_fd, AcceptWatcher* watcher);
			static void process_event(int send_fd, IOWatcher* watcher, int fd, int revents);
			static void process_recv(int send_fd, IOWatcher* watcher, int fd);
			static void process_send(int send_fd, IOWatcher* watcher, int fd);
		};
	}
}

#endif // defined(HAVE_EV_H)

#endif // OOSVRBASE_PROACTOR_EV_H_INCLUDED_
