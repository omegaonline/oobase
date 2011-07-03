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
			void close_watcher(int fd);
			int start_watcher(int fd);
			
			int bind_fd(int fd);
			int unbind_fd(int fd);
				
			int recv(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer, size_t bytes, const OOBase::timeval_t* timeout);
			int send(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffer, int err), OOBase::Buffer* buffer);
			int send_v(int fd, void* param, void (*callback)(void* param, OOBase::Buffer* buffers[], size_t count, int err), OOBase::Buffer* buffers[], size_t count);
		
			int run(int& err, const OOBase::timeval_t* timeout = NULL);
		
		private:
			OOBase::SpinLock m_lock;
			ev_loop*         m_pLoop;
			ev_io            m_pipe_watcher;
			int              m_pipe_fds[2];
		
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
			
			struct Watcher : public ev_io
			{
				void (*m_callback)(int fd, int events, void* param);
				void* m_param;
			};
					
			struct IOWatcher : public ev_io
			{
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueSend;
				OOBase::Queue<IOOp,OOBase::HeapAllocator> m_queueRecv;
				int                                       m_closed;
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
			
			OOBase::Mutex                                       m_ev_lock;
			OOBase::HashTable<int,ev_io*,OOBase::HeapAllocator> m_mapWatchers;
			OOBase::Queue<IOEvent,OOBase::LocalAllocator>*      m_pIOQueue;
					
			static void pipe_callback(ev_loop*, ev_io* watcher, int);
			static void watcher_callback(ev_loop* pLoop, ev_io* watcher, int revents);
			static void iowatcher_callback(ev_loop* pLoop, ev_io* watcher, int revents);
			
			void pipe_callback();
			void process_event(Watcher* watcher, int fd, int revents);
			void process_event(IOWatcher* watcher, int fd, int revents);
		};
	}
}

#endif // defined(HAVE_EV_H)

#endif // OOSVRBASE_PROACTOR_EV_H_INCLUDED_
