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

#ifndef OOBASE_TIME_H_INCLUDED_
#define OOBASE_TIME_H_INCLUDED_

#include "Base.h"

#include <time.h>

#if defined(_WIN32)
#if defined(__MINGW32__) && defined(_WINSOCKAPI_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#endif

#if defined(HAVE_UNISTD_H)
#include <sys/time.h>
#endif

namespace OOBase
{
	class CDRStream;

	class Clock : public NonCopyable
	{
	public:
		Clock();

		void get_timeval(::timeval& timeout) const;
		unsigned long millisecs() const;

	private:
#if defined(_WIN32)
		LONGLONG   m_start;
#elif defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
		::timespec m_start;
#else
#error Implement platform native time
#endif
	};

	class Timeout
	{
	public:
		Timeout();
		Timeout(unsigned long seconds, unsigned int microseconds);

		Timeout(const Timeout&);

		Timeout& operator = (const Timeout& rhs);

		bool operator == (const Timeout& rhs) const;
		bool operator < (const Timeout& rhs) const;

		void swap(Timeout& rhs);

		bool has_expired() const;
		bool is_infinite() const { return m_null; }
	
		bool get_timeval(::timeval& timeout) const;
		unsigned long millisecs() const;

		bool read(CDRStream& stream);
		bool write(CDRStream& stream) const;

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
		bool get_abs_timespec(::timespec& timeout) const;
		bool get_timespec(::timespec& timeout) const;
#endif

	private:
		bool      m_null;
		
#if defined(_WIN32)
		LONGLONG   m_end;
#elif defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
		::timespec m_end;
#else
#error Implement platform native time
#endif
	};

	inline bool operator != (const Timeout& lhs, const Timeout& rhs)
	{
		return !(lhs == rhs);
	}

	inline bool operator <= (const Timeout& lhs, const Timeout& rhs)
	{
		return !(rhs < lhs);
	}

	inline bool operator > (const Timeout& lhs, const Timeout& rhs)
	{
		return rhs < lhs;
	}

	inline bool operator >= (const Timeout& lhs, const Timeout& rhs)
	{
		return !(lhs < rhs);
	}
}

#endif // OOBASE_TIME_H_INCLUDED_
