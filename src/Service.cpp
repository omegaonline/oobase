///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#include "../include/OOBase/Logger.h"
#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Posix.h"

#include "../include/OOBase/Service.h"

#include "Win32Impl.h"

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#endif

#if defined(_WIN32)

namespace OOBase
{
	// The discrimination type for singleton scoping for this module
	struct Module
	{
		int unused;
	};
}

namespace
{
	struct QuitData
	{
		QuitData();

		void signal(int how);
		int wait();
		int get_result();

	private:
		OOBase::Condition::Mutex m_lock;
		OOBase::Condition        m_condition;
		int                      m_result;
	};
	typedef OOBase::Singleton<QuitData,OOBase::Module> QUIT;

	BOOL WINAPI control_c(DWORD evt)
	{
		QUIT::instance().signal(evt);
		return TRUE;
	}

	QuitData::QuitData() : m_result(-1)
	{
		if (!SetConsoleCtrlHandler(control_c,TRUE))
			LOG_ERROR(("SetConsoleCtrlHandler failed: %s",OOBase::system_error_text()));
	}

	void QuitData::signal(int how)
	{
		assert(how != -1);

		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);
		m_result = how;
		guard.release();

		m_condition.signal();
	}

	int QuitData::wait()
	{
		// Wait for the event to be signalled
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		while (m_result == -1)
			m_condition.wait(m_lock);
		
		return m_result;
	}

	int QuitData::get_result()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		return m_result;
	}
}

template class OOBase::Singleton<QuitData,OOBase::Module>;

OOBase::Server::Server()
{
	QUIT::instance();
}

void OOBase::Server::signal(int how)
{
	QUIT::instance().signal(how);
}

int OOBase::Server::wait_for_quit()
{
	return QUIT::instance().wait();
}

void OOBase::Server::quit()
{
	signal(CTRL_C_EVENT);
}

namespace
{
	static SERVICE_STATUS_HANDLE s_ssh = (SERVICE_STATUS_HANDLE)NULL;

	VOID WINAPI ServiceCtrl(DWORD dwControl)
	{
		SERVICE_STATUS ss = {0};
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;
		ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		ss.dwCurrentState = SERVICE_RUNNING;

		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ss.dwCurrentState = SERVICE_STOP_PENDING;
			break;

		default:
			break;
		}

		// Set the status of the service
		if (s_ssh)
			SetServiceStatus(s_ssh,&ss);

		// Tell service_main thread to stop.
		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
			QUIT::instance().signal(CTRL_CLOSE_EVENT);
			break;

		case SERVICE_CONTROL_SHUTDOWN:
			QUIT::instance().signal(CTRL_SHUTDOWN_EVENT);
			break;

		case SERVICE_CONTROL_PARAMCHANGE:
			QUIT::instance().signal(CTRL_BREAK_EVENT);
			break;

		default:
			break;
		}
	}

	VOID WINAPI ServiceMain(DWORD /*dwArgc*/, LPWSTR* lpszArgv)
	{
		SERVICE_STATUS ss = {0};
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PARAMCHANGE;
		ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

		// Register the service ctrl handler.
		s_ssh = RegisterServiceCtrlHandlerW(lpszArgv[0],&ServiceCtrl);
		if (s_ssh)
		{
			ss.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(s_ssh,&ss);
		}

		// Wait for the event to be signalled
		QUIT::instance().wait();

		if (s_ssh)
		{
			ss.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(s_ssh,&ss);
		}
	}
}

int OOBase::Service::wait_for_quit()
{
	static wchar_t sn[] = L"";
	static SERVICE_TABLE_ENTRYW ste[] =
	{
		{sn, &ServiceMain },
		{NULL, NULL}
	};

	// Because it can take a while to determine if we are a service or not,
	// install the Ctrl+C handlers first
	QUIT::instance();

	stdout_write("Attempting Win32 service start...");

	if (!StartServiceCtrlDispatcherW(ste))
	{
		DWORD err = GetLastError();
		if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			stdout_write("Not a Win32 service");

			// We are running from the shell...
			return Server::wait_for_quit();
		}
		else
			return GetLastError();
	}

	// By the time we get here, it's all over
	return QUIT::instance().get_result();
}

#elif defined(HAVE_UNISTD_H)

OOBase::Server::Server() :
		m_tid(pthread_self())
{
	sigemptyset(&m_set);
	sigaddset(&m_set, SIGQUIT);
	sigaddset(&m_set, SIGTERM);
	sigaddset(&m_set, SIGHUP);

	int err = pthread_sigmask(SIG_BLOCK, &m_set, NULL);
	if (err != 0)
		LOG_ERROR(("pthread_sigmask failed: %s",system_error_text(err)));
}

int OOBase::Server::wait_for_quit()
{
	int ret = 0;
	m_tid = pthread_self();
	sigwait(&m_set,&ret);
	return ret;
}

void OOBase::Server::signal(int sig)
{
	pthread_kill(m_tid,sig);
}

void OOBase::Server::quit()
{
	signal(SIGTERM);
}

#else
#error Fix me!
#endif

int OOBase::Service::pid_file(const char* pszPidFile)
{
#if !defined(HAVE_UNISTD_H)
	(void)pszPidFile;
	return 0;
#else

	int fd = open(pszPidFile, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd < 0)
		return errno;

	int err = POSIX::set_close_on_exec(fd,true);
	if (err == 0)
	{
		struct flock fl = {0};
		fl.l_type = F_WRLCK;
		fl.l_start = 0;
		fl.l_whence = SEEK_SET;
		fl.l_len = 0;
		if (fcntl(fd,F_SETLK,&fl) < 0)
		{
			err = errno;
			close(fd);

			if (err == EAGAIN)
				err = EACCES;	
		}
		else
		{
			ftruncate(fd,0);

			LocalString str;
			if ((err = str.printf("%d",getpid())) == 0)
				write(fd,str.c_str(),str.length()+1);
		}
	}
	return err;
#endif
}

