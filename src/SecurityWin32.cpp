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

#include "../include/OOSvrBase/SecurityWin32.h"

#if defined(_WIN32)

#include "../include/OOBase/Memory.h"

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

OOSvrBase::Win32::sec_descript_t::sec_descript_t()
{
	// Create a new security descriptor
	m_psd = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (!m_psd)
		OOBase_CallCriticalFailure(GetLastError());

	// Initialize a security descriptor.
	if (!InitializeSecurityDescriptor((PSECURITY_DESCRIPTOR)m_psd,SECURITY_DESCRIPTOR_REVISION))
		OOBase_CallCriticalFailure(GetLastError());
}

OOSvrBase::Win32::sec_descript_t::~sec_descript_t()
{
}

DWORD OOSvrBase::Win32::sec_descript_t::SetEntriesInAcl(ULONG cCountOfExplicitEntries, PEXPLICIT_ACCESSW pListOfExplicitEntries, PACL OldAcl)
{
	if (m_pACL)
		m_pACL = NULL;

	PACL pACL;
	DWORD dwErr = ::SetEntriesInAclW(cCountOfExplicitEntries,pListOfExplicitEntries,OldAcl,&pACL);
	if (ERROR_SUCCESS != dwErr)
		return dwErr;

	m_pACL = pACL;

	// Add the ACL to the SD
	if (!SetSecurityDescriptorDacl((PSECURITY_DESCRIPTOR)m_psd,TRUE,m_pACL,FALSE))
		return GetLastError();

	return ERROR_SUCCESS;
}

DWORD OOSvrBase::Win32::GetNameFromToken(HANDLE hToken, OOBase::SmartPtr<wchar_t,OOBase::LocalAllocator>& strUserName, OOBase::SmartPtr<wchar_t,OOBase::LocalAllocator>& strDomainName)
{
	// Find out all about the user associated with hToken
	OOBase::SmartPtr<TOKEN_USER,OOBase::HeapAllocator> ptrUserInfo = static_cast<TOKEN_USER*>(GetTokenInfo(hToken,TokenUser));
	if (!ptrUserInfo)
		return GetLastError();

	SID_NAME_USE name_use;
	DWORD dwUNameSize = 0;
	DWORD dwDNameSize = 0;
	LookupAccountSidW(NULL,ptrUserInfo->User.Sid,NULL,&dwUNameSize,NULL,&dwDNameSize,&name_use);
	if (dwUNameSize == 0)
		return GetLastError();

	strUserName = static_cast<wchar_t*>(OOBase::LocalAllocate(dwUNameSize*sizeof(wchar_t)));
	if (!strUserName)
		return ERROR_OUTOFMEMORY;

	if (dwDNameSize)
	{
		strDomainName = static_cast<wchar_t*>(OOBase::LocalAllocate(dwDNameSize*sizeof(wchar_t)));
		if (!strDomainName)
			return ERROR_OUTOFMEMORY;
	}

	if (!LookupAccountSidW(NULL,ptrUserInfo->User.Sid,strUserName,&dwUNameSize,strDomainName,&dwDNameSize,&name_use))
		return GetLastError();

	return ERROR_SUCCESS;
}

