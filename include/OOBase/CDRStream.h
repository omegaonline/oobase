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
#include "Timeout.h"

namespace OOBase
{
	class CDRStream
	{
	public:
		static const int MaxAlignment = 8;

		CDRStream(size_t len = 256) :
				m_endianess(OOBASE_BYTE_ORDER),
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
				m_endianess(OOBASE_BYTE_ORDER),
				m_last_error(0)
		{ 
			if (m_buffer)
				m_buffer->addref();
		}

		CDRStream(const CDRStream& rhs) :
				m_buffer(rhs.m_buffer),
				m_endianess(rhs.m_endianess),
				m_last_error(rhs.m_last_error)
		{
		}

		CDRStream& operator = (const CDRStream& rhs)
		{
			if (&rhs != this)
			{
				m_buffer = rhs.m_buffer;
				m_endianess = rhs.m_endianess;
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

		int last_error() const
		{
			return m_last_error;
		}

		template <typename T>
		T byte_swap(const T& val) const
		{
			return (m_endianess == OOBASE_BYTE_ORDER ? val : OOBase::byte_swap(val));
		}

		void endianess(unsigned short be)
		{
			m_endianess = be;
		}

		unsigned short endianess() const
		{
			return m_endianess;
		}

		bool write_endianess()
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_last_error = m_buffer->align_wr_ptr(2);
			if (m_last_error != 0)
				return false;

			m_last_error = m_buffer->space(2);
			if (m_last_error != 0)
				return false;

			char* wr_ptr = m_buffer->wr_ptr();
			wr_ptr[0] = static_cast<char>(m_endianess >> 8);
			wr_ptr[1] = static_cast<char>(m_endianess & 0xFF);

			m_buffer->wr_ptr(2);

			return true;
		}

		bool read_endianess()
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			m_buffer->align_rd_ptr(2);
			if (m_buffer->length() < 2)
				return error_eof();

			const char* rd_ptr = m_buffer->rd_ptr();
			m_endianess = (rd_ptr[0] << 8) | rd_ptr[1];
			m_buffer->rd_ptr(2);
			return true;
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

			m_buffer->align_rd_ptr(detail::alignof<T>::value);
			if (m_buffer->length() < sizeof(T))
				return error_eof();

			val = byte_swap(*reinterpret_cast<const T*>(m_buffer->rd_ptr()));
			m_buffer->rd_ptr(sizeof(T));
			return true;
		}

		/** A specialization of read() for type \p Timeout.
		 */
		bool read(Timeout& val)
		{
			return val.read(*this);
		}

		/** A specialization of read() for type \p bool.
		 */
		bool read(bool& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			if (m_buffer->length() < 1)
				return error_eof();

			val = (*m_buffer->rd_ptr() == 0 ? false : true);
			m_buffer->rd_ptr(1);
			return true;
		}

		/** Read a string.
		 */
		template <typename S>
		bool read_string(S& val)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			size_t len = 0;
			if (!read4(len))
				return false;

			if (len == 0)
			{
				val.clear();
				return true;
			}

			if (len > m_buffer->length())
				return error_too_big();

			m_last_error = val.assign(m_buffer->rd_ptr(),len);
			if (m_last_error == 0)
				m_buffer->rd_ptr(len);

			return (m_last_error == 0);
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

		/** Templatized socket recv function.
		 */
		template <typename S, typename STR>
		bool recv_string(S& pSocket, STR& str)
		{
			size_t mark = m_buffer->mark_rd_ptr();

			m_last_error = pSocket->recv(m_buffer,4);
			if (m_last_error != 0)
				return false;

			size_t len = 0;
			if (!read4(len))
				return false;

			if (len)
			{
				m_last_error = pSocket->recv(m_buffer,len);
				if (m_last_error != 0)
					return false;
			}

			// Now reset rd_ptr and read the string
			m_buffer->mark_rd_ptr(mark);

			return read_string(str);
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

			m_last_error = m_buffer->align_wr_ptr(detail::alignof<T>::value);
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

			if (!write4(len))
				return false;

			// Then the bytes of the string
			return write_bytes(reinterpret_cast<const unsigned char*>(pszText),len);
		}

		/// A specialization of write() for character arrays.
		template <size_t S>
		bool write(const char (&arr)[S])
		{
			return write(arr,S);
		}

		/// A specialization of write() for type \p Timeout.
		bool write(const Timeout& val)
		{
			return val.write(*this);
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

		template <typename STR>
		bool write_string(const STR& str)
		{
			return write(str.c_str(),str.length());
		}

		bool write_bytes(const unsigned char* buffer, size_t count)
		{
			if (m_last_error != 0 || !m_buffer)
				return false;

			if (count)
			{
				m_last_error = m_buffer->space(count);
				if (m_last_error != 0)
					return false;

				memcpy(m_buffer->wr_ptr(),buffer,count);
				m_buffer->wr_ptr(count);
			}

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
		unsigned short m_endianess;
		int            m_last_error;

		bool error_eof()
		{
#if defined(_WIN32)
			m_last_error = ERROR_HANDLE_EOF;
#else
			m_last_error = ENOSPC;
#endif
			return false;
		}

		bool error_too_big()
		{
#if defined(_WIN32)
			m_last_error = ERROR_BUFFER_OVERFLOW;
#else
			m_last_error = E2BIG;
#endif
			return false;
		}

		bool read4(size_t& len)
		{
			// We do this because we haven't got a safe uint32_t type
			unsigned char len_buf[4] = {0};
			if (read_bytes(len_buf,4) != 4)
				return false;

			if (m_endianess == OOBASE_BIG_ENDIAN)
			{
				len = len_buf[0] << (3*8);
				len += len_buf[1] << (2*8);
				len += len_buf[2] << (1*8);
				len += len_buf[3];
				return true;
			}
			else if (m_endianess == OOBASE_LITTLE_ENDIAN)
			{
				len = len_buf[3] << (3*8);
				len += len_buf[2] << (2*8);
				len += len_buf[1] << (1*8);
				len += len_buf[0];
				return true;
			}
			else
				m_last_error = EINVAL;
			
			return false;
		}

		bool write4(size_t len)
		{
			if (len > 0xFFFFFFFF)
				return error_too_big();

			// We do this because we haven't got a safe uint32_t type
			unsigned char len_buf[4] = {0};
			if (m_endianess == OOBASE_BIG_ENDIAN)
			{
				len_buf[0] = static_cast<unsigned char>(len >> (3*8));
				len_buf[1] = static_cast<unsigned char>((len & 0x00FF0000) >> (2*8));
				len_buf[2] = static_cast<unsigned char>((len & 0x0000FF00) >> (1*8));
				len_buf[3] = static_cast<unsigned char>(len & 0x000000FF);
			}
			else if (m_endianess == OOBASE_LITTLE_ENDIAN)
			{
				len_buf[3] = static_cast<unsigned char>(len >> (3*8));
				len_buf[2] = static_cast<unsigned char>((len & 0x00FF0000) >> (2*8));
				len_buf[1] = static_cast<unsigned char>((len & 0x0000FF00) >> (1*8));
				len_buf[0] = static_cast<unsigned char>(len & 0x000000FF);
			}
			else
			{
				m_last_error = EINVAL;
				return false;
			}

			// Write the length first
			return write_bytes(len_buf,4);
		}
	};
}

#endif // OOBASE_CDR_STREAM_H_INCLUDED_
