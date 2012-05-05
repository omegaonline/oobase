///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
//
// This file is part of OOSvrBase, the Omega Online Base library.
//
// OOSvrBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOSvrBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOSvrBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#include "../include/OOBase/GlobalNew.h"
#include "../include/OOBase/Condition.h"

#include "../include/OOSvrBase/Proactor.h"

namespace
{
	struct WaitCallback : public OOBase::Future<int>
	{
		WaitCallback() : OOBase::Future<int>(0)
		{}

		static void callback_recv(void* param, OOBase::Buffer*, int err)
		{
			callback(param,err);
		}

		static void callback(void* param, int err)
		{
			static_cast<WaitCallback*>(param)->signal(err);
		}
	};
}

int OOSvrBase::AsyncSocket::recv(OOBase::Buffer* buffer, size_t bytes)
{
	WaitCallback wait;
	int err = recv(&wait,&WaitCallback::callback_recv,buffer,bytes);
	if (err == 0)
		err = wait.wait();

	return err;
}

int OOSvrBase::AsyncSocket::send(OOBase::Buffer* buffer)
{
	WaitCallback wait;
	int err = send(&wait,&WaitCallback::callback,buffer);
	if (err == 0)
		err = wait.wait();

	return err;
}
