///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2011 Rick Taylor
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

#include "../include/OOBase/String.h"

#include "tr24731.h"

#if !defined(va_copy)
#define va_copy(a,b) ((a) = (b))
#endif

OOBase::LocalString::~LocalString()
{
	OOBase::LocalAllocator::free(m_data);
}

int OOBase::LocalString::assign(const LocalString& str)
{
	int err = 0;
	if (this != &str)
		err = assign(str.c_str(),str.length());
	return err;
}

int OOBase::LocalString::assign(const char* sz, size_t len)
{
	char* new_sz = NULL;
	if (sz && len)
	{
		if (len == npos)
			len = strlen(sz);

		if (len == length())
		{
			memcpy(m_data,sz,len);
			return 0;
		}

		new_sz = static_cast<char*>(OOBase::LocalAllocator::allocate(len+1));
		if (!new_sz)
			return ERROR_OUTOFMEMORY;

		memcpy(new_sz,sz,len);
		new_sz[len] = '\0';
	}

	OOBase::LocalAllocator::free(m_data);
	m_data = new_sz;

	return 0;
}

int OOBase::LocalString::append(const char* sz, size_t len)
{
	if (sz && len)
	{
		size_t our_len = length();
		if (len == npos)
			len = strlen(sz);

		char* new_sz = static_cast<char*>(OOBase::LocalAllocator::reallocate(m_data,our_len + len + 1));
		if (!new_sz)
			return ERROR_OUTOFMEMORY;

		memcpy(new_sz+our_len,sz,len);
		new_sz[our_len+len] = '\0';
		m_data = new_sz;
	}

	return 0;
}

int OOBase::LocalString::replace_all(char from, char to)
{
	size_t i = length();
	while (i > 0)
	{
		--i;
		if (m_data[i] == from)
			m_data[i] = to;
	}
	return 0;
}

int OOBase::LocalString::truncate(size_t len)
{
	if (len < length())
		m_data[len] = '\0';
	return 0;
}

int OOBase::LocalString::vprintf(const char* format, va_list args)
{
	char szBuf[256];
	char* new_buf = szBuf;

	va_list args_copy;
	va_copy(args_copy,args);

	int r = vsnprintf_s(new_buf,sizeof(szBuf),format,args_copy);
	if (r == -1)
		return errno;

	if (static_cast<size_t>(r) >= sizeof(szBuf))
	{
		for (;;)
		{
			new_buf = static_cast<char*>(OOBase::LocalAllocator::allocate(r+1));
			if (!new_buf)
				return ERROR_OUTOFMEMORY;

			va_copy(args_copy,args);
			int r1 = vsnprintf_s(new_buf,r+1,format,args_copy);
			if (r1 == -1)
				return errno;

			if (r1 > r+1)
				r = r1;
			else
				break;
		}
	}

	int err = assign(new_buf,r);

	if (new_buf != szBuf)
		OOBase::LocalAllocator::free(new_buf);

	return err;
}

int OOBase::LocalString::printf(const char* format, ...)
{
	va_list args;
	va_start(args,format);

	int err = vprintf(format,args);

	va_end(args);

	return err;
}

size_t OOBase::LocalString::find(char c, size_t start) const
{
	if (start >= length())
		return npos;

	const char* p = strchr(m_data + start,c);
	if (!p)
		return npos;

	// Returns *absolute* position
	return static_cast<size_t>(p - m_data);
}

size_t OOBase::LocalString::find(const char* sz, size_t start) const
{
	if (start >= length())
		return npos;

	const char* p = strstr(m_data + start,sz);
	if (!p)
		return npos;

	// Returns *absolute* position
	return static_cast<size_t>(p - m_data);
}

