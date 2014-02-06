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

#ifndef OOBASE_STRING_H_INCLUDED_
#define OOBASE_STRING_H_INCLUDED_

#include "Memory.h"
#include "Atomic.h"
#include "ScopedArrayPtr.h"
#include "SharedPtr.h"
#include "tr24731.h"

#include <string.h>

#if !defined(va_copy)
#define va_copy(a,b) ((a) = (b))
#endif

namespace OOBase
{
	template <typename A, size_t S>
	inline int vprintf(ScopedArrayPtr<char,A,S>& ptr, const char* format, va_list args)
	{
		for (;;)
		{
			va_list args_copy;
			va_copy(args_copy,args);

			int r = vsnprintf_s(ptr.get(),ptr.count(),format,args_copy);

			va_end(args_copy);

			if (r == -1)
				return errno;

			if (static_cast<size_t>(r) < ptr.count())
				return 0;

			if (!ptr.reallocate(static_cast<size_t>(r) + 1))
				return ERROR_OUTOFMEMORY;
		}
	}

#if defined(__GNUC__)
	template <typename A, size_t S>
	int printf(ScopedArrayPtr<char,A,S>& ptr, const char* format, ...) __attribute__((format(printf,2,3)));
#endif

	template <typename A, size_t S>
	inline int printf(ScopedArrayPtr<char,A,S>& ptr, const char* format, ...)
	{
		va_list args;
		va_start(args,format);

		int err = OOBase::vprintf(ptr,format,args);

		va_end(args);

		return err;
	}

	template <typename Allocator>
	class ScopedStringImpl : public NonCopyable
	{
	public:
		static const size_t npos = size_t(-1);

		ScopedStringImpl() : m_data()
		{}

		ScopedStringImpl(AllocatorInstance& allocator) : m_data(allocator)
		{}

		int compare(const char* rhs) const
		{
			if (!m_data)
				return (rhs == NULL ? 0 : -1);
			else if (rhs == NULL)
				return 1;

			return strcmp(m_data.get(),rhs);
		}

		template <typename A1>
		int compare(const ScopedStringImpl<A1>& rhs) const
		{
			if (this == &rhs)
				return true;
			return this->compare(rhs.c_str());
		}

		template <typename T> bool operator == (T rhs) const { return (this->compare(rhs) == 0); }
		template <typename T> bool operator < (T rhs) const { return (this->compare(rhs) < 0); }
		template <typename T> bool operator <= (T rhs) const { return (this->compare(rhs) <= 0); }
		template <typename T> bool operator > (T rhs) const { return (this->compare(rhs) > 0); }
		template <typename T> bool operator >= (T rhs) const { return (this->compare(rhs) >= 0); }
		template <typename T> bool operator != (T rhs) const { return (this->compare(rhs) != 0); }

		int assign(const char* sz, size_t len = npos)
		{
			if (len == npos)
				len = strlen(sz);

			if (!m_data.reallocate(len + 1))
				return ERROR_OUTOFMEMORY;

			if (len)
				memcpy(m_data.get(),sz,len);
			m_data[len] = '\0';
			return 0;
		}

		int append(const char* sz, size_t len = npos)
		{
			if (len == npos)
				len = strlen(sz);

			if (len)
			{
				size_t orig_len = length();
				if (!m_data.reallocate(orig_len + len + 1))
					return ERROR_OUTOFMEMORY;

				memcpy(m_data.get() + orig_len,sz,len);
				m_data[orig_len + len] = '\0';
			}
			return 0;
		}

		const char* c_str() const
		{
			return m_data && m_data.count() ? m_data.get() : NULL;
		}

		char& operator [](ptrdiff_t i) const
		{
			return m_data[i];
		}

		bool empty() const
		{
			return !m_data || m_data.count() == 0 || m_data[0] == '\0';
		}

		void clear()
		{
			if (m_data)
				m_data[0] = '\0';
		}

		size_t length() const
		{
			return empty() ? 0 : strlen(m_data.get());
		}

		size_t find(char c, size_t start = 0) const
		{
			if (empty())
				return npos;

			const char* f = strchr(m_data.get() + start,c);
			return f ? size_t(f - m_data.get()) : npos;
		}

		size_t find(const char* sz, size_t start = 0) const
		{
			if (empty())
				return npos;

			const char* f = strstr(m_data.get() + start,sz);
			return f ? size_t(f - m_data.get()) : npos;
		}

#if defined(__GNUC__)
		int printf(const char* format, ...) __attribute__((format(printf,2,3)))
#else
		int printf(const char* format, ...)
#endif
		{
			va_list args;
			va_start(args,format);

			int err = OOBase::vprintf(m_data,format,args);

			va_end(args);

			return err;
		}

