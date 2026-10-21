#include "worker.hpp"
#include "pairing_adapter.hpp"
#include "control_pipe.hpp"
#include <algorithm>
namespace ow::win {
namespace {
json::object response(){return {{"schema",control_schema},{"ok",true},{"exit_code",0},{"observed_at_utc_ms",utc_ms()}};}
json::object config_result(ControlSettings& settings,const json::object& request,const json::object& effective,bool live){
 auto out=response();out["live"]=live;auto rows=settings.describe(effective,live);
 if(text(request,"action",16)=="get"){auto name=text(request,"name",64);json::array selected;for(auto& row:rows)if(text(row.as_object(),"name",64)==name)selected.emplace_back(row);if(selected.empty())throw std::runtime_error("Unknown setting");rows=std::move(selected);}
 out["settings"]=std::move(rows);return out;
}
json::object reset_values(const json::object& request){auto defaults=control_defaults();ControlSettings temporary(defaults,[](const json::object&){});std::vector<std::string> names;for(const auto& name:request.at("names").as_array())names.emplace_back(name.as_string());temporary.reset(names);json::object values;for(const auto& row:temporary.describe(defaults,false)){const auto& r=row.as_object();auto name=text(r,"name",64);if(std::find(names.begin(),names.end(),name)!=names.end())values[name]=r.at("current");}return values;}
json::object auth_status(const std::filesystem::path& root){auto out=response();out["credential_present"]=std::filesystem::is_regular_file(root/"credential.dpapi");out["recovery"]= "Use source setup.cmd -RePair after an explicit drain/stop. No token retrieval or replacement over IPC.";return out;}
}
json::object load_control_document(const std::filesystem::path& root,Store& store,bool persist_migration){
 auto doc=parse(read_file(root/"worker.json")).as_object();const bool migrate=!doc.contains("operator");
 if(migrate){auto op=control_defaults().at("operator").as_object();for(const auto& key:{"priority","video_preference","image_preference"})if(auto v=store.get(key))op[key]=*v;op["paused"]=store.get("paused").value_or("false")=="true";doc["operator"]=std::move(op);}
 doc=validate_control_config(std::move(doc));if(migrate&&persist_migration)atomic_write(root/"worker.json",json::serialize(doc));return doc;
}
json::object Worker::control(const json::object& request){
 std::lock_guard lock(control_mutex_);if(number(request,"schema",1)!=1)throw std::runtime_error("Control schema mismatch");const auto command=text(request,"command",16);
 if(command=="pause"||command=="drain")pause(true);else if(command=="resume")pause(false);
 else if(command=="exit"){auto mode=text(request,"mode",8);if(mode!="drain"&&mode!="stop")throw std::runtime_error("Explicit exit mode required");exit(mode=="stop");}
 else if(command=="restart")restart();
 else if(command=="console"){++viewer_generation_;auto out=response();out["requested"]=true;out["viewer_required"]=config_.headless;return out;}
 else if(command=="config"){
  auto action=text(request,"action",16);if(action=="set")update_settings(request.at("values").as_object());else if(action=="reset")update_settings(reset_values(request));else if(action!="list"&&action!="get")throw std::runtime_error("Unknown config operation");
  auto effective=effective_config_;effective["operator"]=json::object{{"priority",priority_==Priority::maximum?"maximum":priority_==Priority::normal?"normal":priority_==Priority::low?"low":"backup"},{"video_preference",video_==Preference::preferred?"preferred":video_==Preference::allowed?"allowed":"disabled"},{"image_preference",image_==Preference::preferred?"preferred":image_==Preference::allowed?"allowed":"disabled"},{"paused",paused_.load()}};
  return config_result(*settings_,request,effective,true);
 }else if(command=="auth"){auto out=auth_status(config_.root);std::lock_guard state_lock(mutex_);const bool pending=!pairing_user_code_.empty()&&utc_ms()<pairing_expires_utc_;out["pairing_user_code"]=pending?json::value(pairing_user_code_):json::value(nullptr);out["pairing_expires_at_utc_ms"]=pending?json::value(pairing_expires_utc_):json::value(nullptr);return out;}
 else if(command=="logs"){auto out=response();out["directory"]=utf8(config_.root.wstring());out["log"]=utf8((config_.root/"worker.log").wstring());return out;}
 else if(command=="history"){auto out=response();out["history"]=read_file(config_.root/"worker-history.txt",32768);return out;}
 else if(command!="status")throw std::runtime_error("Unknown control command");
 auto out=response();out["live"]=true;out["agent_version"]="0.1.2";out["build_revision"]=OW_BUILD_REVISION;out["pid"]=GetCurrentProcessId();out["owner_sid"]=process_owner_sid(GetCurrentProcess());out["process_uptime_ms"]=monotonic_ms()-process_started_;out["connected"]=report_connected_.load();out["paused"]=paused_.load();out["draining"]=exit_requested_.load()||(paused_&&active_count()>0);out["exiting"]=exit_requested_.load();
 auto gate=admission_.start_gate();const auto pending=store_.pending_count();const bool accepting=report_connected_&&!exit_requested_&&gate.accepted&&pending<512;
 out["headless"]=config_.headless;out["restart_requested"]=restart_requested_.load();out["viewer_generation"]=viewer_generation_.load();out["identity_ready"]=config_.headless;
 out["process_started_at_utc_ms"]=process_started_utc_;const auto contact=last_contact_utc_.load();out["last_coordinator_contact_at_utc_ms"]=contact?json::value(contact):json::value(nullptr);
 json::array profiles;{std::lock_guard state_lock(mutex_);const auto ready=capability_report_.processing_ready();out["processing_ready"]=ready?json::value(*ready):json::value(nullptr);for(const auto& p:hardware_profiles_)profiles.emplace_back(json::object{{"device_id",p.stable_id},{"device_name",p.device_name},{"physical_id",p.physical_id},{"backend",adaptive::backend_token(p.backend)},{"backend_index",p.backend_index},{"codec",p.codec==adaptive::Codec::h264?"h264":"hevc"},{"sampled",true},{"vram_mb",p.vram_mb},{"proven_sessions",p.proven_sessions}});}out["profiles"]=std::move(profiles);
 out["accepting"]=accepting;out["admission_reason"]=!report_connected_?"coordinator_offline":exit_requested_?"exiting":pending>=512?"result_backlog":gate.reason;out["heartbeat"]=heartbeat();out["pending_results"]=pending;
 // Free-form errors can contain server-supplied text: expose only structured safe state.
 out["state"]=exit_requested_?"exiting":paused_?"paused":accepting?"accepting":report_connected_?"not_accepting":"offline";
 return out;
}
json::object offline_control(const std::filesystem::path& root,const json::object& request){
 const auto command=text(request,"command",16);if(command=="auth")return auth_status(root);
 if(command=="logs"){auto out=response();out["directory"]=utf8(root.wstring());return out;}
 if(command!="config")throw std::runtime_error("Offline mode cannot control a worker or claim live status");
 Store store(root/"journal.sqlite3");auto doc=load_control_document(root,store,false);ControlSettings settings(doc,[&](const json::object& next){try{atomic_write(root/"worker.json",json::serialize(next));}catch(...){throw ControlFailure(5,"Atomic settings persistence failed");}});
 auto action=text(request,"action",16);if(action=="set"||action=="reset"){
  auto values=action=="set"?request.at("values").as_object():reset_values(request);
  settings.patch(values);
 }else if(action!="list"&&action!="get")throw std::runtime_error("Unknown config operation");return config_result(settings,request,doc,false);
}
}