int OOBase::String::copy_on_write(size_t inc)
{
	node_addref(m_node);

	if (!m_node || m_node->m_refcount > 2)
	{
		size_t our_len = (m_node ? length() : 0);

		// There is an implicit len+1 here as m_data[1] in struct
		Node* new_node = static_cast<Node*>(OOBase::HeapAllocator::allocate(sizeof(Node) + our_len+inc));
		if (!new_node)
			return ERROR_OUTOFMEMORY;

		if (our_len)
			memcpy(new_node->m_data,m_node->m_data,our_len);

		new_node->m_refcount = 1;
		new_node->m_data[our_len] = '\0';

		if (m_node)
		{
			node_release(m_node);
			node_release(m_node);
		}

		m_node = new_node;
	}
	else
	{
		if (inc > 0)
		{
			size_t our_len = length();

			// It's our buffer to play with... (There is an implicit +1 here)
			Node* new_node = static_cast<Node*>(OOBase::HeapAllocator::reallocate(m_node,sizeof(Node) + our_len+inc));
			if (!new_node)
				return ERROR_OUTOFMEMORY;

			m_node = new_node;
		}

		node_release(m_node);
	}

	return 0;
}

int OOBase::String::assign(const char* sz, size_t len)
{
	int err = 0;

	if (len == npos && sz)
		len = strlen(sz);

	if (sz && len)
	{
		if ((err = copy_on_write(len)) == 0)
		{
			memcpy(m_node->m_data,sz,len);
			m_node->m_data[len] = '\0';
		}
	}
	else
	{
		node_release(m_node);
		m_node = NULL;
	}
	return err;
}

int OOBase::String::append(const char* sz, size_t len)
{
	int err = 0;
	if (len == npos && sz)
		len = strlen(sz);

	if (sz && len)
	{
		if ((err = copy_on_write(len)) == 0)
		{
			size_t our_len = length();
			memcpy(m_node->m_data+our_len,sz,len);
			m_node->m_data[our_len+len] = '\0';
		}
	}

	return err;
}

int OOBase::String::replace_all(char from, char to)
{
	bool copied = false;
	size_t i = length();
	while (i > 0)
	{
		--i;
		if (m_node->m_data[i] == from)
		{
			if (!copied)
			{
				int err = copy_on_write(0);
				if (err != 0)
					return err;

				copied = true;
			}
			m_node->m_data[i] = to;
		}
	}
	return 0;
}

int OOBase::String::truncate(size_t len)
{
	int err = 0;
	if (len < length() && (err = copy_on_write(0)) == 0)
		m_node->m_data[len] = '\0';

	return err;
}

int OOBase::String::printf(const char* format, ...)
{
	va_list args;
	va_start(args,format);

	char* new_buf = NULL;
	char szBuf[256];
	int r = vsnprintf_s(szBuf,sizeof(szBuf),format,args);

	va_end(args);

	if (r == -1)
		return errno;

	if (static_cast<size_t>(r) < sizeof(szBuf))
		new_buf = szBuf;
	else
	{
		for (;;)
		{
			new_buf = static_cast<char*>(OOBase::LocalAllocator::allocate(r+1));
			if (!new_buf)
				return ERROR_OUTOFMEMORY;

			va_start(args,format);

			int r1 = vsnprintf_s(new_buf,r+1,format,args);

			va_end(args);

			if (r1 == -1)
				return errno;

			if (r1 > r+1)
				r = r1;
			else
				break;
		}
	}

	int err = assign(new_buf,r);

	if (new_buf != szBuf)
		OOBase::LocalAllocator::free(new_buf);

	return err;
}

size_t OOBase::String::find(char c, size_t start) const
{
	if (start >= length())
		return npos;

	const char* p = strchr(m_node->m_data + start,c);
	if (!p)
		return npos;

	// Returns *absolute* position
	return static_cast<size_t>(p - m_node->m_data);
}

size_t OOBase::String::find(const char* sz, size_t start) const
{
	if (start >= length())
		return npos;

	const char* p = strstr(m_node->m_data + start,sz);
	if (!p)
		return npos;

	// Returns *absolute* position
	return static_cast<size_t>(p - m_node->m_data);
}

void OOBase::String::node_addref(Node* node)
{
	if (node)
		++node->m_refcount;
}

void OOBase::String::node_release(Node* node)
{
	if (node && --node->m_refcount == 0)
		OOBase::HeapAllocator::free(node);
}
