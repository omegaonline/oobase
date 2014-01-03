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

	namespace detail
	{
		namespace strings
		{
			static const size_t npos = size_t(-1);

			struct StringNode
			{
				Atomic<size_t> m_refcount;
				size_t         m_length;
				char           m_data[1];
			};

			struct StringNodeAllocator
			{
				Atomic<size_t>     m_refcount;
				size_t             m_length;
				AllocatorInstance* m_allocator;
				char               m_data[1];
			};

			int grow(size_t inc, StringNode*& node, void* (*pfnAllocate)(size_t,size_t), void (*pfnFree)(void*));
			int grow(size_t inc, StringNodeAllocator*& node);
		}

		template <typename Allocator>
		class StringBase
		{
		public:
			int assign(const char* sz, size_t len = strings::npos)
			{
				int err = 0;
				len = plen(sz,len);
				if (len)
				{
					if ((err = clone(len)) == 0)
						memcpy(this->m_node->m_data,sz,len);
				}
				else
					node_release(this->m_node);

				return err;
			}

#if defined(__GNUC__)
			int printf(const char* format, ...) __attribute__((format(printf,2,3)))
#else
			int printf(const char* format, ...)
#endif
			{
				va_list args;
				va_start(args,format);

				ScopedArrayPtr<char> ptr;
				int err = OOBase::vprintf(ptr,format,args);

				va_end(args);

				if (!err)
					assign(ptr.get());

				return err;
			}

		protected:
			StringBase() : m_node(NULL)
			{}

			StringBase(const StringBase& rhs) : m_node(rhs.m_node)
			{
				if (m_node)
					++m_node->m_refcount;
			}

			StringBase& operator = (const StringBase& rhs)
			{
				if (this != &rhs)
					node_replace(m_node,rhs.m_node);

				return *this;
			}

			strings::StringNode* m_node;

			static void node_replace(strings::StringNode*& node, strings::StringNode* repl)
			{
				node_release(node);
				node = repl;
				if (node)
					++node->m_refcount;
			}

			static void node_release(strings::StringNode*& node)
			{
				if (node && --node->m_refcount == 0)
				{
					Allocator::free(node);
					node = NULL;
				}
			}

			int grow(size_t inc)
			{
				return strings::grow(inc,m_node,&Allocator::allocate,&Allocator::free);
			}

			int clone(size_t len)
			{
				size_t our_len = m_node ? m_node->m_length : 0;
				if (len > our_len)
					return grow(len - our_len);

				int err = grow(0);
				if (!err)
				{
					m_node->m_data[len] = '\0';
					m_node->m_length = len;
				}
				return err;
			}

			static const size_t plen(const char* sz, size_t len)
			{
				if (!sz)
					len = 0;
				else if (len == strings::npos)
					len = strlen(sz);
				return len;
			}
		};

		template <>
		class StringBase<AllocatorInstance>
		{
		public:
			int assign(const char* sz, size_t len = strings::npos)
			{
				len = plen(sz,len);

				int err = clone(len);
				if (!err && len)
					memcpy(this->m_node->m_data,sz,len);

				return err;
			}

			AllocatorInstance& get_allocator() const
			{
				if (!m_node)
					OOBase_CallCriticalFailure(ERROR_OUTOFMEMORY);
				return *m_node->m_allocator;
			}

#if defined(__GNUC__)
			int printf(const char* format, ...) __attribute__((format(printf,2,3)))
#else
			int printf(const char* format, ...)
#endif
			{
				if (!m_node)
					return ERROR_OUTOFMEMORY;

				va_list args;
				va_start(args,format);

				ScopedArrayPtr<char,AllocatorInstance> ptr(*m_node->m_allocator);
				int err = OOBase::vprintf(ptr,format,args);

				va_end(args);

				if (!err)
					assign(ptr.get());
				return err;
			}

		protected:
			StringBase(AllocatorInstance& allocator) : m_node(NULL)
			{
				m_node = static_cast<strings::StringNodeAllocator*>(allocator.allocate(sizeof(strings::StringNodeAllocator),alignment_of<strings::StringNodeAllocator>::value));
				if (m_node)
				{
					m_node->m_refcount = 1;
					m_node->m_allocator = &allocator;
					m_node->m_length = 0;
					m_node->m_data[0] = '\0';
				}
			}

			StringBase(const StringBase& rhs) : m_node(rhs.m_node)
			{
				if (m_node)
					++m_node->m_refcount;
			}

			StringBase& operator = (const StringBase& rhs)
			{
				if (this != &rhs)
					node_replace(m_node,rhs.m_node);

				return *this;
			}

			strings::StringNodeAllocator* m_node;

			static void node_replace(strings::StringNodeAllocator*& node, strings::StringNodeAllocator* repl)
			{
				node_release(node);
				node = repl;
				if (node)
					++node->m_refcount;
			}

			static void node_release(strings::StringNodeAllocator*& node)
			{
				if (node && --node->m_refcount == 0)
				{
					node->m_allocator->free(node);
					node = NULL;
				}
			}

			int grow(size_t inc)
			{
				return strings::grow(inc,m_node);
			}

			int clone(size_t len)
			{
				size_t our_len = m_node ? m_node->m_length : 0;
				if (len > our_len)
					return grow(len - our_len);

				int err = grow(0);
				if (!err)
				{
					m_node->m_data[len] = '\0';
					m_node->m_length = len;
				}
				return err;
			}

			static const size_t plen(const char* sz, size_t len)
			{
				if (!sz)
					len = 0;
				else if (len == strings::npos)
					len = strlen(sz);
				return len;
			}
		};

		template <typename Allocator>
		class StringImpl : public StringBase<Allocator>
		{
			typedef StringBase<Allocator> baseClass;

		public:
			static const size_t npos = size_t(-1);

			StringImpl() : baseClass()
			{}

			StringImpl(AllocatorInstance& allocator) : baseClass(allocator)
			{}

			~StringImpl()
			{
				baseClass::node_release(this->m_node);
			}

			int assign(const char* sz, size_t len = npos)
			{
				return baseClass::assign(sz,len);
			}

			int assign(const StringImpl& rhs)
			{
				baseClass::node_replace(this->m_node,rhs.m_node);
				return 0;
			}

			template <typename A>
			int assign(const StringImpl<A>& str)
			{
				return baseClass::assign(str.c_str(),str.length());
			}

			int append(const char* sz, size_t len = npos)
			{
				int err = 0;
				len = baseClass::plen(sz,len);
				if (len)
				{
					size_t our_len = length();
					if ((err = baseClass::grow(len)) == 0)
						memcpy(this->m_node->m_data+our_len,sz,len);
				}

				return err;
			}

			template <typename A>
			int append(const StringImpl<A>& str)
			{
				return append(str.c_str(),str.length());
			}

			int replace_all(char from, char to)
			{
				bool copied = false;
				size_t i = length();
				while (i > 0)
				{
					--i;
					if (this->m_node->m_data[i] == from)
					{
						if (!copied)
						{
							int err = baseClass::grow(0);
							if (err != 0)
								return err;

							copied = true;
						}
						this->m_node->m_data[i] = to;
					}
				}
				return 0;
			}

			int truncate(size_t len)
			{
				int err = 0;
				if (len < length())
					err = baseClass::clone(0);

				return err;
			}

			int concat(const char* sz1, const char* sz2)
			{
				int err = baseClass::assign(sz1);
				if (err == 0)
					err = append(sz2);

				return err;
			}

			int compare(const char* rhs) const
			{
				if (this->m_node == NULL)
					return (rhs == NULL ? 0 : -1);
				else if (rhs == NULL)
					return 1;

				return strcmp(this->m_node->m_data,rhs);
			}

			int compare(const StringImpl& rhs) const
			{
				if (this == &rhs)
					return true;
				else
					return this->compare_i(rhs.c_str(),rhs.length());
			}

			template <typename A>
			int compare(const StringImpl<A>& rhs) const
			{
				return this->compare_i(rhs.c_str(),rhs.length());
			}

			template <typename T> bool operator == (T rhs) const { return (this->compare(rhs) == 0); }
			template <typename T> bool operator < (T rhs) const { return (this->compare(rhs) < 0); }
			template <typename T> bool operator <= (T rhs) const { return (this->compare(rhs) <= 0); }
			template <typename T> bool operator > (T rhs) const { return (this->compare(rhs) > 0); }
			template <typename T> bool operator >= (T rhs) const { return (this->compare(rhs) >= 0); }
			template <typename T> bool operator != (T rhs) const { return (this->compare(rhs) != 0); }

			void clear()
			{
				baseClass::assign(NULL,0);
			}

			size_t length() const
			{
				return (this->m_node ? this->m_node->m_length : 0);
			}

			bool empty() const
			{
				return length() == 0;
			}

			const char* c_str() const
			{
				return (length() ? this->m_node->m_data : "\0");
			}

			char operator [] (size_t idx) const
			{
				return (idx < length() ? this->m_node->m_data[idx] : '\0');
			}

			bool replace_at(size_t idx, char c)
			{
				if (idx >= length())
					return false;

				this->m_node->m_data[idx] = c;
				return true;
			}

			size_t find(char c, size_t start = 0) const
			{
				if (start >= length())
					return npos;

				const char* p = static_cast<const char*>(memchr(this->m_node->m_data + start,c,this->m_node->m_length - start));
				if (!p)
					return npos;

				// Returns *absolute* position
				return static_cast<size_t>(p - this->m_node->m_data);
			}

			size_t find(const char* sz, size_t start = 0) const
			{
				if (!sz)
					return 0;

				size_t len = strlen(sz);
				if (!len)
					return 0;

				size_t pos = find(sz[0],start);
				while (pos != npos)
				{
					if (this->m_node->m_length - pos < len)
						return npos;

					if (memcmp(this->m_node->m_data + pos,sz,len) == 0)
						break;

					pos = find(sz[0],pos+1);
				}

				return pos;
			}

		private:
			int compare_i(const char* sz, size_t len) const
			{
				len = baseClass::plen(sz,len);
				if (this->m_node == NULL)
					return (len == 0 ? 0 : -1);
				else if (!len)
					return 1;

				size_t min = (len > this->m_node->m_length ? this->m_node->m_length : len);
				int r = memcmp(this->m_node->m_data,sz,min);
				if (r != 0)
					return r;

				if (len > this->m_node->m_length)
					return -1;

				if (len < this->m_node->m_length)
					return 1;

				return 0;
			}
		};
	}

	typedef detail::StringImpl<CrtAllocator> String;

	template <typename Allocator = ThreadLocalAllocator>
	class ScopedString : public NonCopyable
	{
	public:
		static const size_t npos = size_t(-1);

		ScopedString() : m_data()
		{}

		ScopedString(AllocatorInstance& allocator) : m_data(allocator)
		{}

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
}

#endif // OOBASE_STRING_H_INCLUDED_
