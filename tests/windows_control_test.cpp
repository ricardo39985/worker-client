#include "test.hpp"
#include "worker.hpp"
#include "control_pipe.hpp"
#include "headless.hpp"
#include "owner_state.hpp"
#include <sddl.h>
#include <future>
using namespace ow;using namespace ow::win;
namespace {
struct Workspace {
 std::filesystem::path root=std::filesystem::temp_directory_path()/("ow-control-test-"+random_hex(12));
 Workspace(){std::filesystem::create_directories(root);atomic_write(root/"worker.json",json::serialize(control_defaults()));}
 ~Workspace(){std::error_code ec;std::filesystem::remove_all(root,ec);}
};
}
TEST("native owner IPC resumes already running controller and reports fresh live state"){
 Workspace t;Worker worker(load_config(t.root));worker.pause(true);ControlPipe pipe(t.root,[&](const json::object& request){return worker.control(request);});
 auto first=control_call(t.root,control_request({"status"}));CHECK(first.at("live")==true);CHECK(first.at("paused")==true);CHECK(first.at("connected")==false);CHECK(first.at("accepting")==false);CHECK(first.at("agent_version")=="0.1.2");CHECK(first.contains("build_revision"));
 CHECK(control_call(t.root,control_request({"resume"})).at("paused")==false);CHECK(!worker.paused());CHECK(control_call(t.root,control_request({"resume"})).at("paused")==false);
 auto later=control_call(t.root,control_request({"status"}));CHECK(number(later,"observed_at_utc_ms",INT64_MAX)>=number(first,"observed_at_utc_ms",INT64_MAX));CHECK(later.at("pid")==first.at("pid"));
 control_call(t.root,control_request({"drain"}));CHECK(worker.paused());CHECK(!worker.ready_to_exit());
 worker.pause(false);CHECK(control_call(t.root,control_request({"status"})).at("paused")==false);
}
TEST("native atomic config readback shared with tray and restart pending limits"){
 Workspace t;Worker worker(load_config(t.root));ControlPipe pipe(t.root,[&](const json::object& r){return worker.control(r);});
 auto invalid=control_call(t.root,control_request({"config","set","--stdin"},"{\"operator.priority\":\"low\",\"limits.cpu_threads\":0}"));CHECK(invalid.at("ok")==false);CHECK(worker.priority()==Priority::normal);
 auto result=control_call(t.root,control_request({"config","set","--stdin"},"{\"operator.priority\":\"low\",\"limits.cpu_threads\":4}"));CHECK(result.at("ok")==true);CHECK(worker.priority()==Priority::low);CHECK(load_config(t.root).budget.cpu_threads==4);
 auto cpu=control_call(t.root,control_request({"config","get","limits.cpu_threads"})).at("settings").as_array().front().as_object();CHECK(cpu.at("restart_required")==true);CHECK(cpu.at("effective")==2);
 worker.priority(Priority::backup);auto tray=control_call(t.root,control_request({"config","get","operator.priority"})).at("settings").as_array().front().as_object();CHECK(tray.at("current")=="backup");CHECK(tray.at("effective")=="backup");
 auto a=std::async(std::launch::async,[&]{for(int i=0;i<4;++i)worker.preference(true,Preference::disabled);});
 for(int i=0;i<4;++i)CHECK(control_call(t.root,control_request({"config","set","--stdin"},"{\"operator.image_preference\":\"preferred\"}")).at("ok")==true);a.get();CHECK(worker.preference(true)==Preference::disabled);CHECK(worker.preference(false)==Preference::preferred);
}
TEST("native legacy settings migrate once preserving subsequent file settings"){
 Workspace t;auto doc=control_defaults();doc.erase("operator");atomic_write(t.root/"worker.json",json::serialize(doc));Store store(t.root/"journal.sqlite3");store.set("paused","true");store.set("priority","backup");
 auto migrated=load_control_document(t.root,store);CHECK(migrated.at("operator").as_object().at("paused")==true);CHECK(migrated.at("operator").as_object().at("priority")=="backup");
 store.set("priority","normal");CHECK(load_control_document(t.root,store)==migrated);
}
TEST("native pipe rejects anonymous caller duplicate server bad schema and unsafe fields"){
 Workspace t;Worker worker(load_config(t.root));ControlPipe pipe(t.root,[&](const json::object& r){return worker.control(r);});THROWS(ControlPipe(t.root,[](const json::object&){return json::object{};}));
 auto bad=control_request({"status"});bad["schema"]=2;CHECK(control_call(t.root,bad).at("ok")==false);
 auto invalid=control_request({"config","set","--stdin"},"{\"../credential.dpapi\":\"bad\"}");CHECK(control_call(t.root,invalid).at("ok")==false);
 const auto name=control_pipe_name(t.root);require(WaitNamedPipeW(name.c_str(),2000),"Wait for anonymous caller test");require(ImpersonateAnonymousToken(GetCurrentThread()),"Impersonate anonymous test client");HANDLE raw=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);const auto error=GetLastError();require(RevertToSelf(),"Restore test identity");Handle attempted(raw);CHECK(!attempted);CHECK(error==ERROR_ACCESS_DENIED);
 CHECK(control_call(t.root,control_request({"status"})).at("ok")==true);
}
TEST("native offline validation preserves legacy file and secrets are never retrieved"){
 Workspace t;auto doc=control_defaults();doc.erase("operator");atomic_write(t.root/"worker.json",json::serialize(doc));
 {Store store(t.root/"journal.sqlite3");store.set("priority","backup");}
 const auto before=read_file(t.root/"worker.json");
 THROWS(offline_control(t.root,control_request({"config","set","--stdin"},"{\"operator.priority\":\"low\",\"limits.max_jobs\":0}")));
 CHECK(read_file(t.root/"worker.json")==before);
 auto listed=offline_control(t.root,control_request({"config","list"}));CHECK(listed.at("live")==false);for(const auto& r:listed.at("settings").as_array())CHECK(r.as_object().at("effective").is_null());
 CHECK(read_file(t.root/"worker.json")==before);
 auto valid=offline_control(t.root,control_request({"config","set","--stdin"},"{\"operator.priority\":\"low\",\"limits.cpu_threads\":4}"));CHECK(valid.at("ok")==true);CHECK(load_config(t.root).budget.cpu_threads==4);
 THROWS(offline_control(t.root,control_request({"resume"})));
 THROWS(offline_control(t.root,control_request({"config","set","--stdin"},"{\"coordinator_url\":\"https://another.test\"}")));
 atomic_write(t.root/"credential.dpapi","SENTINEL_PRIVATE_TOKEN_NOT_TO_RETRIEVE");auto auth=offline_control(t.root,control_request({"auth"}));CHECK(auth.at("credential_present")==true);CHECK(json::serialize(auth).find("SENTINEL")==std::string::npos);
}
TEST("native bounded replies hold complete maximum-size configuration metadata"){
 Workspace t;ControlPipe pipe(t.root,[](const json::object& r){return json::object{{"schema",1},{"ok",true},{"exit_code",0},{"padding",std::string(text(r,"command")=="status"?300*1024:control_frame_limit,'x')}};});
 auto reply=control_call(t.root,control_request({"status"}));CHECK(reply.at("ok")==true);CHECK(reply.at("padding").as_string().size()==300*1024);
 auto oversized=control_call(t.root,control_request({"auth"}));CHECK(oversized.at("ok")==false);CHECK(oversized.at("exit_code")==5);
}
TEST("native idle control client times out without blocking later authenticated callers"){
 Workspace t;ControlPipe pipe(t.root,[](const json::object&){return json::object{{"schema",1},{"ok",true},{"exit_code",0}};});
 const auto name=control_pipe_name(t.root);require(WaitNamedPipeW(name.c_str(),2000),"Wait for idle-client test");Handle idle(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));CHECK(idle);Sleep(2300);idle.reset();
 CHECK(control_call(t.root,control_request({"status"})).at("ok")==true);
}
TEST("native headless bootstrap fails closed without owner identity and never creates pairing or journal"){
 Workspace t;const auto before=read_file(t.root/"worker.json");CHECK(run_headless(t.root)==4);CHECK(read_file(t.root/"worker.json")==before);CHECK(!std::filesystem::exists(t.root/"credential.dpapi"));CHECK(!std::filesystem::exists(t.root/"pairing-key.dpapi"));CHECK(!std::filesystem::exists(t.root/"journal.sqlite3"));
}
TEST("native unattended state refuses broad write grants and absent keys never create an identity"){
 Workspace t;const auto key=t.root/"absent-key.dpapi";THROWS(existing_public_pairing_key(key));CHECK(!std::filesystem::exists(key));
 auto sddl=L"D:P(A;OICI;FA;;;"+wide(process_owner_sid(GetCurrentProcess()))+L")(A;OICI;FA;;;WD)";PSECURITY_DESCRIPTOR sd{};require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Fixture DACL");BOOL ok=SetFileSecurityW(t.root.c_str(),DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,sd);LocalFree(sd);CHECK(ok);THROWS(verify_owner_state_path(t.root));
}
TEST("native unavailable control is bounded and never creates worker state"){
 Workspace t;auto start=monotonic_ms();THROWS(control_call(t.root,control_request({"resume"})));CHECK(monotonic_ms()-start<4000);CHECK(!std::filesystem::exists(t.root/"journal.sqlite3"));
}

