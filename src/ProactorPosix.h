///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#ifndef OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_
#define OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_

#include "../include/OOSvrBase/Proactor.h"

#if defined(HAVE_UNISTD_H) && !defined(_WIN32)

#include "../include/OOBase/Condition.h"
#include "../include/OOBase/Set.h"
#include "../include/OOBase/Stack.h"

namespace OOSvrBase
{
	namespace detail
	{
		enum TxDirection
		{
			eTXRecv = 1,
			eTXSend = 2
		};

		class ProactorPosix : public Proactor
		{
		// Proactor public members
		public:
			Acceptor* accept_local(void* param, accept_local_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			Acceptor* accept_remote(void* param, accept_remote_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

			AsyncSocket* attach_socket(OOBase::socket_t sock, int& err);
			AsyncLocalSocket* attach_local_socket(OOBase::socket_t sock, int& err);

			AsyncSocket* connect_socket(const sockaddr* addr, socklen_t addr_len, int& err, const OOBase::Timeout& timeout = OOBase::Timeout());
			AsyncLocalSocket* connect_local_socket(const char* path, int& err, const OOBase::Timeout& timeout = OOBase::Timeout());

		// 'Internal' public members
		public:
			typedef void (*fd_callback_t)(int fd, void* param, unsigned int events);

			int bind_fd(int fd, void* param, fd_callback_t callback);
			int unbind_fd(int fd);

			int watch_fd(int fd, unsigned int events);

			typedef OOBase::Timeout (*timer_callback_t)(void* param);

			int start_timer(void* param, timer_callback_t callback, const OOBase::Timeout& timeout);
			int stop_timer(void* param);

			void stop();

		protected:
			struct TimerItem
			{
				void*            m_param;
				timer_callback_t m_callback;
				OOBase::Timeout  m_timeout;

				bool operator < (const TimerItem& rhs) const
				{
					// Reversed sort order
					return rhs.m_timeout < m_timeout;
				}
			};

			ProactorPosix();
			virtual ~ProactorPosix();

			int init();

			int read_control();

			bool check_timers(TimerItem& active_timer, OOBase::Timeout& timeout);
			int process_timer(const TimerItem& active_timer);

			virtual int do_bind_fd(int fd, void* param, fd_callback_t callback) = 0;
			virtual int do_watch_fd(int fd, unsigned int events) = 0;
			virtual int do_unbind_fd(int fd) = 0;

			OOBase::SpinLock       m_lock;
			bool                   m_stopped;
			int                    m_read_fd;

		private:
			OOBase::Set<TimerItem> m_timers;
			int                    m_write_fd;

			int add_timer(void* param, timer_callback_t callback, const OOBase::Timeout& timeout);
			int remove_timer(void* param);
			int watch_fd_i(int fd, unsigned int events, OOBase::Future<int>* future);
		};
	}
}

#endif // defined(HAVE_UNISTD_H) && !defined(_WIN32)

#endif // OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_
