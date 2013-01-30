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

#include "../include/OOBase/Memory.h"
#include "../include/OOBase/Buffer.h"

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

OOBase::Buffer* OOBase::Buffer::create(AllocatorInstance& allocator, size_t cbSize, size_t align)
{
	return detail::BufferImpl<AllocatorInstance>::create(allocator,cbSize,align);
}

OOBase::Buffer::Buffer(char* buffer, size_t cbSize, size_t align) :
		m_buffer(buffer),
		m_capacity(cbSize),
		m_align(align),
		m_wr_ptr(buffer),
		m_rd_ptr(buffer)
{
}

const char* OOBase::Buffer::rd_ptr() const
{
	return m_rd_ptr;
}

size_t OOBase::Buffer::mark_rd_ptr() const
{
	return static_cast<size_t>(m_rd_ptr - m_buffer);
}

void OOBase::Buffer::mark_rd_ptr(size_t mark)
{
	m_rd_ptr = (m_buffer + mark);
}

void OOBase::Buffer::rd_ptr(size_t cbSkip)
{
	m_rd_ptr += cbSkip;

	if (m_rd_ptr > m_wr_ptr)
		m_rd_ptr = m_wr_ptr;
}

void OOBase::Buffer::align_rd_ptr(size_t align)
{
	size_t overrun = (reinterpret_cast<uintptr_t>(m_rd_ptr) & (align-1));
	if (overrun)
		rd_ptr(align - overrun);
}

char* OOBase::Buffer::wr_ptr()
{
	return m_wr_ptr;
}

size_t OOBase::Buffer::mark_wr_ptr() const
{
	return static_cast<size_t>(m_wr_ptr - m_buffer);
}

void OOBase::Buffer::mark_wr_ptr(size_t mark)
{
	m_wr_ptr = (m_buffer + mark);
}

int OOBase::Buffer::wr_ptr(size_t cbExpand)
{
	int err = space(cbExpand);
	if (err == 0)
		m_wr_ptr += cbExpand;
	return err;
}

int OOBase::Buffer::align_wr_ptr(size_t align)
{
	int err = 0;
	size_t overrun = (reinterpret_cast<uintptr_t>(m_wr_ptr) & (align-1));
	if (overrun)
	{
		overrun = align - overrun;
		err = space(overrun);
		if (err == 0)
		{
			memset(m_wr_ptr,0xee,overrun);
			wr_ptr(overrun);
		}
	}
	return err;
}

size_t OOBase::Buffer::length() const
{
	return static_cast<size_t>(m_wr_ptr - m_rd_ptr);
}

void OOBase::Buffer::reset()
{
	m_rd_ptr = m_wr_ptr = m_buffer;
}

void OOBase::Buffer::compact()
{
	char* orig_rd = m_rd_ptr;
	ptrdiff_t len = (m_wr_ptr - m_rd_ptr);

	reset();

	if (len > 0)
	{
		// We know there is space...
		memmove(m_rd_ptr,orig_rd,static_cast<size_t>(len));
		m_wr_ptr = m_rd_ptr + len;
	}
}

size_t OOBase::Buffer::space() const
{
	size_t used = static_cast<size_t>(m_wr_ptr - m_buffer);

	return (used >= m_capacity ? 0 : m_capacity - used);
}

/**
 *  \warning A reallocation may occur updating rd_ptr() and wr_ptr().
 */
int OOBase::Buffer::space(size_t cbSpace)
{
	int err = 0;
	size_t cbAbsCapacity = (m_wr_ptr - m_buffer) + cbSpace;
	if (cbAbsCapacity > m_capacity)
	{
		size_t rd_pos = (m_rd_ptr - m_buffer);
		size_t wr_pos = (m_wr_ptr - m_buffer);

		char* new_ptr = reallocate(m_buffer,cbAbsCapacity,m_align);
		if (!new_ptr)
			return ERROR_OUTOFMEMORY;

		m_buffer = new_ptr;
		m_rd_ptr = m_buffer + rd_pos;
		m_wr_ptr = m_buffer + wr_pos;
		m_capacity = cbAbsCapacity;
	}
	return err;
}