TEST("native restart fences admission but preserves durable desired pause across replacement"){
 Workspace t;
 {Worker worker(load_config(t.root));ControlPipe pipe(t.root,[&](const json::object& r){return worker.control(r);});
  auto r=control_call(t.root,json::object{{"schema",1},{"command","restart"}});CHECK(r.at("ok")==true);CHECK(r.at("exiting")==true);CHECK(r.at("paused")==true);
  auto doc=parse(read_file(t.root/"worker.json")).as_object();CHECK(doc.at("operator").as_object().at("paused")==false);
  CHECK(control_call(t.root,json::object{{"schema",1},{"command","restart"}}).at("ok")==true);
  CHECK(worker.ready_to_exit());
 }
 Worker replacement(load_config(t.root));CHECK(!replacement.paused());
}
TEST("native restart refuses to exit before matching durable result acknowledgment"){
 Workspace t;Worker worker(load_config(t.root));Store journal(t.root/"journal.sqlite3");CHECK(journal.reserve("restart-attempt","restart-job","synthetic-fingerprint"));CHECK(journal.start("restart-attempt"));journal.result("restart-attempt","synthetic-result");
 auto r=worker.control(json::object{{"schema",1},{"command","restart"}});CHECK(r.at("exiting")==true);CHECK(!worker.ready_to_exit());
 auto p=journal.pending().front();CHECK(!journal.acknowledge(p.sequence,"wrong-attempt"));CHECK(!worker.ready_to_exit());CHECK(journal.acknowledge(p.sequence,p.attempt_id));CHECK(worker.ready_to_exit());
}
