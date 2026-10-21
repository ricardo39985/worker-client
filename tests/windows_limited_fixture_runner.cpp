#include "platform.hpp"
#include "control_pipe.hpp"
#include "ow/protocol.hpp"
#include <sddl.h>
#include <iostream>
// Test-only same-owner privilege reduction; never a product launch/credential path.
// No account creation/logon, credentials, service or scheduled task.
// Documented LUA_TOKEN is required: privilege stripping alone retains the
// original elevation flag. Verify actual TokenElevation; never mask/waive it.
// https://learn.microsoft.com/windows/win32/api/securitybaseapi/nf-securitybaseapi-createrestrictedtoken
namespace {
struct FixtureStation {
 HWINSTA original=GetProcessWindowStation(),station{};HDESK original_desktop=GetThreadDesktop(GetCurrentThreadId()),desktop{};std::wstring name,qualified;
 FixtureStation(){using namespace ow::win;name=L"OrganizerWorker-Fixture-"+wide(random_hex(12));qualified=name+L"\\Default";
  auto sddl=L"D:P(A;;GA;;;"+wide(process_owner_sid(GetCurrentProcess()))+L")(A;;GA;;;SY)S:(ML;;NW;;;ME)";PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Private fixture station ACL");SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
  try{station=CreateWindowStationW(name.c_str(),CWF_CREATE_ONLY,WINSTA_ALL_ACCESS,&sa);require(station!=nullptr,"Create NEW private fixture station");require(SetProcessWindowStation(station),"Select fixture-only station");desktop=CreateDesktopW(L"Default",nullptr,nullptr,0,GENERIC_ALL,&sa);require(desktop!=nullptr,"Create private noninteractive desktop");require(SetProcessWindowStation(original),"Restore test caller station");require(SetThreadDesktop(original_desktop),"Restore test caller desktop");LocalFree(sd);}
  catch(...){SetProcessWindowStation(original);SetThreadDesktop(original_desktop);if(desktop)CloseDesktop(desktop);if(station)CloseWindowStation(station);LocalFree(sd);throw;}
 }
 ~FixtureStation(){if(desktop)CloseDesktop(desktop);if(station)CloseWindowStation(station);}
};
}
int wmain(int argc,wchar_t** argv){using namespace ow;using namespace ow::win;try{
 if(argc!=4)throw std::runtime_error("Usage: limited-fixture-runner NEW_ROOT APPROVED_FFMPEG SHA256");
 HANDLE raw{};require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY|TOKEN_DUPLICATE|TOKEN_ASSIGN_PRIMARY,&raw),"Open own test token");Handle parent(raw);HANDLE reduced{};
 PSID admin{};require(ConvertStringSidToSidW(L"S-1-5-32-544",&admin),"Test admin SID");SID_AND_ATTRIBUTES disabled{admin,0};auto ok=CreateRestrictedToken(parent.get(),DISABLE_MAX_PRIVILEGE|LUA_TOKEN,1,&disabled,0,nullptr,0,nullptr,&reduced);LocalFree(admin);require(ok,"Reduce own test token");Handle token(reduced);
 TOKEN_ELEVATION e{};DWORD n{};require(GetTokenInformation(token.get(),TokenElevation,&e,sizeof(e),&n),"Read actual limited elevation");if(e.TokenIsElevated)throw std::runtime_error("Privilege reduction did not produce a normal token; acceptance is NOT established");
 DWORD required{};GetTokenInformation(token.get(),TokenDefaultDacl,nullptr,0,&required);std::vector<unsigned char> acl_info(required);require(GetTokenInformation(token.get(),TokenDefaultDacl,acl_info.data(),required,&required),"Inspect fixture token DACL");SECURITY_DESCRIPTOR descriptor{};require(InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION),"Describe fixture token");require(SetSecurityDescriptorDacl(&descriptor,TRUE,reinterpret_cast<TOKEN_DEFAULT_DACL*>(acl_info.data())->DefaultDacl,FALSE),"Describe default DACL");LPWSTR dacl_text{};require(ConvertSecurityDescriptorToStringSecurityDescriptorW(&descriptor,SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&dacl_text,nullptr),"Read test-only DACL metadata");std::cout<<"TEST_ONLY_REDUCED_DEFAULT_DACL="<<utf8(dacl_text)<<"\n";LocalFree(dacl_text);
 GetTokenInformation(token.get(),TokenIntegrityLevel,nullptr,0,&required);std::vector<unsigned char> il_info(required);require(GetTokenInformation(token.get(),TokenIntegrityLevel,il_info.data(),required,&required),"Inspect actual fixture integrity");auto integrity=reinterpret_cast<TOKEN_MANDATORY_LABEL*>(il_info.data())->Label.Sid;std::cout<<"TEST_ONLY_INTEGRITY_RID="<<*GetSidSubAuthority(integrity,*GetSidSubAuthorityCount(integrity)-1)<<"\n";
 // Source BA-only default objects and HIGH IL survive LUA_TOKEN (actual red33).
 // Narrow to own-user objects / at most MEDIUM IL, adding no privileges/SID/logon.
 auto user_sid=wide(process_owner_sid(GetCurrentProcess()));PSECURITY_DESCRIPTOR private_sd{};auto private_acl=L"D:P(A;;GA;;;"+user_sid+L")(A;;GA;;;SY)";require(ConvertStringSecurityDescriptorToSecurityDescriptorW(private_acl.c_str(),SDDL_REVISION_1,&private_sd,nullptr),"Build test-only own-user object DACL");
 try{PACL acl{};BOOL present{},defaults{};require(GetSecurityDescriptorDacl(private_sd,&present,&acl,&defaults),"Read test-only DACL");TOKEN_DEFAULT_DACL dacl{acl};require(SetTokenInformation(token.get(),TokenDefaultDacl,&dacl,sizeof(dacl)),"Set fixture own-user object access");}catch(...){LocalFree(private_sd);throw;}LocalFree(private_sd);
 PSID user{};require(ConvertStringSidToSidW(user_sid.c_str(),&user),"Read unchanged fixture SID");TOKEN_OWNER selected_owner{user};auto selected=SetTokenInformation(token.get(),TokenOwner,&selected_owner,sizeof(selected_owner));LocalFree(user);require(selected,"Set fixture own-user object ownership");
 if(*GetSidSubAuthority(integrity,*GetSidSubAuthorityCount(integrity)-1)>SECURITY_MANDATORY_MEDIUM_RID){PSID medium{};require(ConvertStringSidToSidW(L"S-1-16-8192",&medium),"Describe medium integrity");TOKEN_MANDATORY_LABEL label{{medium,SE_GROUP_INTEGRITY}};auto lowered=SetTokenInformation(token.get(),TokenIntegrityLevel,&label,sizeof(label)+GetLengthSid(medium));LocalFree(medium);require(lowered,"LOWER fixture integrity");}
 // Never modify existing desktops. Explicit CI-only context, NOT a fresh logon.
 FixtureStation station;auto exe=executable_directory()/"ow_unattended_fixture.exe";std::wstring command=quote_windows_argument(exe.wstring());for(int i=1;i<argc;i++)command+=L" "+quote_windows_argument(argv[i]);STARTUPINFOW si{};si.cb=sizeof(si);si.lpDesktop=station.qualified.data();PROCESS_INFORMATION pi{};
 require(CreateProcessAsUserW(token.get(),exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi),"Launch same-owner limited fixture");Handle process(pi.hProcess),thread(pi.hThread);if(process_owner_sid(process.get())!=process_owner_sid(GetCurrentProcess())||process_elevated(process.get()))throw std::runtime_error("Actual child owner/elevation mismatch");
 if(WaitForSingleObject(process.get(),600000)!=WAIT_OBJECT_0)throw std::runtime_error("Fixture still pending at acceptance deadline; no forced process termination");DWORD code{};require(GetExitCodeProcess(process.get(),&code),"Read fixture outcome");return static_cast<int>(code);
 }catch(const std::exception& e){std::cerr<<"LIMITED_FIXTURE_REJECTED: "<<e.what()<<"\n";return 1;}}
