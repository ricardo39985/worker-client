#include "owner_task.hpp"
#include "control_pipe.hpp"
#include "ow/lifecycle.hpp"
#include "ow/control.hpp"
#include <taskschd.h>
#include <wincred.h>
#include <sddl.h>
#include <wrl/client.h>
#include <vector>
namespace ow::win {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT r,const char* stage){if(FAILED(r))throw ControlFailure(4,stage);}
struct Apartment {HRESULT r=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Apartment(){if(FAILED(r)&&r!=RPC_E_CHANGED_MODE)throw ControlFailure(4,"Cannot initialize task service");}~Apartment(){if(SUCCEEDED(r))CoUninitialize();}};
struct Bstr {BSTR p{};explicit Bstr(const std::wstring& s):p(SysAllocStringLen(s.data(),static_cast<UINT>(s.size()))){if(!p)throw std::bad_alloc();}~Bstr(){SysFreeString(p);}operator BSTR()const{return p;}};
std::wstring take(BSTR p){std::wstring r=p?std::wstring(p,SysStringLen(p)):std::wstring{};SysFreeString(p);return r;}
struct Secret {std::vector<wchar_t> data;explicit Secret(std::size_t n):data(n){}~Secret(){SecureZeroMemory(data.data(),data.size()*sizeof(wchar_t));}};
struct Variant {VARIANT v;Variant(){VariantInit(&v);}explicit Variant(const std::wstring& s):Variant(){v.vt=VT_BSTR;v.bstrVal=SysAllocStringLen(s.data(),static_cast<UINT>(s.size()));if(!v.bstrVal)throw std::bad_alloc();}~Variant(){if(v.vt==VT_BSTR&&v.bstrVal)SecureZeroMemory(v.bstrVal,SysStringByteLen(v.bstrVal));VariantClear(&v);}};
std::wstring owner(){return wide(process_owner_sid(GetCurrentProcess()));}
std::wstring task_name(){return L"OrganizerWorker-Unattended-"+owner();}
std::wstring account_sid(const std::wstring& name){PSID parsed{};if(ConvertStringSidToSidW(name.c_str(),&parsed)){LPWSTR value{};require(ConvertSidToStringSidW(parsed,&value),"Read task SID");std::wstring result(value);LocalFree(value);LocalFree(parsed);return result;}DWORD bytes{},chars{};SID_NAME_USE kind{};LookupAccountNameW(nullptr,name.c_str(),nullptr,&bytes,nullptr,&chars,&kind);std::vector<unsigned char> sid(bytes);std::vector<wchar_t> domain(chars);require(bytes&&LookupAccountNameW(nullptr,name.c_str(),sid.data(),&bytes,domain.data(),&chars,&kind),"Resolve task owner SID");LPWSTR text{};require(ConvertSidToStringSidW(sid.data(),&text),"Format task owner SID");std::wstring value(text);LocalFree(text);return value;}
void no_reparse(const std::filesystem::path& p){auto q=std::filesystem::absolute(p);for(;;){auto a=GetFileAttributesW(q.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))throw ControlFailure(4,"Missing/reparse owner launch state");auto parent=q.parent_path();if(parent==q||parent.empty())break;q=std::move(parent);}}
std::filesystem::path powershell(){std::wstring path(32768,0);auto n=GetSystemDirectoryW(path.data(),static_cast<UINT>(path.size()));if(!n||n>=path.size())throw ControlFailure(4,"Cannot locate native Windows task host");path.resize(n);return std::filesystem::path(path)/L"WindowsPowerShell/v1.0/powershell.exe";}
std::wstring arguments(const std::filesystem::path& root){return L"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+quote_windows_argument((root/L"unattended/launch.ps1").wstring());}
struct Tasks {
 Apartment apartment;ComPtr<ITaskService> service;ComPtr<ITaskFolder> folder;
 Tasks(){checked(CoCreateInstance(CLSID_TaskScheduler,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&service)),"Task service unavailable");Variant empty;checked(service->Connect(empty.v,empty.v,empty.v,empty.v),"Task service connection denied");checked(service->GetFolder(Bstr(L"\\"),&folder),"Owner task folder unavailable");}
 ComPtr<IRegisteredTask> get(bool required=true){ComPtr<IRegisteredTask> t;auto r=folder->GetTask(Bstr(task_name()),&t);if(FAILED(r)&&required)throw ControlFailure(4,"Unattended enrollment required: run installed CLI enroll in LOCAL owner Windows UI");return t;}
};
void task_acl(IRegisteredTask* task){BSTR raw{};checked(task->GetSecurityDescriptor(DACL_SECURITY_INFORMATION,&raw),"Cannot inspect owner task ACL");auto s=take(raw);PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(s.c_str(),SDDL_REVISION_1,&sd,nullptr),"Read owner task ACL");
 try{BOOL present{},defaulted{};PACL acl{};if(!GetSecurityDescriptorDacl(sd,&present,&acl,&defaulted)||!present||!acl)throw ControlFailure(4,"Owner task has no restrictive DACL");bool have_owner=false,have_system=false;
  for(DWORD n=0;n<acl->AceCount;n++){void* ptr{};require(GetAce(acl,n,&ptr),"Read task ACE");auto* header=static_cast<ACE_HEADER*>(ptr);if(header->AceType!=ACCESS_ALLOWED_ACE_TYPE)throw ControlFailure(4,"Unexpected task ACE; local review required");auto* ace=static_cast<ACCESS_ALLOWED_ACE*>(ptr);LPWSTR sid{};require(ConvertSidToStringSidW(&ace->SidStart,&sid),"Read task ACE owner");std::wstring value(sid);LocalFree(sid);if(value==owner())have_owner=true;else if(value==L"S-1-5-18")have_system=true;else throw ControlFailure(4,"Task grants another account access");}
  if(!have_owner||!have_system)throw ControlFailure(4,"Task owner/System permissions missing");LocalFree(sd);
 }catch(...){LocalFree(sd);throw;}
}
LaunchTask expected(const std::filesystem::path& root){return {utf8(owner()),TaskLogon::password,true,true,true,true,0,{{utf8(powershell().wstring()),utf8(arguments(root)),utf8(root.wstring())}}};}
void verify(Tasks& tasks,const std::filesystem::path& root){
 no_reparse(root);no_reparse(root/"unattended");no_reparse(root/"unattended/launch.ps1");no_reparse(root/"unattended/enrollment.json");
 if(sha256_file(root/"unattended/launch.ps1")!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Installed owner launcher differs from reviewed version");
 auto marker=parse(read_file(root/"unattended/enrollment.json")).as_object();if(number(marker,"schema",1)!=1||text(marker,"owner_sid",256)!=utf8(owner())||text(marker,"task_name",256)!=utf8(task_name()))throw ControlFailure(4,"Enrollment owner/profile mismatch");
 auto task=tasks.get();task_acl(task.Get());ComPtr<ITaskDefinition> d;checked(task->get_Definition(&d),"Cannot inspect enrolled task");ComPtr<IPrincipal> p;checked(d->get_Principal(&p),"Cannot inspect task principal");BSTR value{};checked(p->get_UserId(&value),"Cannot read task owner");auto sid=account_sid(take(value));TASK_LOGON_TYPE logon{};TASK_RUNLEVEL level{};checked(p->get_LogonType(&logon),"Cannot read task logon");checked(p->get_RunLevel(&level),"Cannot read task runlevel");
 ComPtr<ITaskSettings> settings;checked(d->get_Settings(&settings),"Cannot inspect task settings");VARIANT_BOOL demand{},enabled{},stop_battery{},defer_battery{},when_available{};TASK_INSTANCES_POLICY multiple{};checked(settings->get_StopIfGoingOnBatteries(&stop_battery),"Cannot inspect safe power policy");checked(settings->get_DisallowStartIfOnBatteries(&defer_battery),"Cannot inspect startup power policy");checked(settings->get_StartWhenAvailable(&when_available),"Cannot inspect startup retry policy");if(stop_battery||defer_battery||!when_available)throw ControlFailure(4,"Task power/availability policy would interrupt unattended work");checked(settings->get_AllowDemandStart(&demand),"Cannot read demand start");checked(settings->get_Enabled(&enabled),"Cannot read task enabled");checked(settings->get_MultipleInstances(&multiple),"Cannot read instance policy");checked(settings->get_ExecutionTimeLimit(&value),"Cannot read execution limit");auto duration=take(value);
 ComPtr<ITriggerCollection> triggers;checked(d->get_Triggers(&triggers),"Cannot inspect task triggers");LONG count{};checked(triggers->get_Count(&count),"Cannot count task triggers");bool boot=false;if(count==1){ComPtr<ITrigger> trigger;checked(triggers->get_Item(1,&trigger),"Cannot read boot trigger");TASK_TRIGGER_TYPE2 type{};VARIANT_BOOL active{};checked(trigger->get_Type(&type),"Cannot inspect trigger type");checked(trigger->get_Enabled(&active),"Cannot inspect trigger enabled");boot=type==TASK_TRIGGER_BOOT&&active!=VARIANT_FALSE;}
 LaunchTask actual{utf8(sid),logon==TASK_LOGON_PASSWORD?TaskLogon::password:logon==TASK_LOGON_S4U?TaskLogon::s4u:logon==TASK_LOGON_INTERACTIVE_TOKEN?TaskLogon::interactive:TaskLogon::service,level==TASK_RUNLEVEL_LUA,boot,demand!=VARIANT_FALSE,multiple==TASK_INSTANCES_IGNORE_NEW,duration==L"PT0S"?0u:1u,{}};
 ComPtr<IActionCollection> actions;checked(d->get_Actions(&actions),"Cannot inspect task actions");checked(actions->get_Count(&count),"Cannot count actions");for(LONG n=1;n<=count;n++){ComPtr<IAction> a;ComPtr<IExecAction> e;checked(actions->get_Item(n,&a),"Cannot read action");checked(a.As(&e),"Only fixed executable action allowed");checked(e->get_Path(&value),"Cannot read action image");auto image=take(value);checked(e->get_Arguments(&value),"Cannot read action arguments");auto args=take(value);checked(e->get_WorkingDirectory(&value),"Cannot read action directory");actual.actions.push_back({utf8(image),utf8(args),utf8(take(value))});}
 if(!enabled)throw ControlFailure(4,"Enrolled task is disabled");try{validate_launch_task(actual,expected(root));}catch(...){throw ControlFailure(4,"Enrolled task policy changed: owner Password/Limited/fixed boot action required");}
}
void create_private_directory(const std::filesystem::path& p){
 if(std::filesystem::exists(p)){no_reparse(p);return;}auto sddl=L"D:P(A;OICI;FA;;;"+owner()+L")(A;OICI;FA;;;SY)";PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Build owner enrollment ACL");SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};bool made=CreateDirectoryW(p.c_str(),&sa)!=FALSE;auto error=GetLastError();LocalFree(sd);if(!made)fail("Create private enrollment directory",error);
}
json::object result(){return {{"schema",1},{"ok",true},{"exit_code",0},{"observed_at_utc_ms",utc_ms()}};}
}
json::object inspect_owner_task(const std::filesystem::path& root){Tasks tasks;verify(tasks,root);auto r=result();r["enrolled"]=true;r["cold_boot_verified"]=false;r["task_name"]=utf8(task_name());return r;}
json::object enroll_owner_task(const std::filesystem::path& root){
 DWORD session{};require(ProcessIdToSessionId(GetCurrentProcessId(),&session),"Read enrollment session");USEROBJECTFLAGS station{};DWORD length{};
 if(!session||!elevated()||!GetUserObjectInformationW(GetProcessWindowStation(),UOI_FLAGS,&station,sizeof(station),&length)||!(station.dwFlags&WSF_VISIBLE))throw ControlFailure(4,"Enrollment requires LOCAL Windows owner UI, elevated for boot-task registration only; never SSH/credentials in arguments");
 no_reparse(root);for(const auto& name:{"worker.json","credential.dpapi","pairing-key.dpapi","journal.sqlite3"})no_reparse(root/name);
 const auto source=executable_directory()/"start-headless.ps1";no_reparse(source);if(sha256_file(source)!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Reviewed packaged headless launcher required");
 Tasks tasks;auto existing=tasks.get(false);if(existing)verify(tasks,root); // never overwrite an unknown/preexisting task
 const auto directory=root/"unattended";create_private_directory(directory);const auto launcher=directory/"launch.ps1";
 if(std::filesystem::exists(launcher)){no_reparse(launcher);if(sha256_file(launcher)!=OW_HEADLESS_LAUNCHER_SHA256)throw ControlFailure(4,"Preexisting enrollment launcher differs; retained for review");}else atomic_write(launcher,read_file(source));
 if(existing){BSTR xml{};checked(existing->get_Xml(&xml),"Cannot snapshot prior owner task");atomic_write(directory/("task-before-"+random_hex(8)+".xml"),utf8(take(xml)));}
 // Windows secure desktop owns the one-time prompt. No command/stdin/env/file
 // accepts a password, and no existing Windows credentials are extracted.
 CREDUI_INFOW ui{sizeof(ui),nullptr,L"Enroll this SAME owner for LIMITED unattended worker startup. Use the Windows account password, never a chat/script.",L"Organizer Worker unattended owner enrollment",nullptr};ULONG package{},bytes{};void* auth{};BOOL save=FALSE;
 auto error=CredUIPromptForWindowsCredentialsW(&ui,0,&package,nullptr,0,&auth,&bytes,&save,CREDUIWIN_SECURE_PROMPT);if(error==ERROR_CANCELLED)throw ControlFailure(4,"Enrollment cancelled; no task changed");if(error)throw ControlFailure(4,"Secure local Windows credential UI unavailable");
 bool registered=false;
 try{
  DWORD users=1024,domains=1024,passwords=4096;Secret user(users),domain(domains),password(passwords);
  if(!CredUnPackAuthenticationBufferW(CRED_PACK_PROTECTED_CREDENTIALS,auth,bytes,user.data.data(),&users,domain.data.data(),&domains,password.data.data(),&passwords))throw ControlFailure(4,"Use an owner credential supported by Windows password task registration");
  std::wstring account=domain.data[0]?std::wstring(domain.data.data())+L"\\"+user.data.data():std::wstring(user.data.data());if(account_sid(account)!=owner()||!password.data[0])throw ControlFailure(4,"Only the SAME owner password credential is permitted; no identity migration/S4U fallback");
  ComPtr<ITaskDefinition> d;checked(tasks.service->NewTask(0,&d),"Cannot prepare owner task");ComPtr<IPrincipal> p;checked(d->get_Principal(&p),"Cannot prepare principal");checked(p->put_UserId(Bstr(account)),"Cannot set owner");checked(p->put_LogonType(TASK_LOGON_PASSWORD),"Cannot set Password logon");checked(p->put_RunLevel(TASK_RUNLEVEL_LUA),"Cannot set LIMITED runlevel");
  ComPtr<ITaskSettings> s;checked(d->get_Settings(&s),"Cannot prepare settings");checked(s->put_Enabled(VARIANT_TRUE),"Cannot enable owner task");checked(s->put_AllowDemandStart(VARIANT_TRUE),"Cannot enable owner activation");checked(s->put_StartWhenAvailable(VARIANT_TRUE),"Cannot enable delayed availability");checked(s->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW),"Cannot prevent duplicate engines");checked(s->put_ExecutionTimeLimit(Bstr(L"PT0S")),"Cannot preserve long jobs");checked(s->put_DisallowStartIfOnBatteries(VARIANT_FALSE),"Cannot set power policy");checked(s->put_StopIfGoingOnBatteries(VARIANT_FALSE),"Cannot preserve battery jobs");checked(s->put_RestartInterval(Bstr(L"PT1M")),"Cannot set bounded failure retry");checked(s->put_RestartCount(3),"Cannot set retry count");
  ComPtr<ITriggerCollection> triggers;ComPtr<ITrigger> t;ComPtr<IBootTrigger> boot;checked(d->get_Triggers(&triggers),"Cannot prepare boot trigger");checked(triggers->Create(TASK_TRIGGER_BOOT,&t),"Cannot create boot trigger");checked(t.As(&boot),"Cannot inspect boot trigger");checked(boot->put_Delay(Bstr(L"PT30S")),"Cannot set boot delay");checked(t->put_Enabled(VARIANT_TRUE),"Cannot enable boot trigger");
  ComPtr<IActionCollection> actions;ComPtr<IAction> a;ComPtr<IExecAction> exec;checked(d->get_Actions(&actions),"Cannot prepare fixed action");checked(actions->Create(TASK_ACTION_EXEC,&a),"Cannot create fixed action");checked(a.As(&exec),"Cannot set fixed action");checked(exec->put_Path(Bstr(powershell().wstring())),"Cannot set native task host");checked(exec->put_Arguments(Bstr(arguments(root))),"Cannot set fixed owner launcher");checked(exec->put_WorkingDirectory(Bstr(root.wstring())),"Cannot set fixed owner directory");
  Variant uid(account),pw;pw.v.vt=VT_BSTR;pw.v.bstrVal=SysAllocString(password.data.data());if(!pw.v.bstrVal)throw std::bad_alloc();Variant acl(L"D:P(A;;GA;;;SY)(A;;GA;;;"+owner()+L")");ComPtr<IRegisteredTask> task;
  auto hr=tasks.folder->RegisterTaskDefinition(Bstr(task_name()),d.Get(),(existing?TASK_UPDATE:TASK_CREATE)|TASK_IGNORE_REGISTRATION_TRIGGERS,uid.v,pw.v,TASK_LOGON_PASSWORD,acl.v,&task);checked(hr,"Windows owner credential registration failed; no S4U/password extraction fallback");registered=true;
  atomic_write(directory/"enrollment.json",json::serialize(json::object{{"schema",1},{"owner_sid",utf8(owner())},{"task_name",utf8(task_name())},{"source",OW_BUILD_REVISION},{"cold_boot_verified",false}}));
 }catch(...){SecureZeroMemory(auth,bytes);CoTaskMemFree(auth);if(registered&&!existing)tasks.folder->DeleteTask(Bstr(task_name()),0);throw;}
 SecureZeroMemory(auth,bytes);CoTaskMemFree(auth);verify(tasks,root);auto r=result();r["enrolled"]=true;r["cold_boot_verified"]=false;r["credential_stored_by_windows"]=true;r["owner_step"]= "Run start/restart for in-context checks; fresh approved boot acceptance remains pending";return r;
}
json::object owner_lifecycle(const std::filesystem::path& root,bool restart,Tick timeout){
 no_reparse(root);Handle lock(CreateFileW((root/"lifecycle.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock)throw ControlFailure(3,"Another owner lifecycle operation is in progress");
 struct Host final:LifecycleHost{
  std::filesystem::path root;Tasks tasks;json::object last;Handle old;bool verified{};std::uint32_t verified_pid{};std::int64_t verified_started{};std::string verified_hash;
  explicit Host(std::filesystem::path p):root(std::move(p)){}
  void policy(){if(!verified){verify(tasks,root);verified=true;}}
  std::optional<LifecycleStatus> observe()override{
   try{last=control_call(root,control_request({"status"}));}catch(...){Handle guard(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(!guard&&GetLastError()==ERROR_SHARING_VIOLATION)throw ControlFailure(3,"Owner controller is busy/unavailable; no second process started");return {};}
   if(!last.at("ok").as_bool())return {};auto pid=static_cast<std::uint32_t>(number(last,"pid",UINT32_MAX));
   auto c=parse(read_file(root/"current.json")).as_object();auto exe=std::filesystem::path(wide(text(c,"directory")))/"OrganizerWorker.exe";
   Handle h(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));if(!h)throw ControlFailure(4,"Live owner process disappeared during verification");std::wstring path(32768,0);DWORD n=static_cast<DWORD>(path.size());require(QueryFullProcessImageNameW(h.get(),0,path.data(),&n),"Read live lifecycle image");path.resize(n);
   const auto started=static_cast<std::int64_t>(number(last,"process_started_at_utc_ms",INT64_MAX));const auto hash=text(c,"sha256",64);
   if(std::filesystem::path(path)!=exe||process_owner_sid(h.get())!=process_owner_sid(GetCurrentProcess()))throw ControlFailure(4,"Actual live image/owner differs from installed manifest");
   if(pid!=verified_pid||started!=verified_started||hash!=verified_hash){no_reparse(exe);if(sha256_file(exe)!=hash)throw ControlFailure(4,"Actual image hash differs");verified_pid=pid;verified_started=started;verified_hash=hash;}
   const bool ready=last.at("connected").as_bool()&&!last.at("processing_ready").is_null();return LifecycleStatus{pid,ready,last.at("exiting").as_bool()};
  }
  void request_restart(const LifecycleStatus& s)override{policy();old.reset(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,s.pid));if(!old)throw ControlFailure(4,"Cannot pin actual restart process");auto r=control_call(root,control_request({"restart"}));if(r.at("ok")!=true)throw ControlFailure(4,"Installed controller does not support safe restart; upgrade without cancelling work");}
  bool alive(const LifecycleStatus& s)override{if(!old)old.reset(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,s.pid));return old&&WaitForSingleObject(old.get(),0)==WAIT_TIMEOUT;}
  void start_enrolled()override{policy();auto task=tasks.get();TASK_STATE state{};checked(task->get_State(&state),"Cannot read task lifecycle state");const auto until=monotonic_ms()+10000;while(state==TASK_STATE_RUNNING&&monotonic_ms()<until){Sleep(100);checked(task->get_State(&state),"Cannot wait previous task lifetime");}if(state==TASK_STATE_RUNNING)throw ControlFailure(3,"Prior task is still draining; no duplicate start");Variant empty;ComPtr<IRunningTask> running;checked(task->Run(empty.v,&running),"Owner task activation failed; re-enroll locally after credential rotation");}
  Tick now()override{return monotonic_ms();}void wait()override{Sleep(100);}
 } host(root);
 try{auto live=run_lifecycle(host,restart,timeout);auto r=host.last;r["lifecycle_ready"]=true;r["pid"]=live.pid;r["cold_boot_verified"]=false;return r;}catch(const ControlFailure&){throw;}catch(...){throw ControlFailure(3,"Lifecycle pending: drain/ACK/network/probe readiness not complete; no job was cancelled");}
}
}
