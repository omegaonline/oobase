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

#include "../include/OOBase/version.h"

const char* OOBase_GetVersion()
{
#if defined(_DEBUG)
	return OOBASE_VERSION " (Debug build)";
#else
	return OOBASE_VERSION;
#endif
}

unsigned int OOBase_GetMajorVersion()
{
	return OOBASE_MAJOR_VERSION;
}

unsigned int OOBase_GetMinorVersion()
{
	return OOBASE_MINOR_VERSION;
}

unsigned int OOBase_GetPatchVersion()
{
	return OOBASE_PATCH_VERSION;
}
