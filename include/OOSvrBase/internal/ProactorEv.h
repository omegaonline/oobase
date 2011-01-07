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

#if !defined(OOSVRBASE_PROACTOR_H_INCLUDED_)
#error include "Proactor.h" instead
#endif

#if !defined(HAVE_EV_H)
#error Includes have got confused, check Proactor.cpp
#endif

#include <ev.h>

#include <deque>

#include "../../OOBase/Thread.h"
#include "../../OOBase/Condition.h"
#include "../../OOBase/SmartPtr.h"

namespace OOSvrBase
{
	namespace Ev
	{
		typedef struct ev_loop ev_loop_t;

		class ProactorImpl : public Proactor
		{
		public:
			ProactorImpl();
			~ProactorImpl();

			OOBase::Socket* accept_local(Acceptor<AsyncLocalSocket>* handler, const std::string& path, int* perr, SECURITY_ATTRIBUTES* psa);
			OOBase::Socket* accept_remote(Acceptor<AsyncSocket>* handler, const std::string& address, const std::string& port, int* perr);

			AsyncSocketPtr attach_socket(OOBase::Socket::socket_t sock, int* perr);
			AsyncLocalSocket* attach_local_socket(OOBase::Socket::socket_t sock, int* perr);

			AsyncLocalSocketPtr connect_local_socket(const std::string& path, int* perr, const OOBase::timeval_t* wait);

			struct io_watcher : public ev_io
			{
				void (*callback)(io_watcher*);
			};

			void init_watcher(io_watcher* watcher, int fd, int events);
			void start_watcher(io_watcher* watcher);
			void stop_watcher(io_watcher* watcher);
						
		private:
			typedef std::deque<io_watcher*,OOBase::CriticalAllocator<io_watcher*> > dequeType;

			// The following vars all use this lock
			OOBase::Mutex m_ev_lock;
			ev_loop_t*    m_pLoop;
			dequeType*    m_pIOQueue;
			bool          m_bAsyncTriggered;

			// The following vars all use this lock
			OOBase::SpinLock m_lock;
			bool             m_bStop;
			dequeType        m_start_queue;
			dequeType        m_stop_queue;

			// The following vars don't...
			typedef std::deque<OOBase::SmartPtr<OOBase::Thread>,OOBase::CriticalAllocator<OOBase::SmartPtr<OOBase::Thread> > > workerDeque;
			workerDeque m_workers;
			ev_async    m_alert;

			static int worker(void* param);
			int worker_i();

			static void on_alert(ev_loop_t*, ev_async* watcher, int);
			static void on_io(ev_loop_t*, ev_io* watcher, int events);
			void on_io_i(io_watcher* watcher, int events);
		};
	}
}

#endif // OOSVRBASE_PROACTOR_EV_H_INCLUDED_
