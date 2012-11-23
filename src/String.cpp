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

int OOBase::detail::strings::grow(size_t inc, StringNode*& node, void* (*pfnAllocate)(size_t,size_t), void* (*pfnRellocate)(void*,size_t,size_t), void (*pfnFree)(void*))
{
	if (node && node->m_refcount == 1)
	{
		if (inc > 0)
		{
			// It's our buffer to play with... (There is an implicit +1 here)
			StringNode* new_node = static_cast<StringNode*>((*pfnRellocate)(node,sizeof(StringNode) + node->m_length + inc,alignof<StringNode>::value));
			if (!new_node)
				return ERROR_OUTOFMEMORY;

			new_node->m_length = node->m_length + inc;
			new_node->m_data[new_node->m_length] = '\0';

			node = new_node;
		}
	}
	else
	{
		size_t our_len = (node ? node->m_length : 0);

		// There is an implicit len+1 here as m_data[1] in struct
		StringNode* new_node = static_cast<StringNode*>((*pfnAllocate)(sizeof(StringNode) + our_len + inc,alignof<StringNode>::value));
		if (!new_node)
			return ERROR_OUTOFMEMORY;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 6385)
#endif

		if (our_len)
			memcpy(new_node->m_data,node->m_data,our_len);

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

		new_node->m_refcount = 1;
		new_node->m_length = our_len + inc;
		new_node->m_data[new_node->m_length] = '\0';

		if (node && --node->m_refcount == 0)
			(*pfnFree)(node);

		node = new_node;
	}

	return 0;
}

int OOBase::detail::strings::grow(size_t inc, StringNodeAllocator*& node)
{
	if (!node)
		return ERROR_OUTOFMEMORY;

	if (node->m_refcount == 1)
	{
		if (inc > 0)
		{
			// It's our buffer to play with... (There is an implicit +1 here)
			StringNodeAllocator* new_node = static_cast<StringNodeAllocator*>(node->m_allocator->reallocate(node,sizeof(StringNodeAllocator) + node->m_length + inc,alignof<StringNodeAllocator>::value));
			if (!new_node)
				return ERROR_OUTOFMEMORY;

			new_node->m_length += inc;
			new_node->m_data[new_node->m_length] = '\0';

			node = new_node;
		}
	}
	else
	{
		// There is an implicit len+1 here as m_data[1] in struct
		StringNodeAllocator* new_node = static_cast<StringNodeAllocator*>(node->m_allocator->allocate(sizeof(StringNodeAllocator) + node->m_length + inc,alignof<StringNodeAllocator>::value));
		if (!new_node)
			return ERROR_OUTOFMEMORY;

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 6385)
#endif

		if (node->m_length)
			memcpy(new_node->m_data,node->m_data,node->m_length);

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

		new_node->m_refcount = 1;
		new_node->m_length = node->m_length + inc;
		new_node->m_data[new_node->m_length] = '\0';
		new_node->m_allocator = node->m_allocator;

		if (node && --node->m_refcount == 0)
			node->m_allocator->free(node);

		node = new_node;
	}

	return 0;
}

int OOBase::printf(TempPtr<char>& ptr, const char* format, ...)
{
	va_list args;
	va_start(args,format);

	int err = OOBase::vprintf(ptr,format,args);

	va_end(args);

	return err;
}

int OOBase::vprintf(TempPtr<char>& ptr, const char* format, va_list args)
{
	for (int r = 63;;)
	{
		size_t len = static_cast<size_t>(r) + 2;
		if (!ptr.reallocate(len))
			return ERROR_OUTOFMEMORY;

		va_list args_copy;
		va_copy(args_copy,args);

		r = vsnprintf_s(ptr,len,format,args_copy);

		va_end(args_copy);

		if (r == -1)
			return errno;
		else if (static_cast<size_t>(r) < len)
			return 0;
	}
}
