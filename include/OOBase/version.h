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

#ifndef OOBASE_VERSION_H_INCLUDED_
#define OOBASE_VERSION_H_INCLUDED_

#define OOBASE_MAJOR_VERSION  0
#define OOBASE_MINOR_VERSION  5
#define OOBASE_PATCH_VERSION  0

#define OOBASE_VERSION_III(n)        #n
#define OOBASE_VERSION_II(a,b,c)     OOBASE_VERSION_III(a.b.c)
#define OOBASE_VERSION_I(a,b,c)      OOBASE_VERSION_II(a,b,c)
#define OOBASE_VERSION               OOBASE_VERSION_I(OOBASE_MAJOR_VERSION,OOBASE_MINOR_VERSION,OOBASE_PATCH_VERSION)

#if defined(__cplusplus)
extern "C"
{
#endif // __cplusplus

unsigned int OOBase_GetMajorVersion();
unsigned int OOBase_GetMinorVersion();
unsigned int OOBase_GetPatchVersion();
const char* OOBase_GetVersion();

#if defined(__cplusplus)
} // extern "C"
#endif // __cplusplus

#endif // OOBASE_VERSION_H_INCLUDED_
