///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#include "../include/OOBase/Timeout.h"
#include "../include/OOBase/CDRStream.h"

namespace
{
#if defined(_WIN32)
	const LONGLONG& qp_freq()
	{
		static LONGLONG freq = -1LL;
		if (freq < 0)
		{
			LARGE_INTEGER liFreq;
			if (QueryPerformanceFrequency(&liFreq))
				freq = liFreq.QuadPart;
			else
				freq = 0;
		}
		return freq;
	}

	LONGLONG time_now()
	{
		LONGLONG freq = qp_freq();
		if (freq > 0)
		{
			LARGE_INTEGER liNow;
			if (!QueryPerformanceCounter(&liNow))
				OOBase_CallCriticalFailure(GetLastError());

			return liNow.QuadPart;
		}
	
	#if (_WIN32_WINNT >= 0x0600)
		return GetTickCount64();
	#else
		return GetTickCount();
	#endif
	}
#endif
}

OOBase::Timeout::Timeout() :
		m_null(true)
{
}

OOBase::Timeout::Timeout(const Timeout& rhs) :
		m_null(rhs.m_null),
		m_end(rhs.m_end)
{
}

OOBase::Timeout::Timeout(unsigned long seconds, unsigned int microseconds) :
		m_null(false)
{
#if defined(_WIN32)

	m_end = time_now();
	LONGLONG freq = qp_freq();
	if (freq > 0)
		m_end += (seconds * freq) + ((microseconds * freq) / 1000000);
	else
		m_end += (seconds * 1000) + (microseconds / 1000);

#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)
	
	if (clock_gettime(CLOCK_MONOTONIC,&m_end) != 0)
		OOBase_CallCriticalFailure(errno);

	m_end.tv_nsec += static_cast<long>(microseconds) * 1000;
	if (m_end.tv_nsec >= 1000000000)
	{
		seconds += m_end.tv_nsec / 1000000000;
		m_end.tv_nsec %= 1000000000;
	}
	m_end.tv_sec += seconds;
	
#endif
}

OOBase::Timeout& OOBase::Timeout::operator = (const Timeout& rhs)
{
	if (this != &rhs)
	{
		m_null = rhs.m_null;
		m_end = rhs.m_end;
	}
	return *this;
}

bool OOBase::Timeout::has_expired() const
{
	if (m_null)
		return false;

#if defined(_WIN32)

	return (time_now() >= m_end);

#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)

	::timespec now = {0};
	if (clock_gettime(CLOCK_MONOTONIC,&now) != 0)
		OOBase_CallCriticalFailure(errno);

	if (now.tv_sec < m_end.tv_sec || (now.tv_sec == m_end.tv_sec && now.tv_nsec < m_end.tv_nsec))
		return false;
	
	return true;

#endif
}

void OOBase::Timeout::get_timeval(::timeval& timeout) const
{
	assert(!m_null);

#if defined(_WIN32)

	LONGLONG diff = m_end - time_now();
	if (diff < 0)
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}
	else
	{
		LONGLONG freq = qp_freq();
		if (freq == 0)
			freq = 1000;

		timeout.tv_sec = static_cast<long>(diff / freq);
		timeout.tv_usec = static_cast<long>(((diff % freq) * 1000000) / freq);
	}
	
#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)

	::timespec now = {0};
	if (clock_gettime(CLOCK_MONOTONIC,&now) != 0)
		OOBase_CallCriticalFailure(errno);

	if (now.tv_sec < m_end.tv_sec || (now.tv_sec == m_end.tv_sec && now.tv_nsec < m_end.tv_nsec))
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
	}
	else
	{
		timeout.tv_sec = m_end.tv_sec - now.tv_sec;
		if (m_end.tv_nsec < now.tv.nsec)
		{
			--timeout.tv_sec;
			timeout.tv_msec = ((m_end.tv_nsec + 1000000000) - now.tv_nsec) / 1000;
		}
		else
			timeout.tv_msec = (m_end.tv_nsec - now.tv_nsec) / 1000;
	}

#endif
}

unsigned long OOBase::Timeout::millisecs() const
{
	if (m_null)
		return INFINITE;

#if defined(_WIN32)

	LONGLONG diff = m_end - time_now();
	if (diff < 0)
		return 0;
	
	LONGLONG freq = qp_freq();
	if (freq == 0)
		freq = 1000;

	LONGLONG r = (diff / freq) * 1000;
	r += ((diff % freq) * 1000) / freq;
	return static_cast<unsigned long>(r);
		
#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)

	::timespec now = {0};
	if (clock_gettime(CLOCK_MONOTONIC,&now) != 0)
		OOBase_CallCriticalFailure(errno);

	if (now.tv_sec < m_end.tv_sec || (now.tv_sec == m_end.tv_sec && now.tv_nsec < m_end.tv_nsec))
		return 0;

	unsigned long r = (m_end.tv_sec - now.tv_sec) * 1000;
	if (m_end.tv_nsec < now.tv.nsec)
	{
		r -= 1000;
		r += ((m_end.tv_nsec + 1000000000) - now.tv_nsec) / 1000000;
	}
	else
		r += (m_end.tv_nsec - now.tv_nsec) / 1000000;

#endif
}

#if defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)

void OOBase::Timeout::get_abs_timespec(timespec& timeout) const
{
	timeout.tv_sec = m_end.tv_sec();
	timeout.tv_nsec = m_end.tv_usec() * 1000;

#error BROKEN!
}

#endif

bool OOBase::Timeout::operator < (const Timeout& rhs) const
{
	if (rhs.m_null)
		return true;
	
	if (!m_null)
	{
#if defined(_WIN32)
		return (m_end < rhs.m_end);
#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)
		if (m_end.tv_sec < rhs.m_end.tv_sec || (m_end.tv_sec == rhs.m_end.tv_sec && m_end.tv_nsec < rhs.m_end.tv_nsec))
			return true;
#endif
	}

	return false;
}

bool OOBase::Timeout::read(CDRStream& stream)
{
	bool res = stream.read(m_null);
	if (res && !m_null)
	{
#if defined(_WIN32)
		res = stream.read(m_end);
#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)
		res = stream.read(m_end.tv_sec);
		if (res)
			res = stream.read(m_end.tv_nsec);
#endif
	}
	return res;
}

bool OOBase::Timeout::write(CDRStream& stream) const
{
	bool res = stream.write(m_null);
	if (res && !m_null)
	{
#if defined(_WIN32)
		res = stream.write(m_end);
#elif defined(HAVE_UNISTD_H) && (_POSIX_TIMERS > 0)
		res = stream.write(m_end.tv_sec);
		if (res)
			res = stream.write(m_end.tv_nsec);
#endif
	}
	return res;
}
