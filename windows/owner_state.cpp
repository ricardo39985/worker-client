#include "owner_state.hpp"
#include "control_pipe.hpp"
#include "ow/control.hpp"
#include "ow/protocol.hpp"
#include <aclapi.h>
#include <sddl.h>
namespace ow::win {
namespace {
std::string sid_text(PSID sid){LPWSTR value{};require(ConvertSidToStringSidW(sid,&value),"Read state SID");std::string s=utf8(value);LocalFree(value);return s;}
bool trusted(const std::string& sid,const std::string& user,bool ancestor){
 return sid==user||sid=="S-1-5-18"||sid=="S-1-5-32-544"||
  (ancestor&&sid=="S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464"); // Windows TrustedInstaller
}
void check_acl(const std::filesystem::path& p,const std::string& user,bool ancestor,bool secret){
 PSID owner{};PACL acl{};PSECURITY_DESCRIPTOR sd{};
 if(GetNamedSecurityInfoW(p.c_str(),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&owner,nullptr,&acl,nullptr,&sd))throw ControlFailure(4,"Cannot inspect existing owner state permissions");
 try{
  if(!owner||!trusted(sid_text(owner),user,ancestor)||!acl)throw ControlFailure(4,"State/ancestor ownership requires local owner review");
  constexpr ACCESS_MASK replace=GENERIC_WRITE|GENERIC_ALL|FILE_WRITE_EA|FILE_WRITE_ATTRIBUTES|FILE_DELETE_CHILD|DELETE|WRITE_DAC|WRITE_OWNER;
  const ACCESS_MASK writes=ancestor?replace:replace|FILE_WRITE_DATA|FILE_APPEND_DATA;
  for(DWORD n=0;n<acl->AceCount;n++){
   void* raw{};require(GetAce(acl,n,&raw),"Read state ACL");auto* h=static_cast<ACE_HEADER*>(raw);
   if((h->AceFlags&INHERIT_ONLY_ACE)||h->AceType==ACCESS_DENIED_ACE_TYPE)continue;
   if(h->AceType!=ACCESS_ALLOWED_ACE_TYPE)throw ControlFailure(4,"Unexpected owner state ACE; local review required");
   auto* a=static_cast<ACCESS_ALLOWED_ACE*>(raw);
   if(!trusted(sid_text(&a->SidStart),user,ancestor)&&(secret||(a->Mask&writes)))throw ControlFailure(4,"Another account can access private identity or replace state/ancestor");
  }
  LocalFree(sd);
 }catch(...){LocalFree(sd);throw;}
}
std::optional<json::object> enrollment(const std::filesystem::path& root){
 const auto path=root/"unattended/enrollment.json";auto attr=GetFileAttributesW(path.c_str());
 if(attr==INVALID_FILE_ATTRIBUTES){auto e=GetLastError();if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)return {};throw ControlFailure(4,"Cannot inspect owner enrollment state");}
 verify_owner_state_path(path);auto m=parse(read_file(path)).as_object();const auto sid=process_owner_sid(GetCurrentProcess());
 if(number(m,"schema",1)!=1||text(m,"owner_sid",256)!=sid||text(m,"task_name",256)!="OrganizerWorker-Unattended-"+sid)throw ControlFailure(4,"Enrollment owner/profile mismatch");
 auto id=text(m,"enrollment_id",64);if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw ControlFailure(4,"Unrecognized enrollment provenance");
 const auto state=text(m,"state",16);if(state!="pending"&&state!="enrolled"&&state!="pairing_repair")throw ControlFailure(4,"Unrecognized owner startup state; no fallback");return m;
}
}
std::filesystem::path verified_owner_image(const std::filesystem::path& root,bool hash){
 verify_owner_state_path(root/"current.json");auto m=parse(read_file(root/"current.json")).as_object();if(number(m,"schema",1)!=1)throw ControlFailure(4,"Unknown owner install manifest");
 auto dir=std::filesystem::path(wide(text(m,"directory"))).lexically_normal();auto allowed=(std::filesystem::absolute(root).lexically_normal()/"versions").wstring()+L"\\";
 if(!dir.is_absolute()||dir.wstring().size()<=allowed.size()||_wcsnicmp(dir.c_str(),allowed.c_str(),allowed.size())!=0)throw ControlFailure(4,"Selected image escapes installed owner versions");
 auto exe=dir/"OrganizerWorker.exe";verify_owner_state_path(exe);auto digest=text(m,"sha256",64);
 if(digest.size()!=64||digest.find_first_not_of("0123456789abcdef")!=std::string::npos||(hash&&sha256_file(exe)!=digest))throw ControlFailure(4,"Actual selected executable hash differs");return exe;
}
bool owner_bootstrap_enrolled(const std::filesystem::path& root){auto m=enrollment(root);return m&&text(*m,"state",16)=="enrolled";}
void complete_owner_pairing(const std::filesystem::path& root){auto m=enrollment(root);if(m&&text(*m,"state",16)=="pairing_repair"){(*m)["state"]="enrolled";atomic_write(root/"unattended/enrollment.json",json::serialize(*m));}}
void require_normal_owner(){auto sid=process_owner_sid(GetCurrentProcess());if(elevated()||sid=="S-1-5-18"||sid=="S-1-5-19"||sid=="S-1-5-20")throw ControlFailure(4,"Normal non-elevated owner token required; service/admin engine refused");}
void verify_owner_state_path(const std::filesystem::path& path,bool private_identity){
 auto p=std::filesystem::absolute(path).lexically_normal();if(p.wstring().starts_with(L"\\\\"))throw ControlFailure(4,"Owner state must be a local per-machine path");
 const auto user=process_owner_sid(GetCurrentProcess());bool ancestor=false;
 for(auto q=p;;){
  auto a=GetFileAttributesW(q.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))throw ControlFailure(4,"Missing or reparse owner state path");
  check_acl(q,user,ancestor,!ancestor&&private_identity);
  auto parent=q.parent_path();if(parent==q||parent.empty())break;q=std::move(parent);ancestor=true;
 }
}
}
