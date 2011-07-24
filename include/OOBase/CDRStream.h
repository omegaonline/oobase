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

#ifndef OOBASE_CDR_STREAM_H_INCLUDED_
#define OOBASE_CDR_STREAM_H_INCLUDED_

#include "Buffer.h"
#include "ByteSwap.h"
#include "String.h"

namespace OOBase
{
	class CDRStream
	{
	public:
		static const int MaxAlignment = 8;

		CDRStream(size_t len = 256) :
#if (OOBASE_BYTE_ORDER == OMEGA_BIG_ENDIAN)
				m_big_endian(true),
#else
				m_big_endian(false),
#endif
				m_last_error(0)
		{
			if (len > 0)
			{
				m_buffer = new (std::nothrow) Buffer(len,MaxAlignment);
				if (!m_buffer)
					m_last_error = ERROR_OUTOFMEMORY;
			}
		}

		CDRStream(Buffer* buffer) :
				m_buffer(buffer),
#if (OOBASE_BYTE_ORDER == OMEGA_BIG_ENDIAN)
				m_big_endian(true),
#else
				m_big_endian(false),
#endif
				m_last_error(0)
		{ 
			if (m_buffer)
				m_buffer->addref();
		}

		CDRStream(const CDRStream& rhs) :
				m_buffer(rhs.m_buffer),
				m_big_endian(rhs.m_big_endian),
				m_last_error(rhs.m_last_error)
		{
		}

		CDRStream& operator = (const CDRStream& rhs)
		{
			if (&rhs != this)
			{
				m_buffer = rhs.m_buffer;
				m_big_endian = rhs.m_big_endian;
				m_last_error = rhs.m_last_error;
			}
			return *this;
		}

		~CDRStream()
		{ 
		}

		const Buffer* buffer() const
		{
			return m_buffer;
		}

		Buffer* buffer()
		{
			return m_buffer;
		}

		int reset()
		{
			if (!m_last_error && m_buffer)
				m_last_error = m_buffer->reset(MaxAlignment);
			return m_last_error;
		}

		int compact()
		{
			if (!m_last_error && m_buffer)
				m_last_error = m_buffer->compact(MaxAlignment);
			return m_last_error;
		}

		void big_endian(bool be)
		{
			m_big_endian = be;
		}

		bool big_endian() const
		{
			return m_big_endian;
		}

		int last_error() const
		{
			return m_last_error;
		}

		template <typename T>
		T byte_swap(const T& val) const
		{
#if (OOBASE_BYTE_ORDER == OMEGA_BIG_ENDIAN)
			return (m_big_endian ? val : OOBase::byte_swap(val));
#else
			return (!m_big_endian ? val : OOBase::byte_swap(val));
#endif
		}

		/** Templatized variable read function.
		 *  This function reads a value of type \p T, and advances rd_ptr() by \p sizeof(T).
		 *  \return \p true on sucess or \p false if length() < \p sizeof(T).
		 */
		template <typename T>
		bool read(T& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_buffer->align_rd_ptr(sizeof(T));
			if (m_buffer->length() < sizeof(T))
			{
#if defined(_WIN32)
				m_last_error = ERROR_HANDLE_EOF;
#elif defined(HAVE_UNISTD_H)
				m_last_error = ENOSPC;
#else
#error Fix me!
#endif
				return false;
			}

			val = byte_swap(*reinterpret_cast<const T*>(m_buffer->rd_ptr()));
			m_buffer->rd_ptr(sizeof(T));
			return true;
		}

		/** A specialization of read() for type \p LocalString.
		 */
		bool read(LocalString& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			// We do this because we haven't got a safe uint32_t type
			unsigned char len_buf[4] = {0};
			if (read_bytes(len_buf,sizeof(len_buf)) != sizeof(len_buf))
				return false;

			size_t len = 0;
			if (m_big_endian)
			{
				len = len_buf[0] << (3*8);
				len += len_buf[1] << (2*8);
				len += len_buf[2] << (1*8);
				len += len_buf[3];
			}
			else
			{
				len = len_buf[3] << (3*8);
				len += len_buf[2] << (2*8);
				len += len_buf[1] << (1*8);
				len += len_buf[0];
			}

			if (len == 0)
				val.clear();
			else
			{
				m_last_error = val.assign(m_buffer->rd_ptr(),len);
				if (m_last_error == 0)
					m_buffer->rd_ptr(len);				
			}
			return (m_last_error == 0);
		}

