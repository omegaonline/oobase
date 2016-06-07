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

#include "../include/OOBase/Win32Security.h"
#include "../include/OOBase/StackAllocator.h"

#if defined(_WIN32)

#include <shlwapi.h>

namespace
{
	class NetApiDestructor
	{
	public:
		static void free(void* ptr)
		{
			NetApiBufferFree(ptr);
		}
	};
}

OOBase::Win32::sec_descript_t::sec_descript_t(PSECURITY_DESCRIPTOR pSD)
{
	if (pSD)
		copy(pSD);
}

OOBase::Win32::sec_descript_t::sec_descript_t(const sec_descript_t& rhs)
{
	copy(rhs.m_psd.get());
}

void OOBase::Win32::sec_descript_t::copy(PSECURITY_DESCRIPTOR other)
{
	if (!other)
		return;

	if (!IsValidSecurityDescriptor(other))
		OOBase_CallCriticalFailure("Invalid SECURITY_DESCRIPTOR");

	DWORD dwLen = GetSecurityDescriptorLength(other);

	SharedPtr<SECURITY_DESCRIPTOR> ours = make_shared<SECURITY_DESCRIPTOR,LocalAllocator>((SECURITY_DESCRIPTOR*)LocalAlloc(LPTR,dwLen));
	if (!ours)
		OOBase_CallCriticalFailure(GetLastError());

	SECURITY_DESCRIPTOR_CONTROL control = 0;
	DWORD dwRevision = 0;
	if (!GetSecurityDescriptorControl(other,&control,&dwRevision))
		OOBase_CallCriticalFailure(GetLastError());

	if (control & SE_SELF_RELATIVE)
	{
		memcpy(ours.get(),other,dwLen);
		m_psd = ours;
	}
	else
	{
		if (MakeSelfRelativeSD(other,ours.get(),&dwLen))
			m_psd = ours;
		else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			ours = make_shared<SECURITY_DESCRIPTOR,LocalAllocator>((SECURITY_DESCRIPTOR*)LocalAlloc(LPTR,dwLen));
			if (!ours)
				OOBase_CallCriticalFailure(GetLastError());

			if (!MakeSelfRelativeSD(other,ours.get(),&dwLen))
				OOBase_CallCriticalFailure(GetLastError());

			m_psd = ours;
		}
		else
			OOBase_CallCriticalFailure(GetLastError());
	}
}

DWORD OOBase::Win32::sec_descript_t::SetEntriesInAcl(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESSW pListOfExplicitEntries)
{
	if (!m_psd)
	{
		// Create a new security descriptor
		m_psd = make_shared<SECURITY_DESCRIPTOR,LocalAllocator>((SECURITY_DESCRIPTOR*)LocalAlloc(LPTR,SECURITY_DESCRIPTOR_MIN_LENGTH));
		if (!m_psd)
			OOBase_CallCriticalFailure(GetLastError());

		// Initialize a security descriptor.
		if (!InitializeSecurityDescriptor(m_psd.get(),SECURITY_DESCRIPTOR_REVISION))
			OOBase_CallCriticalFailure(GetLastError());
	}

	PACL pACL = NULL;
	DWORD dwErr = ::SetEntriesInAclW(cCountOfExplicitEntries,pListOfExplicitEntries,m_pACL.get(),&pACL);
	if (dwErr)
		return dwErr;

	if (pACL != m_pACL.get())
		m_pACL = make_shared<ACL,LocalAllocator>(pACL);

	// Add the ACL to the SD
	if (!SetSecurityDescriptorDacl(m_psd.get(),TRUE,m_pACL.get(),FALSE))
		return GetLastError();

	return ERROR_SUCCESS;
}

DWORD OOBase::Win32::GetNameFromToken(HANDLE hToken, ScopedArrayPtr<wchar_t>& strUserName, ScopedArrayPtr<wchar_t>& strDomainName)
{
	// Find out all about the user associated with hToken
	UniquePtr<TOKEN_USER,ThreadLocalAllocator> ptrUserInfo;
	DWORD dwErr = GetTokenInfo(hToken,TokenUser,ptrUserInfo);
	if (dwErr)
		return dwErr;

	SID_NAME_USE name_use;
	DWORD dwUNameSize = static_cast<DWORD>(strUserName.count());
	DWORD dwDNameSize = static_cast<DWORD>(strDomainName.count());
	LookupAccountSidW(NULL,ptrUserInfo->User.Sid,strUserName.get(),&dwUNameSize,strDomainName.get(),&dwDNameSize,&name_use);
	if (dwUNameSize == 0)
		return GetLastError();

	if (!strUserName.resize(dwUNameSize))
		return system_error();

	if (dwDNameSize)
	{
		if (!strDomainName.resize(dwDNameSize))
			return system_error();
	}

	if (!LookupAccountSidW(NULL,ptrUserInfo->User.Sid,strUserName.get(),&dwUNameSize,strDomainName.get(),&dwDNameSize,&name_use))
		return GetLastError();

	return ERROR_SUCCESS;
}