DWORD OOSvrBase::Win32::LoadUserProfileFromToken(HANDLE hToken, HANDLE& hProfile)
{
	// Get the names associated with the user SID
	OOBase::SmartPtr<wchar_t,OOBase::LocalAllocator> strUserName;
	OOBase::SmartPtr<wchar_t,OOBase::LocalAllocator> strDomainName;

	DWORD err = GetNameFromToken(hToken,strUserName,strDomainName);
	if (err != ERROR_SUCCESS)
		return err;

	// Lookup a DC for pszDomain
	LPBYTE v = NULL;
	NetGetAnyDCName(NULL,strDomainName,&v);
	OOBase::SmartPtr<wchar_t,NetApiDestructor> ptrDCName = reinterpret_cast<wchar_t*>(v);

	// Try to find the user's profile path...
	v = NULL;
	NetUserGetInfo(ptrDCName,strUserName,3,&v);
	OOBase::SmartPtr<USER_INFO_3,NetApiDestructor> pInfo = reinterpret_cast<USER_INFO_3*>(v);

	// Load the Users Profile
	PROFILEINFOW profile_info = {0};
	profile_info.dwSize = sizeof(PROFILEINFOW);
	profile_info.dwFlags = PI_NOUI;
	profile_info.lpUserName = strUserName;

	if (pInfo->usri3_profile != NULL)
		profile_info.lpProfilePath = pInfo->usri3_profile;

	if (ptrDCName != NULL)
		profile_info.lpServerName = ptrDCName;

	if (!LoadUserProfileW(hToken,&profile_info))
		return GetLastError();

	hProfile = profile_info.hProfile;
	return ERROR_SUCCESS;
}

DWORD OOSvrBase::Win32::GetLogonSID(HANDLE hToken, OOBase::SmartPtr<void,OOBase::LocalAllocator>& pSIDLogon)
{
	// Get the logon SID of the Token
	OOBase::SmartPtr<TOKEN_GROUPS,OOBase::HeapAllocator> ptrGroups = static_cast<TOKEN_GROUPS*>(GetTokenInfo(hToken,TokenGroups));
	if (!ptrGroups)
		return GetLastError();

	// Loop through the groups to find the logon SID
	for (DWORD dwIndex = 0; dwIndex < ptrGroups->GroupCount; ++dwIndex)
	{
		if ((ptrGroups->Groups[dwIndex].Attributes & SE_GROUP_LOGON_ID) == SE_GROUP_LOGON_ID)
		{
			// Found the logon SID...
			if (IsValidSid(ptrGroups->Groups[dwIndex].Sid))
			{
				DWORD dwLen = GetLengthSid(ptrGroups->Groups[dwIndex].Sid);
				pSIDLogon = static_cast<PSID>(OOBase::LocalAllocate(dwLen));
				if (!pSIDLogon)
					return ERROR_OUTOFMEMORY;

				if (!CopySid(dwLen,pSIDLogon,ptrGroups->Groups[dwIndex].Sid))
					return GetLastError();

				return ERROR_SUCCESS;
			}
		}
	}

	return ERROR_INVALID_SID;
}

DWORD OOSvrBase::Win32::SetTokenDefaultDACL(HANDLE hToken)
{
	// Get the current Default DACL
	OOBase::SmartPtr<TOKEN_DEFAULT_DACL,OOBase::HeapAllocator> ptrDef_dacl = static_cast<TOKEN_DEFAULT_DACL*>(GetTokenInfo(hToken,TokenDefaultDacl));
	if (!ptrDef_dacl)
		return ERROR_OUTOFMEMORY;

	// Get the logon SID of the Token
	OOBase::SmartPtr<void,OOBase::LocalAllocator> ptrSIDLogon;
	DWORD dwRes = GetLogonSID(hToken,ptrSIDLogon);
	if (dwRes != ERROR_SUCCESS)
		return dwRes;

	EXPLICIT_ACCESSW ea = {0};

	// Set maximum access for the logon SID
	ea.grfAccessPermissions = GENERIC_ALL;
	ea.grfAccessMode = GRANT_ACCESS;
	ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
	ea.Trustee.ptstrName = (LPWSTR)ptrSIDLogon;

	TOKEN_DEFAULT_DACL def_dacl = {0};
	dwRes = SetEntriesInAclW(1,&ea,ptrDef_dacl->DefaultDacl,&def_dacl.DefaultDacl);
	if (dwRes != ERROR_SUCCESS)
		return dwRes;

	// Now set the token default DACL
	if (!SetTokenInformation(hToken,TokenDefaultDacl,&def_dacl,sizeof(def_dacl)))
		dwRes = GetLastError();

	::LocalFree(def_dacl.DefaultDacl);

	return dwRes;
}

