#include <cctype>
#include "worker.hpp"
#include "pairing_adapter.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
namespace ow::win {
namespace {
std::string priority_name(Priority p){return p==Priority::maximum?"maximum":p==Priority::normal?"normal":p==Priority::low?"low":"backup";}
Priority parse_priority(const std::string& p){if(p=="maximum")return Priority::maximum;if(p=="normal")return Priority::normal;if(p=="low")return Priority::low;if(p=="backup")return Priority::backup;throw std::runtime_error("Unknown stored priority");}
std::string pref_name(Preference p){return p==Preference::preferred?"preferred":p==Preference::allowed?"allowed":"disabled";}
Preference parse_pref(const std::string& p){if(p=="preferred")return Preference::preferred;if(p=="allowed")return Preference::allowed;if(p=="disabled")return Preference::disabled;throw std::runtime_error("Unknown task preference");}
void sleep_until_stop(const std::atomic_bool& stop,unsigned milliseconds){for(unsigned elapsed=0;elapsed<milliseconds&&!stop;elapsed+=100)Sleep(std::min(100u,milliseconds-elapsed));}
void check_protocol(const Response& r){
 if(r.status==404)throw std::runtime_error("Coordinator worker endpoints are not deployed; no pairing performed");
 if(r.status!=200)throw std::runtime_error("Coordinator protocol check returned HTTP "+std::to_string(r.status));
 auto v=parse(r.body);if(number(v.as_object(),"protocol",1)!=1)throw std::runtime_error("Coordinator worker protocol is incompatible");
}
void run_probe(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& workspace,const std::function<bool()>& cancelled){
 Child child(exe,args,workspace,{512,0,128,1},Priority::low);auto until=monotonic_ms()+30000;
 for(;;){if(cancelled&&cancelled())throw std::runtime_error("Capability probe cancelled");if(auto code=child.exit_code()){if(*code)throw std::runtime_error("FFmpeg functional probe failed");break;}if(monotonic_ms()>=until)throw std::runtime_error("FFmpeg functional probe timed out");Sleep(100);}
}
}
Config load_config(const std::filesystem::path& root){
 Config c;c.root=root;auto obj=parse(read_file(root/"worker.json")).as_object();if(number(obj,"schema",1)!=1)throw std::runtime_error("Unsupported configuration version");c.coordinator=coordinator_origin(text(obj,"coordinator_url",1024));
 for(const auto& v:obj.at("storage_hosts").as_array()){auto s=std::string(v.as_string());std::transform(s.begin(),s.end(),s.begin(),[](unsigned char x){return static_cast<char>(std::tolower(x));});if(!allowed_url("https://"+s+"/",{s}))throw std::runtime_error("Invalid approved storage host");c.storage_hosts.push_back(s);}
 c.ffmpeg=wide(text(obj,"ffmpeg_path",32700));c.ffmpeg_sha256=text(obj,"ffmpeg_sha256",64);
 const auto& limits=obj.at("limits").as_object();c.budget={number(limits,"ram_mb",1024*1024),0,number(limits,"scratch_mb",1024*1024),number(limits,"cpu_threads",64)};c.reserve_ram_mb=number(limits,"reserve_ram_mb",1024*1024);c.max_jobs=static_cast<std::size_t>(number(limits,"max_jobs",64));
 if(c.budget.ram_mb<256||!c.budget.cpu_threads||!c.max_jobs||!c.budget.scratch_mb)throw std::runtime_error("Invalid resource budget");
 return c;
}
Probe probe_ffmpeg(const Config& c,const std::function<bool()>& cancelled){
 if(c.ffmpeg.empty())return {false,false,"FFmpeg not configured; conversion capabilities disabled"};
 Probe p;auto workspace=c.root/"probes"/("probe-"+random_hex(8));
 try{
 if(!c.ffmpeg.is_absolute()||!std::filesystem::is_regular_file(c.ffmpeg))throw std::runtime_error("Configured FFmpeg binary is missing");
 if(c.ffmpeg_sha256.size()!=64||sha256_file(c.ffmpeg)!=c.ffmpeg_sha256)throw std::runtime_error("FFmpeg SHA-256 differs from approved configuration");
 std::filesystem::create_directories(workspace);
 run_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",L"color=c=black:s=32x32:r=1",L"-t",L"1",L"-an",L"-c:v",L"libx264",L"-threads",L"1",L"-pix_fmt",L"yuv420p",(workspace/"sample.mp4").wstring()},workspace,cancelled);
 std::filesystem::copy_file(workspace/"sample.mp4",workspace/"source.bin");
 JobSpec spec;spec.offer.capability="conversion.video.h264";spec.offer.resources={512,0,128,1};run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.video=std::filesystem::file_size(workspace/"result.mp4")>0;
 spec.offer.capability="conversion.image.jpeg";run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.image=std::filesystem::file_size(workspace/"result.jpg")>0;
 p.reason="CPU conversion probes passed; GPU and inference are not advertised";
 }catch(const std::exception& e){p.reason=e.what();}
 std::error_code error;std::filesystem::remove_all(workspace,error);return p;
}
Worker::Worker(Config config):config_(std::move(config)),store_(config_.root/"journal.sqlite3"),log_(config_.root/"worker.log"),admission_(config_.budget,config_.max_jobs){
 std::optional<std::string> proven_origin;
 ProtectedPairingSecrets identity(config_.root);
 if(auto credential=identity.read(SecretSlot::credential)) {
  proven_origin=text(parse(*credential).as_object(),"coordinator_url",1024);
 }
 bind_journal_origin(store_,config_.coordinator,proven_origin);
 if(auto p=store_.get("priority"))priority_=parse_priority(*p);
 if(auto p=store_.get("video_preference"))video_=parse_pref(*p);
 if(auto p=store_.get("image_preference"))image_=parse_pref(*p);
 paused_=store_.get("paused").value_or("false")=="true";admission_.pause(paused_);
 for(const auto& id:store_.interrupted())store_.result(id,json::serialize(json::object{{"protocol",1},{"type","result"},{"attempt_id",id},{"status","abandoned"},{"reason","worker_restarted"}}));
 safe_log("Worker starting. Closing the console only closes the viewer; use the tray to exit.");
 // Slow capability probes run on the background thread, never blocking tray creation.
 network_=std::jthread([this]{network();});try{watchdog_=std::jthread([this]{watchdog();});}catch(...){stop_=true;network_.join();throw;}
}
Worker::~Worker(){stop_=true;{std::lock_guard lock(mutex_);for(auto& [_,r]:running_)r->data->cancel=true;}if(network_.joinable())network_.join();if(watchdog_.joinable())watchdog_.join();std::unordered_map<std::string,std::unique_ptr<Running>> remaining;{std::lock_guard lock(mutex_);remaining.swap(running_);}remaining.clear();}
void Worker::safe_log(const std::string& s) noexcept {try{log_.write(s);}catch(...){paused_=true;admission_.pause(true);}}
void Worker::state(std::string s){{std::lock_guard lock(mutex_);status_=s;}safe_log(s);}
std::string Worker::status() const{std::lock_guard lock(mutex_);return status_;}
std::size_t Worker::active_count() const{return admission_.active().size();}
void Worker::pause(bool p){if(exit_requested_&&!p)throw std::runtime_error("Worker is exiting; restart it to resume");paused_=p;admission_.pause(p);store_.set("paused",p?"true":"false");safe_log(p?"Paused: no new jobs will start":"Resumed");}
void Worker::priority(Priority p){priority_=p;store_.set("priority",priority_name(p));safe_log("Machine priority: "+priority_name(p));}
void Worker::apply_preferences(){admission_.capability("conversion.video.h264",verified_video_.load(),video_);admission_.capability("conversion.image.jpeg",verified_image_.load(),image_);}
void Worker::preference(bool video,Preference value){if(video)video_=value;else image_=value;store_.set(video?"video_preference":"image_preference",pref_name(value));apply_preferences();safe_log("Task preference changed; active jobs are not interrupted");}
void Worker::exit(bool kill){pause(true);exit_requested_=true;{std::lock_guard lock(mutex_);if(kill)for(auto& [_,r]:running_)r->data->cancel=true;}state(kill?"EXITING: stopping jobs; pending results remain in journal":"DRAINING: finish active jobs, then exit");}
bool Worker::ready_to_exit() const{if(!exit_requested_)return false;std::lock_guard lock(mutex_);return running_.empty();}
json::object Worker::heartbeat(){
 auto free=available_resources(config_.root,config_.reserve_ram_mb);auto used=admission_.used();json::array active;
 for(const auto& a:admission_.active())active.emplace_back(json::object{{"attempt_id",a.offer.attempt_id},{"state",a.phase==Phase::reserved?"reserved":a.phase==Phase::running?"running":"cancelling"}});
 return {{"protocol",1},{"type","heartbeat"},{"status",paused_?"paused":"ready"},{"priority",priority_name(priority_)},{"active",std::move(active)},
 {"available",{{"ram_mb",free.ram_mb},{"vram_mb",0},{"scratch_mb",free.scratch_mb},{"cpu_threads",free.cpu_threads}}},
 {"reserved",{{"ram_mb",used.ram_mb},{"scratch_mb",used.scratch_mb},{"cpu_threads",used.cpu_threads}}},
 {"capabilities",{{"conversion.video.h264",{{"verified",verified_video_.load()},{"preference",pref_name(video_)}}},{"conversion.image.jpeg",{{"verified",verified_image_.load()},{"preference",pref_name(image_)}}}}}};
}
std::string Worker::paired_token(){
 ProtectedPairingSecrets secrets(config_.root);
 NativePairingTransport transport;
 NativePairingKey key(config_.root);
 wchar_t name[256]{};DWORD length=256;
 require(GetComputerNameW(name,&length),"Read worker display name");
 auto already_paired=secrets.read(SecretSlot::credential).has_value();
 auto token=pairing_token(config_.coordinator,utf8(std::wstring(name,length)),secrets,transport,key,
  PairingEnvironment{[]{return utc_ms();},[this](unsigned ms){sleep_until_stop(stop_,ms);},
   [this]{return stop_.load()||exit_requested_.load();},
   [this](const std::string& code,unsigned seconds){
    state("PAIRING CODE: "+code+" | approve this exact code on Lightsail; expires in "+std::to_string(seconds)+" seconds");
   }});
 if(!already_paired)safe_log("PAIRING APPROVED: protected identity saved; connecting to coordinator");
 return token;
}
void Worker::network(){
 try{auto probe=probe_ffmpeg(config_,[this]{return stop_||exit_requested_;});verified_video_=probe.video;verified_image_=probe.image;safe_log(probe.reason);apply_preferences();}catch(const std::exception& e){state(e.what());}
 if(config_.coordinator.empty()){state("UNCONFIGURED: set coordinator_url after Lightsail worker endpoints are deployed");while(!stop_)sleep_until_stop(stop_,500);return;}
 unsigned backoff=1000;
 while(!stop_){
 try{
  auto token=paired_token();if(stop_||exit_requested_)return;Http http;check_protocol(http.request("GET",config_.coordinator+"/v1/worker/protocol"));WebSocket socket(config_.coordinator+"/v1/worker/connect",token);SecureZeroMemory(token.data(),token.size());
  socket.send(json::serialize(json::object{{"protocol",1},{"type","hello"},{"agent_version","0.1.2"},{"state",heartbeat()}}));state("CONNECTED: waiting for work");backoff=1000;Tick next_heartbeat=0,next_results=0;
  while(!stop_){
   auto now=monotonic_ms();if(now>=next_heartbeat){
    // Specs for expired unstarted offers are not an unbounded in-memory queue.
    for(auto it=offered_.begin();it!=offered_.end();){if(it->second.offer.reserve_until<=now)it=offered_.erase(it);else ++it;}
    socket.send(json::serialize(heartbeat()));next_heartbeat=now+5000;
   }
   if(now>=next_results){for(const auto& p:store_.pending()){auto body=parse(p.body).as_object();body["outbox_sequence"]=p.sequence;socket.send(json::serialize(body));}next_results=now+2000;}
   if(auto incoming=socket.receive()){auto value=parse(*incoming);message(socket,value.as_object());}
  }
 }catch(const PairingActionRequired& e){
  pause(true);state(std::string("ACTION REQUIRED: ")+e.what());
  while(!stop_&&!exit_requested_)sleep_until_stop(stop_,500);
  return;
 }catch(const std::exception& e){
  state(std::string("OFFLINE: ")+e.what());
  // Never bypass an explicit revocation by silently starting a new pairing.
  auto reason=std::string(e.what());if(reason.find("HTTP 401")!=std::string::npos||reason.find("HTTP 403")!=std::string::npos){pause(true);state("REVOKED: exit worker and explicitly re-pair after administrator approval");while(!stop_)sleep_until_stop(stop_,500);return;}
  sleep_until_stop(stop_,backoff+static_cast<unsigned>(GetTickCount64()%997));backoff=std::min(30000u,backoff*2);
 }
 }
}
void Worker::message(WebSocket& socket,const json::object& m){
 if(number(m,"protocol",1)!=1)throw std::runtime_error("Unsupported coordinator protocol");auto type=text(m,"type",32);
 if(type=="ping"){socket.send(json::serialize(heartbeat()));return;}
 if(type=="result_ack"){auto id=text(m,"attempt_id",128);auto seq=number(m,"outbox_sequence",INT64_MAX);if(store_.acknowledge(static_cast<std::int64_t>(seq),id))safe_log("RESULT ACKNOWLEDGED "+id);return;}
 if(type=="offer"){
  std::string id;try{id=text(m,"attempt_id",128);auto spec=decode_offer(m,monotonic_ms(),config_.storage_hosts);
   if(store_.pending_count()>=512)throw std::runtime_error("Unacknowledged result backlog is full");
   if(auto s=store_.state(id);s&&*s!="reserved"&&*s!="running"){socket.send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",false},{"reason","attempt_already_recorded"}}));return;}
   auto decision=admission_.offer(spec.offer,monotonic_ms(),available_resources(config_.root,config_.reserve_ram_mb));
   if(decision.accepted&&decision.reason!="duplicate"){
    try{if(!store_.reserve(id,spec.offer.job_id,spec.offer.fingerprint))throw std::runtime_error("journal attempt conflict");offered_.insert_or_assign(id,spec);}catch(...){admission_.cancel(id);throw;}
   }
   socket.send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",decision.accepted},{"reason",decision.reason}}));
  }catch(const std::exception&){if(safe_id(id))socket.send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",false},{"reason","invalid_or_unavailable"}}));safe_log("Rejected offer: validation, dependency or journal check failed");}return;
 }
 auto id=text(m,"attempt_id",128);if(!safe_id(id))throw std::runtime_error("Invalid attempt identity");
 if(type=="begin"||type=="renew"){
  auto token=text(m,"lease_token",512);auto server_time=number(m,"server_time_ms",INT64_MAX),expires=number(m,"lease_until_ms",INT64_MAX);auto local=utc_ms();
  auto deadline=lease_deadline(static_cast<Tick>(server_time),static_cast<Tick>(expires),local,monotonic_ms());
  if(type=="renew"){admission_.renew(id,token,number(m,"sequence",UINT64_MAX),deadline,monotonic_ms());return;}
  auto started=admission_.begin(id,token,deadline,monotonic_ms());
  if(started==Start::started){
   auto i=offered_.find(id);if(i==offered_.end()||!store_.start(id)){admission_.finish(id);throw std::runtime_error("No persisted spec for lease");}
   auto data=std::make_shared<TaskData>();data->spec=i->second;data->token=token;data->hard_deadline=monotonic_ms()+static_cast<Tick>(data->spec.timeout_ms);offered_.erase(i);
   auto running=std::make_unique<Running>();running->data=data;{std::lock_guard lock(mutex_);running->thread=std::jthread([this,data]{execute(data);});running_.emplace(id,std::move(running));}
   safe_log("START "+id+" "+data->spec.offer.capability);
  }
  socket.send(json::serialize(json::object{{"protocol",1},{"type","begin_reply"},{"attempt_id",id},{"accepted",started!=Start::declined}}));return;
 }
 if(type=="cancel"){
  auto token=text(m,"lease_token",512);for(const auto& a:admission_.active())if(a.offer.attempt_id==id&&a.lease_token==token){admission_.cancel(id);std::lock_guard lock(mutex_);if(auto i=running_.find(id);i!=running_.end())i->second->data->cancel=true;}
  return;
 }
 throw std::runtime_error("Unknown coordinator message type");
}
void Worker::execute(std::shared_ptr<TaskData> data){
 const auto& s=data->spec;auto id=s.offer.attempt_id;auto workspace=config_.root/"jobs"/("job-"+id);json::object result{{"protocol",1},{"type","result"},{"attempt_id",id},{"job_id",s.offer.job_id},{"lease_token",data->token}};
 auto cancelled=[&]{return stop_||data->cancel||monotonic_ms()>=data->hard_deadline;};
 try{
  if(std::filesystem::exists(workspace))throw std::runtime_error("Attempt workspace already exists; refusing unsafe reuse");std::filesystem::create_directories(workspace);
  Http http;http.download(s.input_url,workspace/"source.bin",s.input_bytes,cancelled);if(sha256_file(workspace/"source.bin")!=s.input_sha256)throw std::runtime_error("Input integrity check failed");if(cancelled())throw std::runtime_error("Job cancelled before conversion");
  {
   auto current=priority_.load();Child child(config_.ffmpeg,ffmpeg_arguments(s,workspace),workspace,s.offer.resources,current);
   for(;;){if(cancelled())throw std::runtime_error("Job cancelled or timed out");if(auto code=child.exit_code()){if(*code)throw std::runtime_error("Conversion process failed; see per-job log");break;}
    auto latest=priority_.load();if(latest!=current){child.priority(latest);current=latest;}
    std::error_code error;auto size=std::filesystem::file_size(workspace/output_name(s),error);if(!error&&size>s.output_max_bytes)throw std::runtime_error("Output exceeded declared byte limit");
    auto log_bytes=std::filesystem::file_size(workspace/"process.log",error);if(!error&&log_bytes>5*1024*1024)throw std::runtime_error("Child output exceeded log budget");Sleep(100);
   }
  }
  auto output=workspace/output_name(s);auto bytes=std::filesystem::file_size(output);if(!bytes||bytes>s.output_max_bytes)throw std::runtime_error("Invalid output size");auto hash=sha256_file(output);auto mime=s.offer.capability=="conversion.video.h264"?"video/mp4":"image/jpeg";
  http.upload(s.output_url,output,mime,cancelled);result["status"]="succeeded";result["output"]={{"sha256",hash},{"bytes",bytes},{"content_type",mime}};
 }catch(const std::exception& e){result["status"]="failed";result["reason"]=std::string(e.what());safe_log("END "+id+": "+e.what());}
 try{store_.result(id,json::serialize(result));safe_log("RESULT PENDING ACK "+id);}catch(...){paused_=true;admission_.pause(true);safe_log("JOURNAL FAILURE: admissions paused; coordinator must recover unacknowledged attempt");}
 // Media is not needed after an attempt: authoritative output is in object
 // storage, and any accepted completion is durably represented in the outbox.
 std::error_code error;
 try{
  if(result.at("status").as_string()!="succeeded"&&std::filesystem::exists(workspace/"process.log")){
   auto logs=config_.root/"logs";std::filesystem::create_directories(logs);
   auto source=workspace/"process.log";auto length=std::filesystem::file_size(source);
   if(length<=5*1024*1024)std::filesystem::copy_file(source,logs/("job-"+id+".log"),std::filesystem::copy_options::overwrite_existing);
   // Only 20 bounded process logs are retained across completed jobs.
   std::vector<std::filesystem::directory_entry> entries;
   for(const auto& entry:std::filesystem::directory_iterator(logs))if(entry.is_regular_file()&&entry.path().extension()==".log")entries.push_back(entry);
   std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
   for(std::size_t i=0;i+20<entries.size();++i)std::filesystem::remove(entries[i].path(),error);
  }
 }catch(...){safe_log("Could not preserve child diagnostic log");}
 std::filesystem::remove_all(workspace,error);admission_.finish(id);data->done=true;
}
void Worker::watchdog(){
 while(!stop_){
 try{
  for(const auto& e:admission_.expire(monotonic_ms())){
   if(e.stop_process){std::lock_guard lock(mutex_);if(auto i=running_.find(e.attempt_id);i!=running_.end())i->second->data->cancel=true;}
   else store_.result(e.attempt_id,json::serialize(json::object{{"protocol",1},{"type","result"},{"attempt_id",e.attempt_id},{"status","abandoned"},{"reason","offer_expired"}}));
  }
  std::vector<std::unique_ptr<Running>> finished;{std::lock_guard lock(mutex_);for(auto i=running_.begin();i!=running_.end();){if(i->second->data->done){finished.push_back(std::move(i->second));i=running_.erase(i);}else ++i;}}finished.clear();
 }catch(const std::exception& e){paused_=true;admission_.pause(true);safe_log(std::string("Watchdog check failed; admissions paused: ")+e.what());}
 sleep_until_stop(stop_,100);
 }
}
}