DWORD OOBase::Win32::LoadUserProfileFromToken(HANDLE hToken, HANDLE& hProfile)
{
	// Get the names associated with the user SID
	ScopedArrayPtr<wchar_t> strUserName,strDomainName;
	DWORD dwErr = GetNameFromToken(hToken,strUserName,strDomainName);
	if (dwErr)
		return dwErr;

	// Lookup a DC for pszDomain
	LPBYTE v = NULL;
	NetGetAnyDCName(NULL,strDomainName.get(),&v);
	UniquePtr<wchar_t,NetApiDestructor> ptrDCName(reinterpret_cast<wchar_t*>(v));

	// Try to find the user's profile path...
	v = NULL;
	NetUserGetInfo(ptrDCName.get(),strUserName.get(),3,&v);
	UniquePtr<USER_INFO_3,NetApiDestructor> pInfo(reinterpret_cast<USER_INFO_3*>(v));

	// Load the Users Profile
	PROFILEINFOW profile_info = {0};
	profile_info.dwSize = sizeof(PROFILEINFOW);
	profile_info.dwFlags = PI_NOUI;
	profile_info.lpUserName = strUserName.get();

	if (pInfo->usri3_profile != NULL)
		profile_info.lpProfilePath = pInfo->usri3_profile;

	if (ptrDCName != NULL)
		profile_info.lpServerName = ptrDCName.get();

	if (!LoadUserProfileW(hToken,&profile_info))
		return GetLastError();

	hProfile = profile_info.hProfile;
	return ERROR_SUCCESS;
}

