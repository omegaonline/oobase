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
#include "Win32.h"

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

			if (!ptr.resize(static_cast<size_t>(r) + 1))
				return system_error();
		}
	}

	template <typename A, size_t S>
	inline int printf(ScopedArrayPtr<char,A,S>& ptr, const char* format, ...) OOBASE_FORMAT(printf,2,3);

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

		ScopedStringImpl() : m_data(), m_len(0)
		{}

		ScopedStringImpl(AllocatorInstance& allocator) : m_data(allocator), m_len(0)
		{}

		int compare(const char* rhs) const
		{
			if (!m_len)
				return (rhs == NULL ? 0 : -1);
			else if (rhs == NULL)
				return 1;

			return strcmp(m_data.get(),rhs);
		}

		template <typename A1>
		int compare(const ScopedStringImpl<A1>& rhs) const
		{
			if (this == &rhs)
				return 0;
			return this->compare(rhs.c_str());
		}

		bool assign(const char* sz, size_t len = npos)
		{
			if (!sz)
				len = 0;

			if (len == npos)
				len = strlen(sz);

			if (!m_data.resize(len + 1))
				return false;

			if (len)
				memcpy(m_data.get(),sz,len);

			m_data[len] = '\0';
			m_len = len;
			return true;
		}

		bool append(const char* sz, size_t len = npos)
		{
			if (len == npos)
				len = strlen(sz);

			if (len)
			{
				size_t orig_len = m_len;
				if (!m_data.resize(orig_len + len + 1))
					return false;

				memcpy(m_data.get() + orig_len,sz,len);
				m_data[orig_len + len] = '\0';
				m_len += len;
			}
			return true;
		}

		bool append(char c)
		{
			return append(&c,1);
		}

		const char* c_str() const
		{
			return m_len ? m_data.get() : NULL;
		}

		char& operator [](ptrdiff_t i) const
		{
			assert(i >= 0 && size_t(i) < m_len);
			return m_data[i];
		}

		bool empty() const
		{
			return (m_len == 0);
		}

		void clear()
		{
			m_len = 0;
		}

		size_t length() const
		{
			return m_len;
		}

		size_t find(char c, size_t start = 0) const
		{
			if (!m_len)
				return npos;

			if (start > m_len)
				return npos;

			const char* f = strchr(m_data.get() + start,c);
			return f ? size_t(f - m_data.get()) : npos;
		}

		size_t find(const char* sz, size_t start = 0) const
		{
			if (!m_len)
				return npos;

			if (start > m_len)
				return npos;
			
			const char* f = strstr(m_data.get() + start,sz);
			return f ? size_t(f - m_data.get()) : npos;
		}

		int printf(const char* format, ...) OOBASE_FORMAT(printf,2,3)
		{
			va_list args;
			va_start(args,format);

			int err = vprintf(format,args);

			va_end(args);

			return err;
		}

		int vprintf(const char* format, va_list args)
		{
			int err = OOBase::vprintf(m_data,format,args);
			if (!err)
				m_len = strlen(m_data.get());
			return err;
		}

		AllocatorInstance& get_allocator() const
		{
			return m_data.get_allocator();
		}

#if defined(_WIN32)
		int wchar_t_to_utf8(const wchar_t* wsz)
		{
			int err = Win32::wchar_t_to_utf8(wsz,m_data);
			if (!err)
				m_len = strlen(m_data.get());
			return err;
		}
#endif

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return stream.read_string(*this);
		}

		bool write(CDRStream& stream) const
		{
			return stream.write(c_str(),m_len);
		}
