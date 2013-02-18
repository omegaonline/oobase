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
		const char* rd_ptr() const;

		/// Advance the read pointer by \p cbSkip bytes.
		void rd_ptr(size_t cbSkip);

		/// Advance the read pointer to \p align byte boundary.
		void align_rd_ptr(size_t align);

		/// Get the current write pointer value.
		char* wr_ptr();

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
		virtual char* reallocate(char* data, size_t size, size_t align) = 0;

		Buffer(char* buffer, size_t cbSize, size_t align);

		char*   m_buffer;   ///< The actual underlying buffer.

	private:
		size_t  m_capacity; ///< The total allocated bytes for \p m_buffer.
		size_t  m_align;    ///< The alignment of the start of \p m_buffer.
		char*   m_wr_ptr;   ///< The current write pointer.
		char*   m_rd_ptr;   ///< The current read pointer.
	};

	namespace detail
	{
		template <typename Allocator>
		class BufferImpl : public Buffer, public Allocating<Allocator>
		{
			friend class Buffer;
			typedef Allocating<Allocator> baseClass;

		private:
			BufferImpl(void* buffer, size_t cbSize, size_t align) :
					Buffer(static_cast<char*>(buffer),cbSize,align),
					baseClass()
			{}

			BufferImpl(AllocatorInstance& allocator, void* buffer, size_t cbSize, size_t align) :
					Buffer(static_cast<char*>(buffer),cbSize,align),
					baseClass(allocator)
			{}

			void destroy()
			{
				baseClass::free(m_buffer);
				baseClass::delete_this(this);
			}

			char* reallocate(char* data, size_t size, size_t align)
			{
				return static_cast<char*>(baseClass::reallocate(data,size,align));
			}

			static Buffer* create(size_t cbSize, size_t align)
			{
				Buffer* pBuffer = NULL;
				void* p = baseClass::allocate(sizeof(BufferImpl),alignof<BufferImpl>::value);
				if (p)
				{
					void* buf = baseClass::allocate(cbSize,align);
					if (!buf)
					{
						baseClass::free(p);
						return NULL;
					}

#if defined(OOBASE_HAVE_EXCEPTIONS)
					try
					{
#endif
						pBuffer = ::new (p) BufferImpl(buf,cbSize,align);
#if defined(OOBASE_HAVE_EXCEPTIONS)
					}
					catch (...)
					{
						baseClass::free(buf);
						baseClass::free(p);
						throw;
					}
#endif
				}
				return pBuffer;
			}

			static Buffer* create(AllocatorInstance& allocator, size_t cbSize, size_t align)
			{
				Buffer* pBuffer = NULL;
				void* p = allocator.allocate(sizeof(BufferImpl),alignof<BufferImpl>::value);
				if (p)
				{
					void* buf = allocator.allocate(cbSize,align);
					if (!buf)
					{
						allocator.free(p);
						return NULL;
					}

#if defined(OOBASE_HAVE_EXCEPTIONS)
					try
					{
#endif
						pBuffer = ::new (p) BufferImpl(allocator,buf,cbSize,align);
#if defined(OOBASE_HAVE_EXCEPTIONS)
					}
					catch (...)
					{
						allocator.free(buf);
						allocator.free(p);
						throw;
					}
#endif
				}
				return pBuffer;
			}
		};
	}
}

template <typename Allocator>
inline OOBase::Buffer* OOBase::Buffer::create(size_t cbSize, size_t align)
{
	return OOBase::detail::BufferImpl<Allocator>::create(cbSize,align);
}

inline OOBase::Buffer* OOBase::Buffer::create(size_t cbSize, size_t align)
{
	return OOBase::detail::BufferImpl<CrtAllocator>::create(cbSize,align);
}

#endif // OOBASE_BUFFER_H_INCLUDED_
