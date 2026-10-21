#include "test.hpp"
#include "ow/control.hpp"
#include "ow/admission.hpp"
#include <thread>
using namespace ow;
TEST("explicit remote resume opens admission without toggling and pause blocks reserved starts") {
 Admission admission({2048,0,8192,2},4);admission.capability("conversion.video.h264",true,Preference::allowed);
 auto doc=control_defaults();doc["operator"].as_object()["paused"]=true;
 std::string durable=json::serialize(doc);
 ControlSettings settings(doc,[&](const json::object& next){durable=json::serialize(next);});
 admission.pause(true);
 auto resume=control_request({"resume"});
 CHECK(text(resume,"command")=="resume");
 for(int i=0;i<2;++i){settings.patch({{"operator.paused",false}});admission.pause(settings.current().at("operator").as_object().at("paused").as_bool());}
 CHECK(!settings.current().at("operator").as_object().at("paused").as_bool());
 CHECK(parse(durable).as_object()==settings.current());
 CHECK(text(control_request({"drain"}),"command")=="drain");
 Offer a{"job-a","attempt-a","conversion.video.h264","fp-a",{512,0,128,1},10000};
 Offer b{"job-b","attempt-b","conversion.video.h264","fp-b",{512,0,128,1},10000};
 CHECK(admission.offer(a,0,{2048,0,8192,2}).accepted);CHECK(admission.begin(a.attempt_id,"lease-a",9000,1)==Start::started);
 CHECK(admission.offer(b,0,{2048,0,8192,2}).accepted);
 settings.patch({{"operator.paused",true}});admission.pause(true);CHECK(!admission.start_gate().accepted);
 CHECK(admission.begin(b.attempt_id,"lease-b",9000,1)==Start::declined);
 CHECK(admission.active().front().phase!=Phase::cancelling);CHECK(admission.used().ram_mb==1024);
 CHECK(admission.begin(a.attempt_id,"lease-a",9000,1)==Start::already_running);
 CHECK(admission.renew(a.attempt_id,"lease-a",1,9500,2));
 settings.patch({{"operator.paused",false}});admission.pause(false);
 CHECK(admission.begin(b.attempt_id,"lease-b",9000,3)==Start::started);
 CHECK(admission.cancel(a.attempt_id));CHECK(admission.used().ram_mb==1024);
 CHECK(admission.finish(a.attempt_id));CHECK(admission.used().ram_mb==512);
}
TEST("multi setting validation and failed persistence never partially publish") {
 auto original=control_defaults();int writes=0;
 ControlSettings settings(original,[&](const json::object&){++writes;});
 THROWS(settings.patch({{"limits.cpu_threads",4},{"limits.max_jobs",0}}));
 CHECK(writes==0);CHECK(settings.current()==original);
 ControlSettings failing(original,[](const json::object&){throw std::runtime_error("disk full");});
 THROWS(failing.patch({{"operator.priority","low"},{"limits.cpu_threads",4}}));CHECK(failing.current()==original);
}
TEST("every configurable setting validates typed values and defaults are resettable") {
 auto doc=control_defaults();doc["coordinator_url"]="https://example.test";ControlSettings settings(doc,[](const json::object&){});
 settings.patch({{"coordinator_url","https://example.test/"},{"storage_hosts",json::array{"storage.example.test"}},{"ffmpeg_path","C:\\approved\\ffmpeg.exe"},{"ffmpeg_sha256",std::string(64,'a')},{"limits.ram_mb",4096},{"limits.reserve_ram_mb",512},{"limits.scratch_mb",16384},{"limits.cpu_threads",4},{"limits.max_jobs",8},{"limits.vram_mb",0},{"operator.priority","backup"},{"operator.video_preference","preferred"},{"operator.image_preference","disabled"},{"operator.paused",true}});
 CHECK(text(settings.current(),"coordinator_url")=="https://example.test");
 CHECK(settings.describe(doc,true).size()==14);
 settings.reset({"operator.priority","limits.vram_mb"});
 CHECK(settings.current().at("operator").as_object().at("priority")=="normal");
 CHECK(!settings.current().at("limits").as_object().contains("vram_mb"));
}
TEST("unsafe fields values endpoints and command arguments fail closed") {
 ControlSettings settings(control_defaults(),[](const json::object&){});
 for(const auto& key:{"../credential.dpapi","root","schema","limits","command","token","operator.unknown"})THROWS(settings.patch({{key,"bad"}}));
 for(const auto& url:{"http://example.test","https://user:secret@example.test","https://example.test/path","https://example.test:444"})THROWS(settings.patch({{"coordinator_url",url}}));
 THROWS(settings.patch({{"limits.cpu_threads",1.5}}));THROWS(settings.patch({{"limits.ram_mb","4096"}}));THROWS(settings.patch({{"limits.cpu_threads",65}}));
 THROWS(settings.patch({{"operator.paused","false"}}));THROWS(settings.patch({{"operator.priority","realtime"}}));
 THROWS(settings.patch({{"storage_hosts",json::array{"*.example.test"}}}));
 THROWS(settings.patch({{"ffmpeg_path","..\\evil.exe"},{"ffmpeg_sha256",std::string(64,'a')}}));
 THROWS(control_request({"toggle"}));THROWS(control_request({"resume","extra"}));THROWS(control_request({"config","set","token=secret"}));
 THROWS(control_request({"exit"}));THROWS(control_request({"exit","anything"}));
 auto request=control_request({"status"});CHECK(validate_control_request(request)==request);request["path"]="../worker.json";THROWS(validate_control_request(request));
 THROWS(validate_control_request(json::object{{"schema",1},{"command","config"},{"action","get"},{"name","../credential.dpapi"}}));
}
TEST("coordinator scope changes cannot bypass setup archival or partially change preferences") {
 auto doc=control_defaults();doc["coordinator_url"]="https://example.test";
 ControlSettings settings(doc,[](const json::object&){});
 settings.patch({{"coordinator_url","https://EXAMPLE.test:443/"}});CHECK(settings.current()==doc);
 THROWS(settings.patch({{"coordinator_url","https://another.test"},{"operator.priority","low"}}));CHECK(settings.current()==doc);
 THROWS(settings.reset({"coordinator_url","operator.priority"}));CHECK(settings.current()==doc);
}
TEST("all documented local commands survive JSON wire roundtrip with strict schema") {
 const std::vector<std::vector<std::string>> commands{{"status"},{"pause"},{"drain"},{"resume"},{"auth"},{"console"},{"logs"},{"history"},{"exit","drain"},{"exit","stop"},{"config","list"},{"config","get","limits.vram_mb"},{"config","reset","operator.priority","limits.vram_mb"},{"config","set","--stdin"}};
 for(const auto& args:commands){auto request=control_request(args,"{\"operator.priority\":\"low\"}");auto wire=parse(json::serialize(request)).as_object();CHECK(validate_control_request(wire)==request);}
 CHECK(control_help().find("resume")!=std::string::npos);CHECK(control_help().find("--stdin")!=std::string::npos);
}
TEST("metadata separates desired effective and restart without claiming offline state") {
 auto original=control_defaults();ControlSettings settings(original,[](const json::object&){});
 settings.patch({{"limits.cpu_threads",4},{"operator.priority","low"}});
 auto rows=settings.describe(original,true);bool cold=false,hot=false;
 for(const auto& row:rows){const auto& r=row.as_object();if(r.at("name")=="limits.cpu_threads"){cold=true;CHECK(r.at("restart_required")==true);CHECK(r.at("effective")==2);}if(r.at("name")=="operator.priority"){hot=true;CHECK(r.at("restart_required")==false);}}
 CHECK(cold&&hot);for(const auto& row:settings.describe(original,false))CHECK(row.as_object().at("effective").is_null());
}
TEST("serialized independent writers preserve unrelated settings and hot roundtrip") {
 ControlSettings settings(control_defaults(),[](const json::object&){});
 std::thread a([&]{for(int i=0;i<40;++i)settings.patch({{"operator.priority","low"}});});
 std::thread b([&]{for(int i=0;i<40;++i)settings.patch({{"operator.image_preference","disabled"}});});a.join();b.join();
 CHECK(settings.current().at("operator").as_object().at("priority")=="low");CHECK(settings.current().at("operator").as_object().at("image_preference")=="disabled");
}
