#include "owner_state.hpp"
#include "control_pipe.hpp"
#include "ow/control.hpp"
#include <aclapi.h>
#include <sddl.h>
namespace ow::win {
void require_normal_owner(){auto sid=process_owner_sid(GetCurrentProcess());if(elevated()||sid=="S-1-5-18"||sid=="S-1-5-19"||sid=="S-1-5-20")throw ControlFailure(4,"Normal non-elevated owner token required; service/admin engine refused");}
void verify_owner_state_path(const std::filesystem::path& path,bool private_identity){
 auto p=std::filesystem::absolute(path).lexically_normal();for(auto q=p;;){auto a=GetFileAttributesW(q.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))throw ControlFailure(4,"Missing or reparse owner state path");auto parent=q.parent_path();if(parent==q||parent.empty())break;q=std::move(parent);}
 PSID owner{};PACL acl{};PSECURITY_DESCRIPTOR sd{};auto error=GetNamedSecurityInfoW(p.c_str(),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&sd);if(error)throw ControlFailure(4,"Cannot inspect existing owner state permissions");
 auto text_sid=[](PSID sid){LPWSTR value{};require(ConvertSidToStringSidW(sid,&value),"Read state SID");std::string s=utf8(value);LocalFree(value);return s;};
 const auto expected=process_owner_sid(GetCurrentProcess());const auto trusted=[&](const std::string& s){return s==expected||s=="S-1-5-18"||s=="S-1-5-32-544";};
 try{
  if(!owner||!trusted(text_sid(owner))||!acl)throw ControlFailure(4,"State is not owned/protected by the current Windows owner");
  constexpr ACCESS_MASK writes=GENERIC_WRITE|GENERIC_ALL|FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES|FILE_DELETE_CHILD|DELETE|WRITE_DAC|WRITE_OWNER;
  for(DWORD n=0;n<acl->AceCount;n++){void* raw{};require(GetAce(acl,n,&raw),"Read state ACL");auto* h=static_cast<ACE_HEADER*>(raw);if((h->AceFlags&INHERIT_ONLY_ACE)||h->AceType==ACCESS_DENIED_ACE_TYPE)continue;if(h->AceType!=ACCESS_ALLOWED_ACE_TYPE)throw ControlFailure(4,"Unexpected owner state ACE; local review required");auto* a=static_cast<ACCESS_ALLOWED_ACE*>(raw);if(!trusted(text_sid(&a->SidStart))&&(private_identity||(a->Mask&writes)))throw ControlFailure(4,"Another account can access private identity or modify executable/state");}
  LocalFree(sd);
 }catch(...){LocalFree(sd);throw;}
}
}
