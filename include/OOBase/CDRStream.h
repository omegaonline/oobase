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
		static const int MaxAlignment = 16;

		CDRStream(size_t len = 256) :
				m_endianess(OOBASE_BYTE_ORDER),
				m_last_error(0)
		{
			if (len > 0)
			{
				m_buffer = Buffer::create(len,MaxAlignment);
				if (!m_buffer)
					m_last_error = ERROR_OUTOFMEMORY;
			}
		}

		CDRStream(AllocatorInstance& allocator, size_t len = 256) :
				m_endianess(OOBASE_BYTE_ORDER),
				m_last_error(0)
		{
			m_buffer = Buffer::create(allocator,len,MaxAlignment);
			if (!m_buffer)
				m_last_error = ERROR_OUTOFMEMORY;
		}

		CDRStream(const RefPtr<Buffer>& buffer) :
				m_buffer(buffer),
				m_endianess(OOBASE_BYTE_ORDER),
				m_last_error(0)
		{ }

		CDRStream(const CDRStream& rhs) :
				m_buffer(rhs.m_buffer),
				m_endianess(rhs.m_endianess),
				m_last_error(rhs.m_last_error)
		{ }

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

		const RefPtr<Buffer>& buffer() const
		{
			return m_buffer;
		}

		RefPtr<Buffer>& buffer()
		{
			return m_buffer;
		}

		size_t length() const
		{
			return (m_buffer ? m_buffer->length() : 0);
		}

		void reset()
		{
			if (m_buffer)
				m_buffer->reset();
		}

		void compact()
		{
			if (m_buffer)
				m_buffer->compact();
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

		void endianess(uint16_t be)
		{
			m_endianess = be;
		}

		uint16_t endianess() const
		{
			return m_endianess;
		}

		bool write_endianess()
		{
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_too_big();
				
			m_last_error = m_buffer->align_wr_ptr(2);
			if (m_last_error != 0)
				return false;

			m_last_error = m_buffer->space(2);
			if (m_last_error != 0)
				return false;

			uint8_t* wr_ptr = m_buffer->wr_ptr();
			wr_ptr[0] = static_cast<uint8_t>(m_endianess >> 8);
			wr_ptr[1] = static_cast<uint8_t>(m_endianess & 0xFF);

			m_buffer->wr_ptr(2);

			return true;
		}

		bool read_endianess()
		{
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_eof();

			m_buffer->align_rd_ptr(2);
			if (m_buffer->length() < 2)
				return error_eof();

			const uint8_t* rd_ptr = m_buffer->rd_ptr();
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
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_eof();
				
			m_buffer->align_rd_ptr(alignment_of<T>::value);
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
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_eof();

			if (m_buffer->length() < 1)
				return error_eof();

			val = (*m_buffer->rd_ptr() != 0);
			m_buffer->rd_ptr(1);
			return true;
		}

		/** Read a string.
		 */
		template <typename S>
		bool read_string(S& val)
		{
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_eof();

			size_t len = 0;
			if (!read_dyn_int(len))
				return false;

			if (len == 0)
			{
				val.clear();
				return true;
			}

			if (len > m_buffer->length())
				return error_too_big();

			m_last_error = val.assign((const char*)m_buffer->rd_ptr(),len);
			if (m_last_error == 0)
				m_buffer->rd_ptr(len);

			return (m_last_error == 0);
		}

		size_t read_bytes(uint8_t* buffer, size_t count)
		{
			if (m_last_error != 0)
				return 0;

			if (!m_buffer)
			{
				error_eof();
				return 0;
			}

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
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_too_big();
				
			m_last_error = m_buffer->align_wr_ptr(alignment_of<T>::value);
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
		bool write(const char* pszText, size_t len = size_t(-1))
		{
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_too_big();

			if (!pszText)
				len = 0;
			else if (len == size_t(-1))
				len = strlen(pszText);

			if (!write_dyn_int(len))
				return false;

			// Then the bytes of the string
			return write_bytes(reinterpret_cast<const uint8_t*>(pszText),len);
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
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_too_big();

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

		bool write_bytes(const uint8_t* buffer, size_t count)
		{
			if (m_last_error != 0)
				return false;

			if (!m_buffer)
				return error_too_big();

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

		size_t write_buffer(const RefPtr<Buffer>& buffer)
		{
			if (m_last_error != 0)
				return 0;

			if (!m_buffer)
			{
				error_too_big();
				return 0;
			}

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

		void swap(CDRStream& rhs)
		{
			m_buffer.swap(rhs.m_buffer);
			OOBase::swap(m_endianess,rhs.m_endianess);
			OOBase::swap(m_last_error,rhs.m_last_error);
		}

	private:
		RefPtr<Buffer> m_buffer;
		uint16_t       m_endianess;
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
			m_last_error = ENOSPC;
#endif
			return false;
		}

		bool read_dyn_int(size_t& len)
		{
			len = 0;
			for (size_t i=0;;i+=7)
			{
				uint8_t b = 0;
				if (!read(b))
					return false;

				size_t v = size_t(b & 0x7F) << i;
				if (len > size_t(-1) - v)
					return error_too_big();

				len += v;

				// If hi bit is set, read another byte
				if (!(b & 0x80))
					return true;
			}
		}

		bool write_dyn_int(size_t len)
		{
			do
			{
				uint8_t b = (len & 0x7F);
				if (len > 0x7F)
					b |= 0x80;

				if (!write(b))
					return false;

				len >>= 7;
			}
			while (len);

			return true;
		}
	};
}

#endif // OOBASE_CDR_STREAM_H_INCLUDED_
