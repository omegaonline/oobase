///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
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

#ifndef OOBASE_CDR_IO_H_INCLUDED_
#define OOBASE_CDR_IO_H_INCLUDED_

#include "Proactor.h"
#include "CDRStream.h"

namespace OOBase
{
	// This is a class purely to allow friends
	class CDRIO
	{
	public:
		template <typename T, typename H>
		static int recv_with_header_sync(H expected_len, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, int err))
		{
			CDRStream stream(expected_len);
			if (stream.last_error())
				return stream.last_error();

			ThunkRHS<T,H>* thunk = pSocket->thunk_allocate<ThunkRHS<T,H>,T*,void (T::*)(CDRStream&,int)>(param,callback);
			if (!thunk)
				return ERROR_OUTOFMEMORY;

			thunk->m_pSocket = pSocket;
			thunk->m_pSocket->addref();
			return pSocket->recv(thunk,&ThunkRHS<T,H>::fn1,stream.buffer(),sizeof(H));
		}

	private:
		template <typename T, typename H>
		struct ThunkRHS
		{
			ThunkRHS(T* param, void (T::*callback)(CDRStream&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator),m_pSocket(NULL)
			{}

			T* m_param;
			void (T::*m_callback)(CDRStream&,int);
			AllocatorInstance& m_allocator;
			AsyncSocket* m_pSocket;

			static void fn1(void* param, Buffer* buffer, int err)
			{
				CDRStream stream(buffer);
				ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else
						err = thunk.m_pSocket->recv(param,&fn2,buffer,msg_len - sizeof(H));
				}
				if (err)
				{
					thunk.m_pSocket->release();
					static_cast<ThunkRHS*>(param)->~ThunkRHS();
					thunk.m_allocator.free(param);
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn2(void* param, Buffer* buffer, int err)
			{
				ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
				thunk.m_pSocket->release();
				static_cast<ThunkRHS*>(param)->~ThunkRHS();
				thunk.m_allocator.free(param);
				CDRStream stream(buffer);
				(thunk.m_param->*thunk.m_callback)(stream,err);
			}
		};
	};
}

#endif // OOBASE_CDR_IO_H_INCLUDED_
