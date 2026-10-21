#include "offline_media.hpp"
#include "control_pipe.hpp"
#include <fstream>
#include <map>
namespace ow::win {
namespace {
void require_fixture(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void fixture_child(const Config& c,const std::filesystem::path& dir,const std::vector<std::wstring>& args){
 std::filesystem::create_directories(dir);Child child(c.ffmpeg,args,dir,{1024,0,4096,1},Priority::low);
 const auto until=monotonic_ms()+180000;
 for(;;){if(auto code=child.exit_code()){require_fixture(*code==0,"Synthetic converter child failed");return;}if(monotonic_ms()>until)throw std::runtime_error("Synthetic converter child timeout");Sleep(50);}
}
}
json::object OfflineMediaHarness::control_acceptance(Config config){
 require_fixture(!std::filesystem::exists(config.root),"Acceptance root must be new");
 require_fixture(config.ffmpeg.is_absolute()&&config.ffmpeg_sha256.size()==64&&sha256_file(config.ffmpeg)==config.ffmpeg_sha256,"Approved converter hash required");
 std::filesystem::create_directories(config.root);config.coordinator="https://fixture.invalid";config.storage_hosts={"fixture.invalid"};config.budget={2048,0,4096,4};config.vram_override_set=true;config.max_jobs=4;
 auto document=control_defaults();document["coordinator_url"]=config.coordinator;document["storage_hosts"]=json::array{"fixture.invalid"};document["ffmpeg_path"]=utf8(config.ffmpeg.wstring());document["ffmpeg_sha256"]=config.ffmpeg_sha256;
 document["limits"]=json::object{{"ram_mb",2048},{"reserve_ram_mb",0},{"scratch_mb",4096},{"cpu_threads",4},{"max_jobs",4},{"vram_mb",0}};document["operator"].as_object()["priority"]="low";
 atomic_write(config.root/"worker.json",json::serialize(document));
 Handle instance(CreateFileW((config.root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));require_fixture(static_cast<bool>(instance),"Acceptance instance lock failed");
 const auto probe=config.root/"probe";
 fixture_child(config,probe,{L"-nostdin",L"-v",L"error",L"-y",L"-f",L"lavfi",L"-i",L"testsrc2=size=640x360:rate=30",L"-frames:v",L"12",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",L"-f",L"mp4",(probe/"source.bin").wstring()});
 JobSpec sample;sample.offer.capability="conversion.video.h264";sample.offer.resources={512,0,256,1};fixture_child(config,probe,ffmpeg_arguments(sample,probe));
 require_fixture(std::filesystem::file_size(probe/"result.mp4")>0,"Typed diagnostic probe produced no output");
 fixture_child(config,probe,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(probe/"result.mp4").wstring(),L"-f",L"null",L"-"});
 const auto source=config.root/"synthetic-source.mp4";
 fixture_child(config,config.root/"generate",{L"-nostdin",L"-v",L"error",L"-y",L"-f",L"lavfi",L"-i",L"testsrc2=size=1280x720:rate=30",L"-frames:v",L"3000",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",source.wstring()});
 std::mutex children_mutex;std::map<std::string,Handle> children;
 config.observe_child=[&](const std::string& id,std::uint32_t pid){std::lock_guard lock(children_mutex);children.emplace(id,Handle(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)));};
 auto worker=std::unique_ptr<Worker>(new Worker(config,true));worker->offline_source_=source;worker->offline_output_=config.root/"delivered";
 worker->verified_video_=true;worker->capability_report_.probed(true,false,false,false,{});worker->apply_preferences();worker->report_connected_=true;worker->last_contact_utc_=utc_ms();
 worker->watchdog_=std::jthread([&]{worker->watchdog();});
 ControlPipe pipe(config.root,[&](const json::object& r){return worker->control(r);});
 const auto digest=sha256_file(source);const auto bytes=std::filesystem::file_size(source);std::map<std::string,json::object> results;std::map<std::string,unsigned> transmissions;json::object last_reply;
 auto send=[&](const std::string& body){auto message=parse(body).as_object();auto type=text(message,"type",32);if(type=="result"){auto id=text(message,"attempt_id",128);results[id]=message;++transmissions[id];}else last_reply=message;};
 auto refresh=[&]{worker->capacity_.observe(Pressure{0.0,0.0,false,0.0},monotonic_ms());};
 auto deliver=[&](const json::object& m){refresh();worker->message(send,parse(json::serialize(m)).as_object());};
 auto offer=[&](const std::string& id){deliver({{"protocol",1},{"type","offer"},{"job_id","job-"+id},{"attempt_id",id},{"capability","conversion.video.h264"},{"offer_ttl_ms",60000},{"timeout_ms",300000},{"resources",{{"ram_mb",512},{"vram_mb",0},{"scratch_mb",1024},{"cpu_threads",1}}},{"input",{{"url","https://fixture.invalid/input"},{"sha256",digest},{"bytes",bytes}}},{"output",{{"put_url","https://fixture.invalid/"+id},{"max_bytes",100*1024*1024}}}});require_fixture(last_reply.at("accepted").as_bool(),"Fake coordinator offer rejected");};
 auto lease=[&](const std::string& id,const char* type,std::uint64_t sequence=0){auto now=utc_ms();json::object m{{"protocol",1},{"type",type},{"attempt_id",id},{"lease_token","fixture-only-lease"},{"server_time_ms",now},{"lease_until_ms",now+30000}};if(sequence)m["sequence"]=sequence;deliver(m);};
 std::uint64_t renewal{};Tick next_renew{};const auto until=monotonic_ms()+600000;
 auto tick=[&]{refresh();worker->send_pending(send);if(monotonic_ms()>=next_renew){++renewal;lease("active-a","renew",renewal);lease("reserved-b","renew",renewal);next_renew=monotonic_ms()+1000;}require_fixture(monotonic_ms()<until,"Acceptance coordinator deadline expired");Sleep(50);};
 auto state=[&](const char* phase){atomic_write(config.root/"acceptance-state.json",json::serialize(json::object{{"fixture",true},{"real_coordinator",false},{"phase",phase},{"pid",GetCurrentProcessId()},{"session",[]{DWORD s{};ProcessIdToSessionId(GetCurrentProcessId(),&s);return s;}()},{"observed_at_utc_ms",utc_ms()}}));};
 auto wait=[&](const char* marker){while(!std::filesystem::exists(config.root/marker))tick();};
 refresh();worker->report_state("accepting","fake coordinator transport");state("ready");wait("start");
 offer("active-a");offer("reserved-b");lease("active-a","begin");require_fixture(last_reply.at("accepted").as_bool(),"BEGIN A rejected");
 for(;;){{std::lock_guard lock(children_mutex);if(children.contains("active-a"))break;}tick();}
 state("child-active");wait("paused");
 require_fixture(worker->paused(),"Owner CLI did not pause actual controller");
 {std::lock_guard lock(children_mutex);require_fixture(children.at("active-a")&&WaitForSingleObject(children.at("active-a").get(),0)==WAIT_TIMEOUT,"Pause proof requires a still-running real converter child");}
 lease("reserved-b","begin");require_fixture(!last_reply.at("accepted").as_bool(),"Paused reserved BEGIN was incorrectly accepted");
 require_fixture(worker->admission_.used().ram_mb>=1024,"Pause released running/reserved capacity");state("pause-verified");wait("resumed");
 require_fixture(!worker->paused(),"Owner CLI did not resume controller");lease("reserved-b","begin");require_fixture(last_reply.at("accepted").as_bool(),"Resumed reserved BEGIN rejected");
 while(!results.contains("active-a")||!results.contains("reserved-b")||transmissions["active-a"]<2||transmissions["reserved-b"]<2)tick();
 json::array outcomes;
 for(const auto& id:{"active-a","reserved-b"}){
  auto result=results.at(id);require_fixture(text(result,"status",32)=="succeeded","Actual admitted conversion failed");const auto output=config.root/"delivered"/id/"result.mp4";const auto& metadata=result.at("output").as_object();require_fixture(sha256_file(output)==text(metadata,"sha256",64)&&std::filesystem::file_size(output)==number(metadata,"bytes",UINT64_MAX),"Fake coordinator output validation failed");
  fixture_child(config,config.root/(std::string("coordinator-decode-")+id),{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",output.wstring(),L"-f",L"null",L"-"});
  const auto sequence=number(result,"outbox_sequence",INT64_MAX);const auto before=worker->store_.pending_count();deliver({{"protocol",1},{"type","result_ack"},{"attempt_id",id},{"outbox_sequence",sequence+1000}});require_fixture(worker->store_.pending_count()==before,"Wrong ACK removed durable result");
  deliver({{"protocol",1},{"type","result_ack"},{"attempt_id",id},{"outbox_sequence",sequence}});outcomes.emplace_back(json::object{{"attempt_id",id},{"status","succeeded"},{"matching_ack",true},{"transmissions",transmissions[id]},{"bytes",metadata.at("bytes")},{"sha256",metadata.at("sha256")}});
 }
 require_fixture(worker->store_.pending_count()==0,"Matching coordinator ACK did not empty outbox");while(worker->active_count())tick();
 json::object report{{"fixture",true},{"real_coordinator",false},{"network_tls_interop",false},{"actual_typed_children",true},{"typed_probe_encode_decode_passed",true},{"paused_child_survived",true},{"reserved_begin_rejected_while_paused",true},{"reserved_begin_accepted_after_resume",true},{"outbox_acknowledged",true},{"coordinator_decode_validated",true},{"jobs",std::move(outcomes)},{"build_revision",OW_BUILD_REVISION}};
 atomic_write(config.root/"acceptance-result.json",json::serialize(report));state("complete");while(!worker->ready_to_exit())tick();return report;
}
}
