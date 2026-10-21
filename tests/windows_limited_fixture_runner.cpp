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
 // Do NOT grant access to or modify the existing interactive desktop. A stripped
 // Administrator token can start the engine there yet FFmpeg DLL initialization
 // fails (actual0xc0000142). Model a valid owner-only noninteractive station.
 FixtureStation station;auto exe=executable_directory()/"ow_unattended_fixture.exe";std::wstring command=quote_windows_argument(exe.wstring());for(int i=1;i<argc;i++)command+=L" "+quote_windows_argument(argv[i]);STARTUPINFOW si{};si.cb=sizeof(si);si.lpDesktop=station.qualified.data();PROCESS_INFORMATION pi{};
 require(CreateProcessAsUserW(token.get(),exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi),"Launch same-owner limited fixture");Handle process(pi.hProcess),thread(pi.hThread);if(process_owner_sid(process.get())!=process_owner_sid(GetCurrentProcess())||process_elevated(process.get()))throw std::runtime_error("Actual child owner/elevation mismatch");
 if(WaitForSingleObject(process.get(),600000)!=WAIT_OBJECT_0)throw std::runtime_error("Fixture still pending at acceptance deadline; no forced process termination");DWORD code{};require(GetExitCodeProcess(process.get(),&code),"Read fixture outcome");return static_cast<int>(code);
 }catch(const std::exception& e){std::cerr<<"LIMITED_FIXTURE_REJECTED: "<<e.what()<<"\n";return 1;}}
