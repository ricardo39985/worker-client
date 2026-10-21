#include "platform.hpp"
#include "owner_state.hpp"
#include "control_pipe.hpp"
#include "ow/control.hpp"
#include "ow/protocol.hpp"
#include "ow/store.hpp"
#include <sddl.h>
namespace ow::win {
namespace {
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
struct Engine {
 std::filesystem::path root;Handle process;
 Engine(const std::filesystem::path& exe,const std::filesystem::path& r,std::wstring args=L"--headless"):root(r){
  auto line=quote_windows_argument(exe.wstring())+L" "+args;auto desktop=current_desktop_path();STARTUPINFOW si{};si.cb=sizeof(si);si.lpDesktop=desktop.data();PROCESS_INFORMATION pi{};require(CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Launch actual headless image");process.reset(pi.hProcess);CloseHandle(pi.hThread);
  check(process_owner_sid(process.get())==process_owner_sid(GetCurrentProcess())&&!process_elevated(process.get()),"Actual bootstrap child must retain normal existing owner");
 }
 ~Engine(){if(process&&WaitForSingleObject(process.get(),0)==WAIT_TIMEOUT){try{control_call(root,control_request({"exit","drain"}));}catch(...){}WaitForSingleObject(process.get(),15000);}}
 DWORD outcome(){check(WaitForSingleObject(process.get(),30000)==WAIT_OBJECT_0,"Headless exit failed bounded no-UI check");DWORD c{};require(GetExitCodeProcess(process.get(),&c),"Read actual engine exit");return c;}
 json::object ready(){const auto end=monotonic_ms()+180000;json::object last;std::string error;for(;;){check(WaitForSingleObject(process.get(),0)==WAIT_TIMEOUT,"Actual headless bootstrap unexpectedly exited");try{last=control_call(root,control_request({"status"}));if(last.at("headless")==true&&last.at("identity_ready")==true&&last.at("token_elevated")==false&&last.at("processing_ready")==true)return last;}catch(const std::exception& e){error=e.what();}if((last.if_contains("processing_ready")&&last.at("processing_ready")==false)||monotonic_ms()>=end){std::string details="Actual headless CPU probe/readiness deadline; no fake fallback. LastControl="+json::serialize(last)+" LastError="+error;try{details+=" OWNED_SYNTHETIC_BOOTSTRAP_LOG="+read_file(root/"worker.log",65536);}catch(...){}throw std::runtime_error(details);}Sleep(100);}}
};
}
void headless_bootstrap_acceptance(const std::filesystem::path& ffmpeg,const std::string& digest){
 require_normal_owner();const auto root=data_directory();check(!std::filesystem::exists(root),"Default owner root exists: bootstrap fixture NEVER changes an existing installation");
 const auto source=executable_directory()/"OrganizerWorker.exe";
 {Engine missing(source,root);check(missing.outcome()==4,"Missing owner state must reject without UI");check(!std::filesystem::exists(root),"Failed headless bootstrap created owner state");}
 auto sddl=L"D:P(A;OICI;FA;;;"+wide(process_owner_sid(GetCurrentProcess()))+L")(A;OICI;FA;;;SY)";PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Bootstrap fixture ACL");SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};BOOL made=CreateDirectoryW(root.c_str(),&sa);LocalFree(sd);require(made,"Create new disposable profile root");
 const auto version=root/"versions/bootstrap-fixture";std::filesystem::create_directories(version);const auto exe=version/"OrganizerWorker.exe";std::filesystem::copy_file(source,exe);atomic_write(root/"current.json",json::serialize(json::object{{"schema",1},{"directory",utf8(version.wstring())},{"sha256",sha256_file(exe)}}));
 auto doc=control_defaults();doc["coordinator_url"]="https://fixture.invalid";doc["storage_hosts"]=json::array{"fixture.invalid"};doc["ffmpeg_path"]=utf8(ffmpeg.wstring());doc["ffmpeg_sha256"]=digest;doc["limits"]=json::object{{"ram_mb",2048},{"reserve_ram_mb",0},{"scratch_mb",4096},{"cpu_threads",2},{"max_jobs",2},{"vram_mb",0}};atomic_write(root/"worker.json",json::serialize(doc));atomic_write(root/"instance.lock","");
 {Store journal(root/"journal.sqlite3");journal.set("coordinator_origin","https://fixture.invalid");}(void)public_pairing_key(root/"pairing-key.dpapi");const auto key=sha256_file(root/"pairing-key.dpapi");const auto record=protect(json::serialize(json::object{{"coordinator_url","https://fixture.invalid"},{"access_token","fixture-not-an-account"}}));atomic_write(root/"credential.dpapi","corrupt-fixture-dpapi");
 {Engine corrupt(exe,root);check(corrupt.outcome()==4,"Corrupt DPAPI must fail without UI or implicit pairing");check(sha256_file(root/"pairing-key.dpapi")==key,"Rejected DPAPI changed identity");}atomic_write(root/"credential.dpapi",record);
 std::filesystem::rename(root/"pairing-key.dpapi",root/"fixture-saved-key");{Engine absent(exe,root);check(absent.outcome()==4,"Missing key must reject");check(!std::filesystem::exists(root/"pairing-key.dpapi"),"Missing key silently regenerated");}std::filesystem::rename(root/"fixture-saved-key",root/"pairing-key.dpapi");
 // Independent failure collection: a backend failure MUST fail the whole gate,
 // but must not conceal restart/exit/no-UI behavior behind arbitrary test order.
 std::vector<std::string> failures;auto expect=[&](bool ok,const std::string& why){if(!ok)failures.push_back(why);};
 auto probe=[&](Engine& engine){try{return engine.ready();}catch(const std::exception& e){failures.push_back(e.what());return control_call(root,control_request({"status"}));}};
 {Engine denied_enrollment(executable_directory()/"OrganizerWorkerCtl.exe",root,L"--json enroll");auto code=denied_enrollment.outcome();expect(code==4,"Noninteractive enroll actual_exit="+std::to_string(code)+" expected4/no UI");expect(!std::filesystem::exists(root/"unattended"),"Rejected enrollment mutated startup state");}
 DWORD first_pid{};
 {Engine live(exe,root);auto s=probe(live);first_pid=static_cast<DWORD>(number(s,"pid",UINT32_MAX));expect(!s.at("connected").as_bool(),"Fixture claimed real coordinator connectivity");control_call(root,control_request({"resume"}));control_call(root,control_request({"restart"}));auto code=live.outcome();expect(code==75,"Safe restart actual_exit="+std::to_string(code)+" expected75 for caller-independent continuation");}
 {Engine replacement(exe,root);auto s=probe(replacement);expect(number(s,"pid",UINT32_MAX)!=first_pid&&!s.at("paused").as_bool(),"Actual replacement lost PID or durable admission intent");expect(sha256_file(root/"pairing-key.dpapi")==key,"Replacement rotated identity");control_call(root,control_request({"exit","drain"}));expect(replacement.outcome()==0,"Explicit drain exit must not schedule a restart");}
 try{Engine malformed(exe,root,L"--headless invalid-option");expect(malformed.outcome()==2,"Malformed headless launch must reject with2/no UI");}catch(const std::exception& e){failures.push_back(std::string("MALFORMED_HEADLESS_NO_UI_FAILED: ")+e.what());}
 if(!failures.empty()){std::string combined="BOOTSTRAP_GATES_FAILED (NONE waived): ";for(const auto& failure:failures)combined+=failure+" | ";throw std::runtime_error(combined);}
 // Current profile/cached-DPAPI synthetic acceptance, NOT fresh-boot enrollment.
}
}