		int vprintf(const char* format, va_list args)
		{
			return OOBase::vprintf(m_data,format,args);
		}

		AllocatorInstance& get_allocator() const
		{
			return m_data.get_allocator();
		}

#if defined(_WIN32)
		int wchar_t_to_utf8(const wchar_t* wsz)
		{
			return wchar_t_to_utf8(wsz,m_data);
		}
#endif

	private:
		ScopedArrayPtr<char,Allocator,24> m_data;
	};

	class String
	{
		typedef SharedPtr<ScopedStringImpl<CrtAllocator> > node_t;

	public:
		static const size_t npos = ScopedStringImpl<CrtAllocator>::npos;

		String() : m_ptr()
		{}

		String(const String& rhs) : m_ptr(rhs.m_ptr)
		{}

		String& operator = (const String& rhs)
		{
			String(rhs).swap(*this);
			return *this;
		}

		void swap(String& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
		}

		int compare(const char* rhs) const
		{
			if (!m_ptr)
				return (rhs == NULL ? 0 : -1);

			return m_ptr->compare(rhs);
		}

		int compare(const String& rhs) const
		{
			if (this == &rhs)
				return true;

			return compare(rhs.c_str());
		}

		template <typename T> bool operator == (T rhs) const { return (this->compare(rhs) == 0); }
		template <typename T> bool operator < (T rhs) const { return (this->compare(rhs) < 0); }
		template <typename T> bool operator <= (T rhs) const { return (this->compare(rhs) <= 0); }
		template <typename T> bool operator > (T rhs) const { return (this->compare(rhs) > 0); }
		template <typename T> bool operator >= (T rhs) const { return (this->compare(rhs) >= 0); }
		template <typename T> bool operator != (T rhs) const { return (this->compare(rhs) != 0); }

		int assign(const char* sz, size_t len = npos)
		{
			if (sz && len)
			{
				node_t ptr = new_node();
				if (!ptr)
					return ERROR_OUTOFMEMORY;

				int err = ptr->assign(sz,len);
				if (err)
					return err;

				m_ptr.swap(ptr);
			}
			return 0;
		}

		int append(const char* sz, size_t len = npos)
		{
			if (sz && len)
			{
				node_t ptr;
				int err = new_node(ptr,m_ptr);
				if (err)
					return err;

				if ((err = ptr->append(sz,len)))
					return err;

				m_ptr.swap(ptr);
			}
			return 0;
		}

		int append(const String& rhs)
		{
			if (!m_ptr)
			{
				m_ptr = rhs.m_ptr;
				return 0;
			}
			return append(rhs.c_str(),rhs.length());
		}

		const char* c_str() const
		{
			return !m_ptr ? NULL : m_ptr->c_str();
		}

		char operator [](ptrdiff_t i) const
		{
			return m_ptr->operator [](i);
		}

		bool empty() const
		{
			return !m_ptr ? true : m_ptr->empty();
		}

		void clear()
		{
			m_ptr.reset();
		}

		size_t length() const
		{
			return !m_ptr ? 0 : m_ptr->length();
		}

		size_t find(char c, size_t start = 0) const
		{
			return !m_ptr ? npos : m_ptr->find(c,start);
		}

		size_t find(const char* sz, size_t start = 0) const
		{
			return !m_ptr ? npos : m_ptr->find(sz,start);
		}

#if defined(__GNUC__)
		int printf(const char* format, ...) __attribute__((format(printf,2,3)))
#else
		int printf(const char* format, ...)
#endif
		{
			va_list args;
			va_start(args,format);

			int err = vprintf(format,args);

			va_end(args);

			return err;
		}

		int vprintf(const char* format, va_list args)
		{
			node_t ptr;
			int err = new_node(ptr,m_ptr);
			if (err)
				return err;

			if ((err = ptr->vprintf(format,args)))
				return err;

			m_ptr.swap(ptr);
			return 0;
		}

#if defined(_WIN32)
		int wchar_t_to_utf8(const wchar_t* wsz)
		{
			node_t ptr;
			int err = new_node(ptr,m_ptr);
			if (err)
				return err;

			if ((err = ptr->wchar_t_to_utf8(wsz)))
				return err;

			m_ptr.swap(ptr);
			return 0;
		}
#endif

	private:
		node_t new_node()
		{
			return allocate_shared<ScopedStringImpl<CrtAllocator>,CrtAllocator>();
		}

		int new_node(node_t& n, const node_t& rhs)
		{
			n = new_node();
			if (!n)
				return ERROR_OUTOFMEMORY;

			if (!rhs)
				return 0;

			return n->assign(rhs->c_str(),rhs->length());
		}

		node_t m_ptr;
	};

	typedef ScopedStringImpl<ThreadLocalAllocator> ScopedString;
}

#endif // OOBASE_STRING_H_INCLUDED_
