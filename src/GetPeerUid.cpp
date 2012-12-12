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
//
// Portions of this file are derived from the PostgreSQL sources:
//
// Portions Copyright (c) 1996-2010, The PostgreSQL Global Development Group
//
// Portions Copyright (c) 1994, The Regents of the University of California
//
// Permissions to use, copy, modify, and distribute this software and its documentation for any purpose,
// without fee, and without a written agreement is hereby granted, provided that the above copyright
// notice and this paragraph and the following two paragraphs appear in all copies.
//
// IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR DIRECT,
// INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
// ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY
// OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
// CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
// ENHANCEMENTS, OR MODIFICATIONS.
//
///////////////////////////////////////////////////////////////////////////////////

#include "./BSDSocket.h"

#if defined(HAVE_UNISTD_H)

int OOBase::POSIX::get_peer_uid(int fd, uid_t& uid)
{
#if defined(HAVE_GETPEEREID)
	/* OpenBSD style:  */
	gid_t gid;
	if (getpeereid(fd, &uid, &gid) != 0)
	{
		/* We didn't get a valid credentials struct. */
		return errno;
	}
	return 0;

#elif defined(HAVE_SO_PEERCRED)
	/* Linux style: use getsockopt(SO_PEERCRED) */
	ucred peercred;
	socklen_t so_len = sizeof(peercred);

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peercred, &so_len) != 0 || so_len != sizeof(peercred))
	{
		/* We didn't get a valid credentials struct. */
		return errno;
	}
	uid = peercred.uid;
	return 0;

#elif defined(HAVE_GETPEERUCRED)
	/* Solaris > 10 */
	ucred_t* ucred = NULL; /* must be initialized to NULL */
	if (getpeerucred(fd, &ucred) != 0)
	{
		/* We didn't get a valid credentials struct. */
		return errno;
	}

	if ((uid = ucred_geteuid(ucred)) == -1)
	{
		int err = errno;
		ucred_free(ucred);
		return err;
	}
	return 0;

#elif (defined(HAVE_STRUCT_CMSGCRED) || defined(HAVE_STRUCT_FCRED) || defined(HAVE_STRUCT_SOCKCRED)) && defined(HAVE_LOCAL_CREDS)

	/*
	* Receive credentials on next message receipt, BSD/OS,
	* NetBSD. We need to set this before the client sends the
	* next packet.
	*/
	int on = 1;
	if (setsockopt(fd, 0, LOCAL_CREDS, &on, sizeof(on)) != 0)
		return errno;

	/* Credentials structure */
#if defined(HAVE_STRUCT_CMSGCRED)
	typedef cmsgcred Cred;
	#define cruid cmcred_uid
#elif defined(HAVE_STRUCT_FCRED)
	typedef fcred Cred;
	#define cruid fc_uid
#elif defined(HAVE_STRUCT_SOCKCRED)
	typedef sockcred Cred;
	#define cruid sc_uid
#endif
	/* Compute size without padding */
	char cmsgmem[ALIGN(sizeof(struct cmsghdr)) + ALIGN(sizeof(Cred))];   /* for NetBSD */

	/* Point to start of first structure */
	struct cmsghdr* cmsg = (struct cmsghdr*)cmsgmem;
	struct iovec iov;

	msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = (char *) cmsg;
	msg.msg_controllen = sizeof(cmsgmem);
	memset(cmsg, 0, sizeof(cmsgmem));

	/*
	 * The one character which is received here is not meaningful; its
	 * purposes is only to make sure that recvmsg() blocks long enough for the
	 * other side to send its credentials.
	 */
	char buf;
	iov.iov_base = &buf;
	iov.iov_len = 1;

	if (recvmsg(fd, &msg, 0) < 0 || cmsg->cmsg_len < sizeof(cmsgmem) || cmsg->cmsg_type != SCM_CREDS)
		return errno;

	Cred* cred = (Cred*)CMSG_DATA(cmsg);
	uid = cred->cruid;
	return 0;

#else
	// We can't handle this situation
	#error Fix me!
#endif
}

#endif
