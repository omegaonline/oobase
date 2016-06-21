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

#include "../include/OOBase/Random.h"
#include "../include/OOBase/Timeout.h"

int OOBase::random_chars(char* buffer, size_t len)
{
	static const char c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYUZabcdefghijklmnopqrstuvwxyz0123456789";

	if (len)
		buffer[--len] = '\0';

	while (len)
	{
		unsigned char buf[128] = {0};
		int err = random_bytes(buf,sizeof(buf));
		if (err)
			return err;

		for (size_t i = 0;i < sizeof(buf) && len;)
			buffer[--len] = c[buf[i++] % (sizeof(c)-1)];
	}

	return 0;
}

OOBase::detail::xoroshiro128plus::xoroshiro128plus()
{
	uint64_t s[2];
	if (random_bytes(s,sizeof(s)) != 0)
	{
		splitmix64 sm(Clock().microseconds());
		m_s0 = sm.next();
		m_s1 = sm.next();
	}
	else
	{
		m_s0 = s[0];
		m_s1 = s[1];
	}
}
