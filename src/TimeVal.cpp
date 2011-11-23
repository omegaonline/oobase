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

#include "../include/OOBase/TimeVal.h"

#include "tr24731.h"

#include <time.h>

#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#include <limits.h>

const OOBase::timeval_t OOBase::timeval_t::MaxTime(LLONG_MAX, 999999);
const OOBase::timeval_t OOBase::timeval_t::Zero(0, 0);

OOBase::timeval_t OOBase::timeval_t::gettimeofday()
{

#if defined(_WIN32)

	static const ULONGLONG epoch = 116444736000000000LL;

	FILETIME file_time;
	GetSystemTimeAsFileTime(&file_time);

	ULARGE_INTEGER ularge;
	ularge.LowPart = file_time.dwLowDateTime;
	ularge.HighPart = file_time.dwHighDateTime;

	// Move to epoch
	ULONGLONG q = (ularge.QuadPart - epoch);

	return OOBase::timeval_t(q / 10000000LL,static_cast<int>((q / 10L) % 1000000L));

#elif defined(HAVE_SYS_TIME_H) && (HAVE_SYS_TIME_H == 1)

	timeval tv;
	::gettimeofday(&tv,NULL);

	return OOBase::timeval_t(tv.tv_sec,tv.tv_usec);

#else
#error gettimeofday() !!
#endif
}

OOBase::timeval_t& OOBase::timeval_t::operator += (const timeval_t& rhs)
{
	if (m_tv_usec + rhs.m_tv_usec >= 1000000)
	{
		int nsec = (m_tv_usec + rhs.m_tv_usec) / 1000000;
		m_tv_usec -= 1000000 * nsec;
		m_tv_sec += nsec;
	}

	m_tv_sec += rhs.m_tv_sec;
	m_tv_usec += rhs.m_tv_usec;

	return *this;
}

OOBase::timeval_t& OOBase::timeval_t::operator -= (const timeval_t& rhs)
{
	/* Perform the carry for the later subtraction by updating r. */
	timeval_t r = rhs;
	if (m_tv_usec < r.m_tv_usec)
	{
		int nsec = (r.m_tv_usec - m_tv_usec) / 1000000 + 1;
		r.m_tv_usec -= 1000000 * nsec;
		r.m_tv_sec += nsec;
	}

	if (m_tv_usec - r.m_tv_usec >= 1000000)
	{
		int nsec = (m_tv_usec - r.m_tv_usec) / 1000000;
		r.m_tv_usec += 1000000 * nsec;
		r.m_tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	 m_tv_usec is certainly positive. */
	m_tv_sec -= r.m_tv_sec;
	m_tv_usec -= r.m_tv_usec;

	return *this;
}

OOBase::timeval_t OOBase::timeval_t::deadline(unsigned long msec)
{
	return gettimeofday() + timeval_t(msec / 1000,(msec % 1000) * 1000);
}

::tm OOBase::timeval_t::gmtime() const
{
	time_t t = this->m_tv_sec;
	::tm v = {0};

#if defined(HAVE_PTHREAD)
	gmtime_r(&t,&v);
#elif defined(HAVE_TR_24731)
	gmtime_s(&v,&t);
#else
	v = *::gmtime(&t);
#endif

	return v;
}

::tm OOBase::timeval_t::localtime() const
{
	time_t t = this->m_tv_sec;
	::tm v = {0};

#if defined(HAVE_PTHREAD)
	localtime_r(&t,&v);
#elif defined(HAVE_TR_24731)
	localtime_s(&v,&t);
#else
	v = *::localtime(&t);
#endif

	return v;
}

OOBase::Countdown::Countdown(const timeval_t* timeout) :
		m_null(timeout == NULL),
		m_end(timeout ? timeval_t::gettimeofday() + *timeout : timeval_t::Zero)
{
}

OOBase::Countdown::Countdown(timeval_t::time_64_t s, int us) :
		m_null(false),
		m_end(timeval_t::gettimeofday() + timeval_t(s,us))
{
}

OOBase::Countdown::Countdown(const Countdown& rhs) :
		m_null(rhs.m_null),
		m_end(rhs.m_end)
{
}

OOBase::Countdown& OOBase::Countdown::operator = (const Countdown& rhs)
{
	if (this != &rhs)
	{
		m_null = rhs.m_null;
		m_end = rhs.m_end;
	}
	return *this;
}

bool OOBase::Countdown::has_ended() const
{
	if (m_null)
		return false;

	return timeval_t::gettimeofday() >= m_end;
}

void OOBase::Countdown::timeout(timeval_t& timeout) const
{
	if (m_null)
		timeout = timeval_t::MaxTime;
	else
	{
		timeval_t now = timeval_t::gettimeofday();
		if (now >= m_end)
			timeout = timeval_t::Zero;
		else
			timeout = (m_end - now);
	}
}

void OOBase::Countdown::timeval(::timeval& timeout) const
{
	timeval_t to;
	this->timeout(to);

	timeout.tv_sec = static_cast<long>(to.tv_sec());
	timeout.tv_usec = to.tv_usec();
}

#if defined(_WIN32)
OOBase::Countdown::Countdown(DWORD millisecs) :
		m_null(millisecs == INFINITE),
		m_end(millisecs == INFINITE ? timeval_t::Zero : timeval_t::gettimeofday() + timeval_t(millisecs / 1000, (millisecs % 1000) * 1000))
{
}

DWORD OOBase::Countdown::msec() const
{
	if (m_null)
		return INFINITE;

	timeval_t now = timeval_t::gettimeofday();
	if (now >= m_end)
		return 0;
		
	return (m_end - now).msec();
}
#else

void OOBase::Countdown::abs_timespec(timespec& timeout) const
{
	timeout.tv_sec = m_end.tv_sec();
	timeout.tv_nsec = m_end.tv_usec() * 1000;
}

#endif