		/** A specialization of read() for type \p bool.
		 */
		bool read(bool& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			if (m_buffer->length() < 1)
			{
#if defined(_WIN32)
				m_last_error = ERROR_HANDLE_EOF;
#elif defined(HAVE_UNISTD_H)
				m_last_error = ENOSPC;
#else
#error Fix me!
#endif
				return false;
			}

			val = (*m_buffer->rd_ptr() == 0 ? false : true);
			m_buffer->rd_ptr(1);
			return true;
		}

		size_t read_bytes(unsigned char* buffer, size_t count)
		{
			if (m_last_error != 0 || !m_buffer)
				return 0;

			if (count > m_buffer->length())
				count = m_buffer->length();

			memcpy(buffer,m_buffer->rd_ptr(),count);
			m_buffer->rd_ptr(count);
			return count;
		}

		/** Templatized variable write function.
		 *  This function writes a value of type \p T, and advances wr_ptr() by \p sizeof(T).
		 *  This function will call space() to increase the internal buffer capacity.
		 *  \return \p true on sucess or \p false if there is no more heap available.
		 */
		template <typename T>
		bool write(const T& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_last_error = m_buffer->align_wr_ptr(sizeof(T));
			if (m_last_error != 0)
				return false;

			m_last_error = m_buffer->space(sizeof(T));
			if (m_last_error != 0)
				return false;

			*reinterpret_cast<T*>(m_buffer->wr_ptr()) = byte_swap(val);
			m_buffer->wr_ptr(sizeof(T));

			return true;
		}

		/// A specialization of write() for type \p const char*.
		bool write(const char* pszText, size_t len = (size_t)-1)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			if (!pszText)
				len = 0;
			else if (len == (size_t)-1)
				len = strlen(pszText);

			if (len >= 0xFFFFFFFF)
			{
#if defined(_WIN32)
				m_last_error = ERROR_BUFFER_OVERFLOW;
#elif defined(HAVE_UNISTD_H)
				m_last_error = E2BIG;
#else
				#error Fix me!
#endif
				return false;
			}

			// We do this because we haven't got a safe uint32_t type
			unsigned char len_buf[4] = {0};
			if (m_big_endian)
			{
				len_buf[0] = static_cast<unsigned char>(len >> (3*8));
				len_buf[1] = static_cast<unsigned char>((len & 0x00FF0000) >> (2*8));
				len_buf[2] = static_cast<unsigned char>((len & 0x0000FF00) >> (1*8));
				len_buf[3] = static_cast<unsigned char>(len & 0x000000FF);
			}
			else
			{
				len_buf[3] = static_cast<unsigned char>(len >> (3*8));
				len_buf[2] = static_cast<unsigned char>((len & 0x00FF0000) >> (2*8));
				len_buf[1] = static_cast<unsigned char>((len & 0x0000FF00) >> (1*8));
				len_buf[0] = static_cast<unsigned char>(len & 0x000000FF);
			}

			// Write the length first
			if (!write_bytes(len_buf,sizeof(len_buf)))
				return false;

			// Then the bytes of the string
			m_last_error = m_buffer->space(len);
			if (m_last_error != 0)
				return false;

			memcpy(m_buffer->wr_ptr(),pszText,len);
			m_buffer->wr_ptr(len);

			return true;
		}

		/// A specialization of write() for type \p bool.
		bool write(bool val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_last_error = m_buffer->space(1);
			if (m_last_error != 0)
				return false;

			*m_buffer->wr_ptr() = (val ? 1 : 0);
			m_buffer->wr_ptr(1);
			return true;
		}

		bool write_bytes(const unsigned char* buffer, size_t count)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_last_error = m_buffer->space(count);
			if (m_last_error != 0)
				return false;

			memcpy(m_buffer->wr_ptr(),buffer,count);
			m_buffer->wr_ptr(count);
			return true;
		}

		size_t write_buffer(const Buffer* buffer)
		{
			if (m_last_error != 0 || !m_buffer)
				return 0;

			size_t count = buffer->length();
			m_last_error = m_buffer->space(count);
			if (m_last_error != 0)
				return 0;

			memcpy(m_buffer->wr_ptr(),buffer->rd_ptr(),count);
			m_buffer->wr_ptr(count);

			return count;
		}

		/** Templatized variable replace function.
		 *  This function writes a value of type \p T, at position \p mark.
		 *  This function does no buffer expansion or alignment.
		 */
		template <typename T>
		void replace(const T& val, size_t mark)
		{
			if (m_buffer)
			{
				size_t mark_cur = m_buffer->mark_wr_ptr();
				m_buffer->mark_wr_ptr(mark);
				write(val);
				m_buffer->mark_wr_ptr(mark_cur);
			}
		}

	private:
		RefPtr<Buffer> m_buffer;
		bool           m_big_endian;
		int            m_last_error;
	};
}

#endif // OOBASE_CDR_STREAM_H_INCLUDED_
