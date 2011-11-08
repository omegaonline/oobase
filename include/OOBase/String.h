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

#include <string.h>

namespace OOBase
{
	class LocalString
	{
	public:
		static const size_t npos = size_t(-1);

		LocalString() : m_data(NULL)
		{}

		~LocalString()
		{
			OOBase::LocalAllocator::free(m_data);
		}

		int assign(const LocalString& str);
		int assign(const char* sz, size_t len = npos);
		int append(const char* sz, size_t len = npos);
		int replace(char from, char to);
		int truncate(size_t len);
		int printf(const char* format, ...);
		int vprintf(const char* format, va_list args);
		int getenv(const char* envvar);

		int concat(const char* sz1, const char* sz2)
		{
			int err = assign(sz1);
			if (err == 0)
				err = append(sz2);

			return err;
		}

		int compare(const char* rhs) const
		{
			if (m_data == NULL)
				return (rhs == NULL ? 0 : -1);
			else if (rhs == NULL)
				return 1;

			return strcmp(m_data,rhs);
		}

		int compare(const LocalString& rhs) const
		{
			return compare(rhs.m_data);
		}

		template <typename T> bool operator == (T rhs) const { return (compare(rhs) == 0); }
		template <typename T> bool operator < (T rhs) const { return (compare(rhs) < 0); }
		template <typename T> bool operator <= (T rhs) const { return (compare(rhs) <= 0); }
		template <typename T> bool operator > (T rhs) const { return (compare(rhs) > 0); }
		template <typename T> bool operator >= (T rhs) const { return (compare(rhs) >= 0); }
		template <typename T> bool operator != (T rhs) const { return (compare(rhs) != 0); }

		void clear()
		{
			assign(NULL,0);
		}

		size_t length() const
		{
			return (m_data ? strlen(m_data) : 0);
		}

		bool empty() const
		{
			return (m_data ? m_data[0] == '\0' : true);
		}

		const char* c_str() const
		{
			return (m_data ? m_data : "\0");
		}

		char operator [] (size_t idx) const
		{
			if (idx >= length())
				return '\0';

			return m_data[idx];
		}

		size_t find(char c, size_t start = 0) const;
		size_t find(const char* sz, size_t start = 0) const;

	private:
		// Do not allow copy constructors or assignment
		// as memory allocation will occur...
		LocalString(const LocalString&);
		LocalString& operator = (const LocalString&);

		char*  m_data;
	};

	class String
	{
	public:
		static const size_t npos = size_t(-1);

		String() : m_node(NULL)
		{}

		String(const String& rhs) : m_node(rhs.m_node)
		{
			node_addref(m_node);
		}

		String& operator = (const String& rhs)
		{
			if (&rhs != this && m_node != rhs.m_node)
			{
				node_release(m_node);
				m_node = rhs.m_node;
				node_addref(m_node);
			}
			return *this;
		}

		~String()
		{
			node_release(m_node);
		}

		int assign(const String& str)
		{
			*this = str;
			return 0;
		}

		int assign(const char* sz, size_t len = npos);
		int append(const char* sz, size_t len = npos);
		int replace(char from, char to);
		int truncate(size_t len);
		int printf(const char* format, ...);

		int concat(const char* sz1, const char* sz2)
		{
			int err = assign(sz1);
			if (err == 0)
				err = append(sz2);

			return err;
		}

		int compare(const char* rhs) const
		{
			if (m_node == NULL)
				return (rhs == NULL ? 0 : -1);
			else if (rhs == NULL)
				return 1;

			return strcmp(m_node->m_data,rhs);
		}

		int compare(const String& rhs) const
		{
			return compare(rhs.m_node ? rhs.m_node->m_data : NULL);
		}

		template <typename T> bool operator == (T rhs) const { return (compare(rhs) == 0); }
		template <typename T> bool operator < (T rhs) const { return (compare(rhs) < 0); }
		template <typename T> bool operator <= (T rhs) const { return (compare(rhs) <= 0); }
		template <typename T> bool operator > (T rhs) const { return (compare(rhs) > 0); }
		template <typename T> bool operator >= (T rhs) const { return (compare(rhs) >= 0); }
		template <typename T> bool operator != (T rhs) const { return (compare(rhs) != 0); }

		void clear()
		{
			assign(NULL,0);
		}

		size_t length() const
		{
			return (m_node ? strlen(m_node->m_data) : 0);
		}

		bool empty() const
		{
			return (m_node ? m_node->m_data[0] == '\0' : true);
		}

		const char* c_str() const
		{
			return (m_node ? m_node->m_data : "\0");
		}

		char operator [] (size_t idx) const
		{
			if (idx >= length())
				return '\0';

			return m_node->m_data[idx];
		}

		size_t find(char c, size_t start = 0) const;
		size_t find(const char* sz, size_t start = 0) const;

	private:
		struct Node
		{
			Atomic<size_t> m_refcount;
			char           m_data[1];
		};
		Node* m_node;

		static void node_addref(Node* node);
		static void node_release(Node* node);
		int copy_on_write(size_t inc);
	};
	
	namespace Paths
	{
		template <typename T>
		inline int AppendDirSeparator(T& str)
		{
			int err = 0;
		#if defined(_WIN32)
			if (str[str.length()-1] != '\\' && str[str.length()-1] != '/')
				err = str.append("\\",1);
		#else
			if (str[str.length()-1] != '/')
				err = str.append("/",1);
		#endif
			return err;
		}

		template <typename T>
		inline int CorrectDirSeparators(T& str)
		{
		#if defined(_WIN32)
			return str.replace('/','\\');
		#else
			return str.replace('\\','/');
		#endif
		}

		template <typename T>
		inline int SplitDirAndFilename(const char* path, T& dir, T& filename)
		{
			int err = dir.assign(path);
			if (err == 0)
			{
				CorrectDirSeparators(dir);
			#if defined(_WIN32)
				static const char sep = '\\';
			#else
				static const char sep = '/';
			#endif
				char* s = strrchr(dir.c_str(),sep);
				if (s)
				{
					if ((err = dir.truncate(s-dir.c_str())) == 0)
						err = filename.assign(dir.c_str()+1);
				}
				return err;
			}
		}
	}
}

#endif // OOBASE_STRING_H_INCLUDED_