DWORD OOBase::Win32::GetLogonSID(HANDLE hToken, UniquePtr<SID,AllocatorInstance>& pSIDLogon)
{
	// Get the logon SID of the Token
	UniquePtr<TOKEN_GROUPS,AllocatorInstance> ptrGroups(pSIDLogon.get_allocator());
	DWORD dwErr = GetTokenInfo(hToken,TokenGroups,ptrGroups);
	if (dwErr)
		return dwErr;

	// Loop through the groups to find the logon SID
	for (DWORD dwIndex = 0; ptrGroups->GroupCount != 0 && dwIndex < ptrGroups->GroupCount; ++dwIndex)
	{
		if ((ptrGroups->Groups[dwIndex].Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID)
		{
			// Found the logon SID...
			if (IsValidSid(ptrGroups->Groups[dwIndex].Sid))
			{
				DWORD dwLen = GetLengthSid(ptrGroups->Groups[dwIndex].Sid);

				pSIDLogon.reset(static_cast<SID*>(pSIDLogon.get_allocator().allocate(dwLen,16)));
				if (!pSIDLogon)
					return system_error();

				if (!CopySid(dwLen,pSIDLogon.get(),ptrGroups->Groups[dwIndex].Sid))
					return GetLastError();

				return ERROR_SUCCESS;
			}
		}
	}

	return ERROR_INVALID_SID;
}

DWORD OOBase::Win32::SetTokenDefaultDACL(HANDLE hToken)
{
	// Get the current Default DACL
	StackAllocator<256> allocator;
	UniquePtr<TOKEN_DEFAULT_DACL,AllocatorInstance> ptrDef_dacl(allocator);
	DWORD dwErr = GetTokenInfo(hToken,TokenDefaultDacl,ptrDef_dacl);
	if (dwErr)
		return dwErr;

	// Get the logon SID of the Token
	UniquePtr<SID,AllocatorInstance> ptrSIDLogon(allocator);
	dwErr = GetLogonSID(hToken,ptrSIDLogon);
	if (dwErr)
		return dwErr;

	EXPLICIT_ACCESSW ea = {0};

	// Set maximum access for the logon SID
	ea.grfAccessPermissions = GENERIC_ALL;
	ea.grfAccessMode = GRANT_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea.Trustee.ptstrName = (LPWSTR)ptrSIDLogon.get();

	TOKEN_DEFAULT_DACL def_dacl = {0};
	dwErr = SetEntriesInAclW(1,&ea,ptrDef_dacl->DefaultDacl,&def_dacl.DefaultDacl);
	if (dwErr)
		return dwErr;

	// Now set the token default DACL
	if (!SetTokenInformation(hToken,TokenDefaultDacl,&def_dacl,sizeof(def_dacl)))
		dwErr = GetLastError();

	::LocalFree(def_dacl.DefaultDacl);

	return dwErr;
}

DWORD OOBase::Win32::EnableUserAccessToDir(const wchar_t* pszPath, const TOKEN_USER* pUser)
{
	wchar_t szPath[MAX_PATH] = {0};
	PathCanonicalizeW(szPath,pszPath);
	PathRemoveFileSpecW(szPath);

	PACL pACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	DWORD dwRes = GetNamedSecurityInfoW(szPath,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&pACL,NULL,&pSD);
	if (dwRes != ERROR_SUCCESS)
		return dwRes;

	UniquePtr<void,Win32::LocalAllocator> ptrSD(pSD);

	EXPLICIT_ACCESSW ea = {0};

	// Set maximum access for the logon SID
	ea.grfAccessPermissions = GENERIC_ALL;
	ea.grfAccessMode = GRANT_ACCESS;
	ea.grfInheritance = OBJECT_INHERIT_ACE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea.Trustee.ptstrName = (LPWSTR)pUser->User.Sid;

	PACL pACLNew;
	dwRes = SetEntriesInAclW(1,&ea,pACL,&pACLNew);
	if (dwRes != ERROR_SUCCESS)
		return dwRes;

	UniquePtr<ACL,Win32::LocalAllocator> ptrACLNew(pACLNew);

	return SetNamedSecurityInfoW(szPath,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,pACLNew,NULL);
}

DWORD OOBase::Win32::RestrictToken(HANDLE hToken, HANDLE* hNewToken)
{
	HMODULE hKernel32 = GetModuleHandleW(L"Kernel32.dll");
	if (hKernel32 != NULL)
	{
		if (GetProcAddress(hKernel32,"CreateRestrictedToken"))
		{
			// Create a restricted token...
			if (!CreateRestrictedToken(hToken,DISABLE_MAX_PRIVILEGE /*| SANDBOX_INERT*/,0,NULL,0,NULL,0,NULL,hNewToken))
				return GetLastError();
		}
		else
		{
			void* TODO;
			// Duplicate Token
			// Adjust Token to remove all privilege apart from SeChangeNotifyPrivilege
			// Add membership of the least privilege SID

#if defined(SAFER_SCOPEID_MACHINE)
			// Apply SAFER rules if possible
			if (GetProcAddress(hKernel32,"SaferCreateLevel"))
			{
				// Use SAFER API
				SAFER_LEVEL_HANDLE hAuthzLevel = NULL;
				if (!SaferCreateLevel(SAFER_SCOPEID_MACHINE,SAFER_LEVELID_UNTRUSTED,SAFER_LEVEL_OPEN,&hAuthzLevel,NULL))
					return GetLastError();

				//// Generate the restricted token we will use.
				DWORD dwRes = ERROR_SUCCESS;
				if (!SaferComputeTokenFromLevel(
					hAuthzLevel,    // SAFER Level handle
					hToken,         // Source token
					hNewToken,     // Target token
					0,              // No flags
					NULL))          // Reserved
				{
					dwRes = GetLastError();
				}

				SaferCloseLevel(hAuthzLevel);

				return dwRes;
			}
#endif
		}
	}

	return ERROR_INVALID_FUNCTION;
}

bool OOBase::Win32::MatchSids(ULONG count, PSID_AND_ATTRIBUTES pSids1, PSID_AND_ATTRIBUTES pSids2)
{
	for (ULONG i=0; i<count; ++i)
	{
		bool bFound = false;
		for (ULONG j=0; j<count; ++j)
		{
			if (EqualSid(pSids1[i].Sid,pSids2[j].Sid) &&
					pSids1[i].Attributes == pSids2[j].Attributes)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
			return false;
	}

	return true;
}

bool OOBase::Win32::MatchPrivileges(ULONG count, PLUID_AND_ATTRIBUTES Privs1, PLUID_AND_ATTRIBUTES Privs2)
{
	for (ULONG i=0; i<count; ++i)
	{
		bool bFound = false;
		for (ULONG j=0; j<count; ++j)
		{
			if (Privs1[i].Luid.LowPart == Privs2[j].Luid.LowPart &&
					Privs1[i].Luid.HighPart == Privs2[j].Luid.HighPart &&
					Privs1[i].Attributes == Privs2[j].Attributes)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
			return false;
	}

	return true;
}

DWORD OOBase::Win32::StringToSID(const char* pszSID, UniquePtr<SID,AllocatorInstance>& pSID)
{
	PSID pSID2 = NULL;

	// Duff declaration in mingw 3.4.5
	if (!ConvertStringSidToSidA((CHAR*)pszSID,&pSID2))
		return GetLastError();

	pSID.reset((SID*)pSID2);
	return 0;
}

#endif // _WIN32
