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

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Condition.h"

#include "../include/OOBase/Proactor.h"

namespace
{
	struct WaitCallback : public OOBase::Future<int>
	{
		WaitCallback() : OOBase::Future<int>(0)
		{}

		static void callback(void* param, OOBase::Buffer*, int err)
		{
			static_cast<WaitCallback*>(param)->signal(err);
		}

		static void callback_msg(void* param, OOBase::Buffer*, OOBase::Buffer*, int err)
		{
			static_cast<WaitCallback*>(param)->signal(err);
		}
	};
}

int OOBase::AsyncSocket::recv(Buffer* buffer, size_t bytes)
{
	WaitCallback wait;
	int err = recv(&wait,&WaitCallback::callback,buffer,bytes);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::recv_msg(Buffer* data_buffer, Buffer* ctl_buffer, size_t data_len)
{
	WaitCallback wait;
	int err = recv_msg(&wait,&WaitCallback::callback_msg,data_buffer,ctl_buffer,data_len);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::send(Buffer* buffer)
{
	WaitCallback wait;
	int err = send(&wait,&WaitCallback::callback,buffer);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::send_msg(Buffer* buffer, Buffer* ctl_buffer)
{
	WaitCallback wait;
	int err = send_msg(&wait,&WaitCallback::callback_msg,buffer,ctl_buffer);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

OOBase::Acceptor* OOBase::Proactor::accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa)
{
	return accept(param,callback,path,err,psa,get_internal_allocator());
}

OOBase::Acceptor* OOBase::Proactor::accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err)
{
	return accept(param,callback,addr,addr_len,err,get_internal_allocator());
}

OOBase::AsyncSocket* OOBase::Proactor::attach(socket_t sock, int& err)
{
	return attach(sock,err,get_internal_allocator());
}

#if defined(_WIN32)
OOBase::AsyncSocket* OOBase::Proactor::attach(HANDLE hPipe, int& err)
{
	return attach(hPipe,err,get_internal_allocator());
}
#endif

OOBase::AsyncSocket* OOBase::Proactor::connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout)
{
	return connect(addr,addr_len,err,timeout,get_internal_allocator());
}

OOBase::AsyncSocket* OOBase::Proactor::connect(const char* path, int& err, const Timeout& timeout)
{
	return connect(path,err,timeout,get_internal_allocator());
}
