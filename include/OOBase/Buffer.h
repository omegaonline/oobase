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

#ifndef OOBASE_BUFFER_H_INCLUDED_
#define OOBASE_BUFFER_H_INCLUDED_

#include "RefCount.h"
#include "Memory.h"

namespace OOBase
{
	/// Used to read and write data to a variable length memory buffer
	/**
	 *  A Buffer instance maintains two pointers, one to the read position: rd_ptr(),
	 *  and one to the write position: wr_ptr().
	 *
	 *  The size of the internal buffer maintained by the class is controlled by space(),
	 *  but care should be exercised because the values returned by rd_ptr() and wr_ptr()
	 *  might change due to reallocation.
	 *
	 *  The following diagram shows the relationships between the read and write pointers
	 *  and the internal buffer:
	 *
	 *  \verbatim
	      *.............*...............*..............*
	    m_buffer     rd_ptr()        wr_ptr()     m_capacity
	                    |<------------->|<------------>|
	                         length()        space()      \endverbatim
	 *  \warning This class is not thread-safe.
	 */
	class Buffer : public RefCounted
	{
	public:
		/// The factory function allocates the internal buffer to size \p cbSize using allocator \p allocator.
		static Buffer* create(size_t cbSize = 256, size_t align = 1);

		/// The factory function allocates the internal buffer to size \p cbSize.
		template <typename Allocator>
		static Buffer* create(size_t cbSize = 256, size_t align = 1);

		/// The factory function allocates the internal buffer to size \p cbSize using allocator \p allocator.
		static Buffer* create(AllocatorInstance& allocator, size_t cbSize = 256, size_t align = 1);

		/// Get the current read pointer value.
		const uint8_t* rd_ptr() const;

		/// Advance the read pointer by \p cbSkip bytes.
		void rd_ptr(size_t cbSkip);

		/// Advance the read pointer to \p align byte boundary.
		void align_rd_ptr(size_t align);

		/// Get the current write pointer value.
		uint8_t* wr_ptr();

		/// Advance the write pointer by \p cbExpand bytes.
		int wr_ptr(size_t cbExpand);

		/// Advance the write pointer to \p align byte boundary.
		int align_wr_ptr(size_t align);

		/// Get the used length of the buffer, the difference between rd_ptr() and wr_ptr().
		size_t length() const;

		/// Reset the read and write pointers to start().
		void reset();

		/// Move the read to start() and copy any existing data 'up' with it, setting wr_ptr() correctly.
		void compact();

		/// Get the amount of space remaining in bytes.
		size_t space() const;

		/// Adjust the amount of space remaining.
		int space(size_t cbSpace);

		/// Return rd_ptr as an offset
		size_t mark_rd_ptr() const;

		/// Move rd_ptr to mark
		void mark_rd_ptr(size_t mark);

		/// Return wr_ptr as an offset
		size_t mark_wr_ptr() const;

		/// Move wr_ptr to mark
		void mark_wr_ptr(size_t mark);

	protected:
		virtual uint8_t* reallocate(uint8_t* data, size_t size, size_t align) = 0;

		Buffer(uint8_t* buffer, size_t cbSize, size_t align);

		uint8_t* m_buffer;   ///< The actual underlying buffer.

	private:
		size_t   m_capacity; ///< The total allocated bytes for \p m_buffer.
		size_t   m_align;    ///< The alignment of the start of \p m_buffer.
		uint8_t* m_wr_ptr;   ///< The current write pointer.
		uint8_t* m_rd_ptr;   ///< The current read pointer.
	};

	namespace detail
	{
		template <typename Allocator>
		class BufferImpl : public Buffer, public Allocating<Allocator>
		{
			typedef Allocating<Allocator> baseClass;

		public:
			BufferImpl(void* buffer, size_t cbSize, size_t align) :
					Buffer(static_cast<uint8_t*>(buffer),cbSize,align),
					baseClass()
			{}

			BufferImpl(AllocatorInstance& allocator, void* buffer, size_t cbSize, size_t align) :
					Buffer(static_cast<uint8_t*>(buffer),cbSize,align),
					baseClass(allocator)
			{}

			static Buffer* create(size_t cbSize, size_t align)
			{
				void* buf = baseClass::allocate(cbSize,align);
				if (!buf)
					return NULL;

				BufferImpl* buffer = NULL;
				if (!baseClass::allocate_new(buffer,buf,cbSize,align))
					baseClass::free(buf);

				return buffer;
			}

			static Buffer* create(AllocatorInstance& allocator, size_t cbSize, size_t align)
			{
				void* buf = allocator.allocate(cbSize,align);
				if (!buf)
					return NULL;

				BufferImpl* buffer = NULL;
				if (!allocator.allocate_new(buffer,allocator,buf,cbSize,align))
					allocator.free(buf);

				return buffer;
			}

		private:
			void destroy()
			{
				baseClass::free(m_buffer);
				baseClass::delete_this(this);
			}

			uint8_t* reallocate(uint8_t* data, size_t size, size_t align)
			{
				return static_cast<uint8_t*>(baseClass::reallocate(data,size,align));
			}
		};
	}
}

template <typename Allocator>
inline OOBase::Buffer* OOBase::Buffer::create(size_t cbSize, size_t align)
{
	return detail::BufferImpl<Allocator>::create(cbSize,align);
}

inline OOBase::Buffer* OOBase::Buffer::create(size_t cbSize, size_t align)
{
	return detail::BufferImpl<CrtAllocator>::create(cbSize,align);
}

#endif // OOBASE_BUFFER_H_INCLUDED_
