#include "control_pipe.hpp"
#include "ow/control.hpp"
#include <sddl.h>
#include <iostream>
using namespace ow;using namespace ow::win;
int wmain(int argc,wchar_t** argv){try{
 if(argc<3)return 2;const auto mode=utf8(argv[1]);const std::filesystem::path root=argv[2];
 if(mode=="serve"){
  ControlPipe pipe(root,[](const json::object&){return json::object{{"schema",1},{"ok",true},{"exit_code",0}};});
  atomic_write(root/"ready.json",json::serialize(json::object{{"pipe",utf8(control_pipe_name(root))},{"owner",process_owner_sid(GetCurrentProcess())}}));
  auto until=monotonic_ms()+60000;while(!std::filesystem::exists(root/"stop")&&monotonic_ms()<until)Sleep(25);return std::filesystem::exists(root/"stop")?0:3;
 }
 if(mode=="name"){std::cout<<utf8(control_pipe_name(root));return 0;}
 if(mode=="call"||mode=="reject"){
  bool rejected=false;try{auto r=control_call(root,control_request({"status"}));if(!r.at("ok").as_bool())return 4;}catch(...){rejected=true;}
  std::cout<<json::serialize(json::object{{"rejected",rejected},{"caller_sid",process_owner_sid(GetCurrentProcess())}})<<'\n';return rejected==(mode=="reject")?0:1;
 }
 if(mode=="deny"&&argc==5){
  Handle h(CreateFileW(argv[2],GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr));const auto error=h?0:GetLastError();auto sid=process_owner_sid(GetCurrentProcess());
  auto ok=!h&&error==ERROR_ACCESS_DENIED&&sid!=utf8(argv[4]);atomic_write(argv[3],json::serialize(json::object{{"denied",ok},{"error",error},{"caller_sid",sid},{"owner_sid",utf8(argv[4])}}));return ok?0:1;
 }
 if(mode=="spoof"&&argc==6){
  PSECURITY_DESCRIPTOR sd{};const auto acl=L"D:P(A;;GA;;;"+std::wstring(argv[4])+L")(A;;GA;;;"+wide(process_owner_sid(GetCurrentProcess()))+L")";require(ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Fixture ACL");SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
  Handle pipe(CreateNamedPipeW(argv[2],PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,4096,4096,2000,&sa));LocalFree(sd);if(!pipe)fail("Spoof fixture bind");atomic_write(argv[3],"ready");
  const bool connected=ConnectNamedPipe(pipe.get(),nullptr)||GetLastError()==ERROR_PIPE_CONNECTED;DWORD bytes{};char buffer[32]{};if(connected)ReadFile(pipe.get(),buffer,sizeof(buffer),&bytes,nullptr);
  atomic_write(argv[5],json::serialize(json::object{{"server_sid",process_owner_sid(GetCurrentProcess())},{"client_bytes",bytes},{"connected",connected}}));return connected&&bytes==0?0:1;
 }
 return 2;
}catch(const std::exception& e){std::cerr<<"OWNER_FIXTURE_FAILURE "<<e.what()<<'\n';return 1;}}