DWORD OOSvrBase::Win32::EnableUserAccessToDir(const wchar_t* pszPath, const TOKEN_USER* pUser)
{
	wchar_t szPath[MAX_PATH] = {0};
	PathCanonicalizeW(szPath,pszPath);
	PathRemoveFileSpecW(szPath);

	PACL pACL = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	DWORD dwRes = GetNamedSecurityInfoW(szPath,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,NULL,NULL,&pACL,NULL,&pSD);
	if (dwRes != ERROR_SUCCESS)
		return dwRes;

	OOBase::SmartPtr<void,OOBase::Win32::LocalAllocDestructor> ptrSD = pSD;

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

	OOBase::SmartPtr<ACL,OOBase::Win32::LocalAllocDestructor> ptrACLNew = pACLNew;

	return SetNamedSecurityInfoW(szPath,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,pACLNew,NULL);
}

DWORD OOSvrBase::Win32::RestrictToken(HANDLE& hToken)
{
	// Work out what version of windows we are running on...
	OSVERSIONINFO os = {0};
	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionEx(&os);

#if !defined(_DEBUG)
	if ((os.dwMajorVersion == 5 && os.dwMinorVersion > 0) || os.dwMajorVersion >= 5)
	{
		//// Use SAFER API
		//SAFER_LEVEL_HANDLE hAuthzLevel = NULL;
		//if (!SaferCreateLevel(SAFER_SCOPEID_MACHINE,SAFER_LEVELID_UNTRUSTED,SAFER_LEVEL_OPEN,&hAuthzLevel,NULL))
		//  return false;

		//// Generate the restricted token we will use.
		//bool bOk = false;
		//HANDLE hNewToken = NULL;
		//if (SaferComputeTokenFromLevel(
		//  hAuthzLevel,    // SAFER Level handle
		//  hToken,         // Source token
		//  &hNewToken,     // Target token
		//  0,              // No flags
		//  NULL))          // Reserved
		//{
		//  // Swap the tokens
		//  CloseHandle(hToken);
		//  hToken = hNewToken;
		//  bOk = true;
		//}

		//SaferCloseLevel(hAuthzLevel);
		//
		//if (!bOk)
		//  return false;
	}
#endif

	if (os.dwMajorVersion >= 5)
	{
		// Create a restricted token...
		HANDLE hNewToken = NULL;
		if (!CreateRestrictedToken(hToken,DISABLE_MAX_PRIVILEGE | SANDBOX_INERT,0,NULL,0,NULL,0,NULL,&hNewToken))
			return GetLastError();

		CloseHandle(hToken);
		hToken = hNewToken;
	}

	if (os.dwMajorVersion > 5)
	{
		// Vista - use UAC as well...
	}

	return ERROR_SUCCESS;
}

void* OOSvrBase::Win32::GetTokenInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS cls)
{
	for (DWORD dwLen = 256;;)
	{
		void* pBuffer = OOBase::HeapAllocate(dwLen);
		if (!pBuffer)
		{
			SetLastError(ERROR_OUTOFMEMORY);
			return NULL;
		}

		if (GetTokenInformation(hToken,cls,pBuffer,dwLen,&dwLen))
			return pBuffer;

		DWORD err = GetLastError();

		OOBase::HeapFree(pBuffer);

		SetLastError(err);
			
		if (err != ERROR_INSUFFICIENT_BUFFER)
			return NULL;
	}
}

bool OOSvrBase::Win32::MatchSids(ULONG count, PSID_AND_ATTRIBUTES pSids1, PSID_AND_ATTRIBUTES pSids2)
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

bool OOSvrBase::Win32::MatchPrivileges(ULONG count, PLUID_AND_ATTRIBUTES Privs1, PLUID_AND_ATTRIBUTES Privs2)
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

#endif // _WIN32
