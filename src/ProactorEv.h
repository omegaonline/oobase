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

#define EV_COMPAT3 0
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

			AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
			AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

			AsyncSocket* connect_socket(const sockaddr* addr, size_t addr_len, int& err, const OOBase::timeval_t* timeout);
			AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::timeval_t* timeout);
				
			int new_watcher(int fd, int events, void* param, void (*callback)(int fd, int events, void* param));
			int close_watcher(int fd);
			int start_watcher(int fd);
			
			int bind_fd(int fd);
			int unbind_fd(int fd);
				
			int recv(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
			int send(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
			int send_v(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);
		
			OOSvrBase::Acceptor* accept_local(void* param, void (*callback)(void* param, OOSvrBase::AsyncLocalSocket* pSocket, int err), const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			OOSvrBase::Acceptor* accept_remote(void* param, void (*callback)(void* param, OOSvrBase::AsyncSocket* pSocket, const sockaddr* addr, size_t addr_len, int err), const sockaddr* addr, size_t addr_len, int& err);

			int run(int& err, const OOBase::timeval_t* timeout = NULL);

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
					void (*m_callback)(void* param, OOBase::Buffer* buffer, int err);
					void (*m_callback_v)(void* param, OOBase::Buffer* buffers[], size_t count, int err);
				};
			};
			
			static bool copy_queue(OOBase::Queue<IOOp,OOBase::HeapAllocator>& dest, OOBase::Queue<IOOp,OOBase::HeapAllocator>& src);

			union param_t
			{
				size_t s;
				void*  p;
			};

		private:
			OOBase::SpinLock m_lock;
			struct ev_loop*  m_pLoop;
			ev_io            m_pipe_watcher;
			int              m_pipe_fds[2];
			bool             m_started;

			struct Watcher : public ev_io
			{
				void (*m_callback)(int fd, int events, void* param);
				void* m_param;
			};
					
			struct IOWatcher : public ev_io
			{
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueSendPending;
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueSend;
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueRecvPending;
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueRecv;
				bool                                      m_busy_recv;
				bool                                      m_busy_send;
				bool                                      m_unbound;
			};
					
			struct IOEvent
			{
				enum
				{
					Watcher,
					IOWatcher,
					Timer
					
				}     m_type;
				int   m_revents;
				int   m_fd;
				void* m_watcher;
			};
			
			OOBase::HashTable<int,ev_io*,OOBase::HeapAllocator> m_mapWatchers;
			OOBase::Queue<IOEvent,OOBase::LocalAllocator>*      m_pIOQueue;
					
			int init();

			static void pipe_callback(struct ev_loop*, ev_io* watcher, int);
			static void watcher_callback(struct ev_loop* pLoop, ev_io* watcher, int revents);
			static void iowatcher_callback(struct ev_loop* pLoop, ev_io* watcher, int revents);
			
			void pipe_callback();
			void process_event(Watcher* watcher, int fd, int revents);
			void process_event(IOWatcher* watcher, int fd, int revents);

			void process_recv(IOWatcher* watcher, int fd);
			void process_send(IOWatcher* watcher, int fd);
		};
	}
}

#endif // defined(HAVE_EV_H)

#endif // OOSVRBASE_PROACTOR_EV_H_INCLUDED_