#endif

	private:
		ScopedArrayPtr<char,Allocator,24> m_data;
		size_t                            m_len;
	};

	template<typename A1, typename T>
	bool operator == (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) == 0;
	}

	template<typename A1, typename T>
	bool operator != (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) != 0;
	}

	template<typename A1, typename T>
	bool operator < (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) < 0;
	}

	template<typename A1, typename T>
	bool operator <= (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) <= 0;
	}

	template<typename A1, typename T>
	bool operator > (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) > 0;
	}

	template<typename A1, typename T>
	bool operator >= (const ScopedStringImpl<A1>& str1, T str2)
	{
		return str1.compare(str2) >= 0;
	}

	template <typename Allocator>
	class SharedString
	{
		typedef SharedPtr<ScopedStringImpl<Allocator> > node_t;

	public:
		static const size_t npos = ScopedStringImpl<Allocator>::npos;

		SharedString() : m_ptr()
		{}

		SharedString(const SharedString& rhs) : m_ptr(rhs.m_ptr)
		{}

		SharedString& operator = (const SharedString& rhs)
		{
			SharedString(rhs).swap(*this);
			return *this;
		}

		void swap(SharedString& rhs)
		{
			m_ptr.swap(rhs.m_ptr);
		}

		int compare(const char* rhs) const
		{
			if (!m_ptr)
				return (rhs == NULL ? 0 : -1);

			return m_ptr->compare(rhs);
		}

		int compare(const SharedString& rhs) const
		{
			if (this == &rhs)
				return true;

			return compare(rhs.c_str());
		}

		template <typename A2>
		int compare(const SharedString<A2>& rhs) const
		{
			return compare(rhs.c_str());
		}

		template <typename A2>
		int compare(const ScopedStringImpl<A2>& rhs) const
		{
			return (m_ptr ? m_ptr->compare(rhs) : compare(rhs.c_str()));
		}

		bool assign(const char* sz, size_t len = npos)
		{
			if (sz && len)
			{
				node_t ptr = new_node();
				if (!ptr || !ptr->assign(sz,len))
					return false;

				m_ptr.swap(ptr);
			}
			return true;
		}

		bool append(const char* sz, size_t len = npos)
		{
			if (sz && len)
			{
				node_t ptr;
				if (!clone_node(ptr,m_ptr) || !ptr->append(sz,len))
					return false;

				m_ptr.swap(ptr);
			}
			return true;
		}

		bool append(char c)
		{
			return append(&c,1);
		}

		bool append(const SharedString& rhs)
		{
			if (!m_ptr)
			{
				m_ptr = rhs.m_ptr;
				return true;
			}
			return append(rhs.c_str(),rhs.length());
		}

		const char* c_str() const
		{
			return !m_ptr ? NULL : m_ptr->c_str();
		}

		operator ScopedStringImpl<Allocator>& ()
		{
			if (!m_ptr && !(m_ptr = new_node()))
				OOBase_CallCriticalFailure(system_error());

			return *m_ptr;
		}

		char operator [](ptrdiff_t i) const
		{
			return !m_ptr ? 0 : m_ptr->operator [](i);
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

		int printf(const char* format, ...) OOBASE_FORMAT(printf,2,3)
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
			if (!clone_node(ptr,m_ptr))
				return system_error();

			int err = ptr->vprintf(format,args);
			if (err != 0)
				return err;

			m_ptr.swap(ptr);
			return 0;
		}

#if defined(_WIN32)
		int wchar_t_to_utf8(const wchar_t* wsz)
		{
			node_t ptr;
			if (!clone_node(ptr,m_ptr))
				return system_error();

			int err = ptr->wchar_t_to_utf8(wsz);
			if (err != 0)
				return err;

			m_ptr.swap(ptr);
			return 0;
		}
#endif

#if defined(OOBASE_CDR_STREAM_H_INCLUDED_)
		bool read(CDRStream& stream)
		{
			return stream.read_string(*this);
		}

		bool write(CDRStream& stream) const
		{
			return stream.write(m_ptr->c_str(),m_ptr->length());
		}
#endif

	private:
		node_t new_node()
		{
			return allocate_shared<ScopedStringImpl<Allocator>,Allocator>();
		}

		bool clone_node(node_t& n, const node_t& rhs)
		{
			n = new_node();
			if (!n)
				return false;

			if (!rhs)
				return true;

			return n->assign(rhs->c_str(),rhs->length());
		}

		node_t m_ptr;
	};

	template <typename Allocator, typename T>
	bool operator == (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) == 0;
	}

	template <typename Allocator, typename T>
	bool operator != (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) != 0;
	}

	template <typename Allocator, typename T>
	bool operator < (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) < 0;
	}

	template <typename Allocator, typename T>
	bool operator <= (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) <= 0;
	}

	template <typename Allocator, typename T>
	bool operator > (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) > 0;
	}

	template <typename Allocator, typename T>
	bool operator >= (const SharedString<Allocator>& str1, T str2)
	{
		return str1.compare(str2) >= 0;
	}

	typedef SharedString<CrtAllocator> String;

	typedef ScopedStringImpl<ThreadLocalAllocator> ScopedString;
}

#endif // OOBASE_STRING_H_INCLUDED_
