#include "owner_task.hpp"
#include "control_pipe.hpp"
#include <sddl.h>
namespace ow::win {
void validate_owner_task_security(const std::wstring& s){PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(s.c_str(),SDDL_REVISION_1,&sd,nullptr),"Read owner task ACL");
 try{BOOL present{},defaulted{};PACL acl{};if(!GetSecurityDescriptorDacl(sd,&present,&acl,&defaulted)||!present||!acl)throw ControlFailure(4,"Owner task has no restrictive DACL");bool have_owner=false,have_system=false;
  for(DWORD n=0;n<acl->AceCount;n++){void* ptr{};require(GetAce(acl,n,&ptr),"Read task ACE");auto* header=static_cast<ACE_HEADER*>(ptr);if(header->AceType!=ACCESS_ALLOWED_ACE_TYPE)throw ControlFailure(4,"Unexpected task ACE; local review required");auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(ptr);LPWSTR sid{};require(ConvertSidToStringSidW(&ace->SidStart,&sid),"Read task ACE owner");std::wstring value(sid);LocalFree(sid);if((ace->Mask&GENERIC_ALL)==0&&(ace->Mask&FILE_ALL_ACCESS)!=FILE_ALL_ACCESS)throw ControlFailure(4,"Owner task access is incomplete");if(value==wide(process_owner_sid(GetCurrentProcess())))have_owner=true;else if(value==L"S-1-5-18")have_system=true;else throw ControlFailure(4,"Task grants another account access");}
  if(!have_owner||!have_system)throw ControlFailure(4,"Task owner/System permissions missing");LocalFree(sd);
 }catch(...){LocalFree(sd);throw;}
}
}
