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

#include "../include/OOSvrBase/Service.h"
#include "../include/OOSvrBase/Logger.h"
#include "../include/OOBase/Singleton.h"

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
	typedef OOBase::Singleton<QuitData> QUIT;

#if defined(_WIN32)

	BOOL WINAPI control_c(DWORD evt)
	{
		QUIT::instance()->signal(evt);
		return TRUE;
	}

	void init_sig_handler()
	{
		if (!SetConsoleCtrlHandler(control_c,TRUE))
			LOG_ERROR(("SetConsoleCtrlHandler failed: %s",OOBase::Win32::FormatMessage().c_str()));
	}

#elif defined(HAVE_SIGNAL_H)

	void on_signal(int sig)
	{
		QUIT::instance()->signal(sig);
	}

	void init_sig_handler()
	{
		// Catch SIGTERM
		if (signal(SIGTERM,&on_signal) == SIG_ERR)
			LOG_ERROR(("signal(SIGTERM) failed: %s",OOBase::strerror(errno).c_str()));

		// Catch SIGHUP
		if (signal(SIGHUP,&on_signal) == SIG_ERR)
			LOG_ERROR(("signal(SIGHUP) failed: %s",OOBase::strerror(errno).c_str()));
	}

#else

#error Fix me!

#endif

	QuitData::QuitData() : m_result(-1)
	{
		init_sig_handler();
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
		{
			m_condition.wait(m_lock);
		} 

		return m_result;
	}

	int QuitData::get_result()
	{
		OOBase::Guard<OOBase::Condition::Mutex> guard(m_lock);

		return m_result;
	}
}

void OOSvrBase::Server::signal(int how)
{
	QUIT::instance()->signal(how);	
}

int OOSvrBase::Server::wait_for_quit()
{
	return QUIT::instance()->wait();
}

#if defined(_WIN32)

void OOSvrBase::Server::quit()
{
	signal(CTRL_C_EVENT);
}

namespace
{
	static SERVICE_STATUS_HANDLE s_ssh = 0;

	static VOID WINAPI ServiceCtrl(DWORD dwControl)
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
			QUIT::instance()->signal(CTRL_C_EVENT);
			break;

		case SERVICE_CONTROL_SHUTDOWN:
			QUIT::instance()->signal(CTRL_SHUTDOWN_EVENT);
			break;

		case SERVICE_CONTROL_PARAMCHANGE:
			QUIT::instance()->signal(CTRL_BREAK_EVENT);
			break;

		default:
			break;
		}
	}

	static VOID WINAPI ServiceMain(DWORD /*dwArgc*/, LPWSTR* lpszArgv)
	{
		SERVICE_STATUS ss = {0};
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PARAMCHANGE;
		ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

		// Register the service ctrl handler.
		s_ssh = RegisterServiceCtrlHandlerW(lpszArgv[0],&ServiceCtrl);
		if (!s_ssh)
			LOG_ERROR(("RegisterServiceCtrlHandlerW failed: %s",OOBase::Win32::FormatMessage().c_str()));
		else
		{
			ss.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(s_ssh,&ss);
		}

		// Wait for the event to be signalled
		QUIT::instance()->wait();

		if (s_ssh)
		{
			ss.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(s_ssh,&ss);
		}
	}
}

int OOSvrBase::Service::wait_for_quit()
{
	static SERVICE_TABLE_ENTRYW ste[] =
	{
		{L"", &ServiceMain },
		{NULL, NULL}
	};	

	// Because it can take a while to determine if we are a service or not,
	// install the Ctrl+C handlers first
	QUIT::instance();

	LOG_DEBUG(("Attempting Win32 service start..."));

	if (!StartServiceCtrlDispatcherW(ste))
	{
		DWORD err = GetLastError();
		if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			LOG_DEBUG(("Not a Win32 service"));

			// We are running from the shell...
			return Server::wait_for_quit();
		}
		else
			LOG_ERROR(("StartServiceCtrlDispatcherW failed: %s",OOBase::Win32::FormatMessage().c_str()));
	}

	// By the time we get here, it's all over
	return QUIT::instance()->get_result();
}

#else

void OOSvrBase::Server::quit()
{
	signal(SIGTERM);
}

#endif
