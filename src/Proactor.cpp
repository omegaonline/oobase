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

#include "../include/OOBase/Condition.h"
#include "../include/OOBase/Proactor.h"

namespace
{
	struct WaitCallback : public OOBase::Future<int>
	{
		WaitCallback() : OOBase::Future<int>(0)
		{}

		static void callback(void* param, const OOBase::RefPtr<OOBase::Buffer>&, int err)
		{
			static_cast<WaitCallback*>(param)->signal(err);
		}

		static void callback_msg(void* param, const OOBase::RefPtr<OOBase::Buffer>&, const OOBase::RefPtr<OOBase::Buffer>&, int err)
		{
			static_cast<WaitCallback*>(param)->signal(err);
		}
	};
}

int OOBase::AsyncSocket::recv(const RefPtr<Buffer>& buffer, size_t bytes)
{
	WaitCallback wait;
	int err = recv(&wait,&WaitCallback::callback,buffer,bytes);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::recv_msg(const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, size_t data_len)
{
	WaitCallback wait;
	int err = recv_msg(&wait,&WaitCallback::callback_msg,data_buffer,ctl_buffer,data_len);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::send(const RefPtr<Buffer>& buffer)
{
	WaitCallback wait;
	int err = send(&wait,&WaitCallback::callback,buffer);
	if (err == 0)
		err = wait.wait(false);

	return err;
}

int OOBase::AsyncSocket::send_msg(const RefPtr<Buffer>& buffer, const RefPtr<Buffer>& ctl_buffer)
{
	WaitCallback wait;
	int err = send_msg(&wait,&WaitCallback::callback_msg,buffer,ctl_buffer);
	if (err == 0)
		err = wait.wait(false);

	return err;
}
