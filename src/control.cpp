#include "ow/control.hpp"
#include "ow/pairing.hpp"
#include "ow/protocol.hpp"
#include <algorithm>
#include <array>
namespace ow {
namespace {
struct Setting {const char* name;const char* type;bool hot;};
constexpr std::array settings{
 Setting{"coordinator_url","https-origin",false},Setting{"storage_hosts","exact-host-array",false},
 Setting{"ffmpeg_path","absolute-path",false},Setting{"ffmpeg_sha256","sha256",false},
 Setting{"limits.ram_mb","integer:256..1048576",false},Setting{"limits.reserve_ram_mb","integer:0..1048576",false},
 Setting{"limits.scratch_mb","integer:1..1048576",false},Setting{"limits.cpu_threads","integer:1..64",false},
 Setting{"limits.max_jobs","integer:1..64",false},Setting{"limits.vram_mb","optional-integer;null=auto;0=off",false},
 Setting{"operator.priority","maximum|normal|low|backup",true},Setting{"operator.video_preference","preferred|allowed|disabled",true},
 Setting{"operator.image_preference","preferred|allowed|disabled",true},Setting{"operator.paused","boolean",true}};
const Setting& known(const std::string& name){for(const auto& s:settings)if(name==s.name)return s;throw std::runtime_error("Unknown setting (use config list)");}
json::value get(const json::object& doc,const std::string& name){auto dot=name.find('.');const auto& obj=dot==std::string::npos?doc:doc.at(name.substr(0,dot)).as_object();auto key=dot==std::string::npos?name:name.substr(dot+1);auto* v=obj.if_contains(key);return v?*v:json::value(nullptr);}
void put(json::object& doc,const std::string& name,const json::value& v){known(name);auto dot=name.find('.');auto& obj=dot==std::string::npos?doc:doc.at(name.substr(0,dot)).as_object();auto key=dot==std::string::npos?name:name.substr(dot+1);if(name=="limits.vram_mb"&&v.is_null())obj.erase(key);else obj[key]=v;}
void choice(const json::object& o,const char* key,std::initializer_list<const char*> choices){auto s=text(o,key,32);if(std::none_of(choices.begin(),choices.end(),[&](auto c){return s==c;}))throw std::runtime_error("Invalid operator setting");}
}
json::object control_defaults(){return {{"schema",1},{"coordinator_url",""},{"storage_hosts",json::array{}},{"ffmpeg_path",""},{"ffmpeg_sha256",""},{"limits",{{"ram_mb",2048},{"reserve_ram_mb",0},{"scratch_mb",8192},{"cpu_threads",2},{"max_jobs",4}}},{"operator",{{"priority","normal"},{"video_preference","allowed"},{"image_preference","allowed"},{"paused",false}}}};}
json::object validate_control_config(json::object doc){
 if(number(doc,"schema",1)!=1)throw std::runtime_error("Unsupported configuration schema");
 if(!doc.contains("operator"))doc["operator"]=control_defaults().at("operator");
 auto endpoint=text(doc,"coordinator_url",1024);if(!endpoint.empty())doc["coordinator_url"]=coordinator_origin(endpoint);
 auto& hosts=doc.at("storage_hosts").as_array();if(hosts.size()>128)throw std::runtime_error("Too many storage hosts");
 for(auto& v:hosts){if(!v.is_string())throw std::runtime_error("Storage host must be a string");std::string h(v.as_string());std::transform(h.begin(),h.end(),h.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(h.find('\0')!=std::string::npos||!allowed_url("https://"+h+"/",{h}))throw std::runtime_error("Invalid exact storage host");v=h;}
 auto path=text(doc,"ffmpeg_path",32700),hash=text(doc,"ffmpeg_sha256",64);
 if(!path.empty()){
  // Only a local absolute Windows path. Network shares/device namespaces are not executable approvals.
  if(path.size()<4||!((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))||path[1]!=':'||(path[2]!='\\'&&path[2]!='/')||path.find("..")!=std::string::npos||path.find(':',2)!=std::string::npos||std::any_of(path.begin(),path.end(),[](unsigned char c){return c<32;}))throw std::runtime_error("Converter requires an absolute local drive path");
  if(hash.size()!=64||!std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))throw std::runtime_error("Converter requires an approved lowercase SHA-256");
 }else if(!hash.empty())throw std::runtime_error("Converter hash requires a path");
 const auto& l=doc.at("limits").as_object();if(number(l,"ram_mb",1048576)<256||!number(l,"scratch_mb",1048576)||!number(l,"cpu_threads",64)||!number(l,"max_jobs",64))throw std::runtime_error("Invalid resource budget");number(l,"reserve_ram_mb",1048576);if(l.contains("vram_mb"))number(l,"vram_mb",UINT64_MAX);
 const auto& op=doc.at("operator").as_object();choice(op,"priority",{"maximum","normal","low","backup"});choice(op,"video_preference",{"preferred","allowed","disabled"});choice(op,"image_preference",{"preferred","allowed","disabled"});if(!op.at("paused").is_bool())throw std::runtime_error("Pause must be a boolean");return doc;
}
ControlSettings::ControlSettings(json::object doc,std::function<void(const json::object&)> persist):current_(validate_control_config(std::move(doc))),persist_(std::move(persist)){}
json::object ControlSettings::current() const{std::lock_guard lock(mutex_);return current_;}
json::object ControlSettings::patch(const json::object& values){std::lock_guard lock(mutex_);auto next=current_;if(values.empty())throw std::runtime_error("Empty update");for(const auto& [key,v]:values)put(next,std::string(key),v);next=validate_control_config(std::move(next));if(next.at("coordinator_url")!=current_.at("coordinator_url"))throw std::runtime_error("Coordinator scope migration requires setup.cmd -RePair");persist_(next);current_=next;return current_;}
json::object ControlSettings::reset(const std::vector<std::string>& names){if(names.empty())throw std::runtime_error("Name settings explicitly to reset");json::object values;auto defaults=control_defaults();for(const auto& n:names){known(n);values[n]=get(defaults,n);}return patch(values);}
json::array ControlSettings::describe(const json::object& effective,bool live) const{std::lock_guard lock(mutex_);json::array rows;const auto defaults=control_defaults();for(const auto& s:settings){auto value=get(current_,s.name);rows.emplace_back(json::object{{"name",s.name},{"type",s.type},{"default",get(defaults,s.name)},{"current",value},{"effective",live?get(effective,s.name):json::value(nullptr)},{"live",s.hot},{"restart_required",live&&!s.hot&&value!=get(effective,s.name)}});}return rows;}
json::object control_request(const std::vector<std::string>& a,const std::string& input){
 if(a.empty())throw std::runtime_error("Command required; use --help");
 json::object r{{"schema",control_schema},{"command",a[0]}};
 if(a[0]=="config"){
  if(a.size()<2)throw std::runtime_error("config requires list|get|set|reset");
  r["action"]=a[1];
  if(a[1]=="list"&&a.size()==2)return r;
  if(a[1]=="get"&&a.size()==3){known(a[2]);r["name"]=a[2];return r;}
  if(a[1]=="set"&&a.size()==3&&a[2]=="--stdin"){std::string_view bytes=input;if(bytes.starts_with("\xef\xbb\xbf"))bytes.remove_prefix(3);r["values"]=parse(bytes,64*1024).as_object();return r;}
  if(a[1]=="reset"&&a.size()>2){json::array names;for(std::size_t i=2;i<a.size();++i){known(a[i]);names.emplace_back(a[i]);}r["names"]=std::move(names);return r;}
 }else if(a[0]=="exit"&&a.size()==2&&(a[1]=="drain"||a[1]=="stop")){r["mode"]=a[1];return r;}
 else if(a.size()==1&&(a[0]=="status"||a[0]=="pause"||a[0]=="drain"||a[0]=="resume"||a[0]=="console"||a[0]=="logs"||a[0]=="history"||a[0]=="auth"))return r;
 throw std::runtime_error("Unknown command/arguments; use --help");
}
json::object validate_control_request(const json::object& r){
 if(number(r,"schema",1)!=1)throw std::runtime_error("Unsupported control schema");
 const auto cmd=text(r,"command",16);std::vector<std::string> args{cmd};
 if(cmd=="config"){
  auto action=text(r,"action",16);args.push_back(action);
  if(action=="get")args.push_back(text(r,"name",64));
  else if(action=="set")args.push_back("--stdin");
  else if(action=="reset")for(const auto& name:r.at("names").as_array()){if(!name.is_string())throw std::runtime_error("Invalid setting name");args.emplace_back(name.as_string());}
 }else if(cmd=="exit")args.push_back(text(r,"mode",8));
 const auto expected=control_request(args,cmd=="config"&&r.at("action")=="set"?json::serialize(r.at("values")):std::string{});
 if(expected!=r)throw std::runtime_error("Unknown control request fields");
 return expected;
}
std::string control_help(){return
 "OrganizerWorkerCtl [--json] [--offline] [--data-root LIVE_ROOT] COMMAND\n"
 "  status                     Fresh live state, jobs, connection and version\n"
 "  pause | drain | resume     Explicit idempotent admission controls\n"
 "  config list | get NAME     Types, defaults, current/effective/restart flags\n"
 "  config set --stdin         Atomic flat-name JSON object from stdin\n"
 "  config reset NAME...       Reset only explicitly named settings\n"
 "  exit drain | exit stop     Finish jobs or explicitly cancel then exit\n"
 "  auth                       Pairing presence/public approval code; never secrets\n"
 "  console | logs | history   Open viewer, locate logs, recent outcomes\n"
 "  --offline                  Config/auth/logs only; requires stopped worker\n"
 "Exit codes: 0 success, 2 usage/validation, 3 unavailable/timeout, 4 denied, 5 I/O.\n"
 "JSON local-control schema=1; no network listener. Credential recovery, endpoint\n"
 "migration and startup remain setup.cmd -RePair/-NoStartup/-NoLaunch; see docs/cli.md.\n";
}
}
