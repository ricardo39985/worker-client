#include <cctype>
#include "worker.hpp"
#include "owner_state.hpp"
#include "ow/lifecycle.hpp"
#include "ow/media_renditions.hpp"
#include "ow/adaptive_evidence.hpp"
#include "ow/device_identity.hpp"
#include "ow/progress.hpp"
#include "ow/encoder_run.hpp"
#include "ow/child_memory.hpp"
#include "ow/rendition_pipeline.hpp"
#include "pairing_adapter.hpp"
#include "hardware.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <future>
#include <limits>
#include <numeric>
#include <regex>
#include <sstream>
#include <tuple>
namespace ow::win {
namespace {
std::string priority_name(Priority p){return p==Priority::maximum?"maximum":p==Priority::normal?"normal":p==Priority::low?"low":"backup";}
Priority parse_priority(const std::string& p){if(p=="maximum")return Priority::maximum;if(p=="normal")return Priority::normal;if(p=="low")return Priority::low;if(p=="backup")return Priority::backup;throw std::runtime_error("Unknown stored priority");}
std::string pref_name(Preference p){return p==Preference::preferred?"preferred":p==Preference::allowed?"allowed":"disabled";}
Preference parse_pref(const std::string& p){if(p=="preferred")return Preference::preferred;if(p=="allowed")return Preference::allowed;if(p=="disabled")return Preference::disabled;throw std::runtime_error("Unknown task preference");}
std::string job_subject(const JobSpec& spec){
 if(spec.offer.capability=="media.video.renditions.v1")return "video renditions";
 if(spec.offer.capability=="media.image.renditions.v1")return "image renditions";
 return output_name(spec);
}
Resources stage_headroom(const std::filesystem::path& root,std::uint64_t reserve){
 auto available=available_resources(root,reserve);available.ram_mb=available.ram_mb/10*9;return available;
}
struct StageClaim {
 Admission& admission;std::string attempt,name;bool held{};
 StageClaim(Admission& a,std::string id,std::string stage):admission(a),attempt(std::move(id)),name(std::move(stage)){}
 StageClaim(const StageClaim&)=delete;StageClaim& operator=(const StageClaim&)=delete;
 void release(){if(held){admission.finish_stage(attempt,name);held=false;}}
 ~StageClaim(){release();}
};
std::string backend_name(adaptive::Backend backend){switch(backend){case adaptive::Backend::nvidia_nvenc:return "NVIDIA NVENC";case adaptive::Backend::intel_qsv:return "Intel QSV";case adaptive::Backend::amd_amf:return "AMD AMF";case adaptive::Backend::cpu:return "CPU";default:return "unknown";}}
void sleep_until_stop(const std::atomic_bool& stop,unsigned milliseconds){for(unsigned elapsed=0;elapsed<milliseconds&&!stop;elapsed+=100)Sleep(std::min(100u,milliseconds-elapsed));}
void check_protocol(const Response& r){
 if(r.status==404)throw std::runtime_error("Coordinator worker endpoints are not deployed; no pairing performed");
 if(r.status!=200)throw std::runtime_error("Coordinator protocol check returned HTTP "+std::to_string(r.status));
 auto v=parse(r.body);if(number(v.as_object(),"protocol",1)!=1)throw std::runtime_error("Coordinator worker protocol is incompatible");
}
struct ProbeRun {std::uint64_t elapsed_ms{},peak_vram_mb{};bool estimated_vram{};std::uint64_t peak_commit_mb{};};
ProbeRun run_child_probe(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& workspace,const std::function<bool()>& cancelled,Resources limits,const adaptive::Device* hardware_device=nullptr,std::uint64_t vram_limit_mb=UINT64_MAX,Tick timeout_ms=30000,std::uint32_t width=0,std::uint32_t height=0,StageTimer* timing=nullptr,const std::function<void(bool)>& on_activity={}){
 struct Activity {const std::function<void(bool)>& callback;bool active{};~Activity(){if(active)callback(false);}} activity{on_activity};
 const auto started=monotonic_ms();
 if(!limits.ram_mb||!limits.cpu_threads||!limits.scratch_mb)throw std::runtime_error("Configured/live resource budget cannot run converter probe");
 Child child(exe,args,workspace,limits,Priority::low);auto until=started+timeout_ms;std::uint64_t peak_vram{};bool observed_gpu=false;
 ChildTimingCapture capture(child,timing);
 for(;;){
  if(cancelled&&cancelled())throw std::runtime_error("Capability probe cancelled");
  if(hardware_device){
   auto sample=sample_gpu_memory(child.process_id(),*hardware_device);
    if(sample){peak_vram=std::max(peak_vram,sample->child_bytes);observed_gpu=true;}
    else{
     vram_limit_mb=std::min(vram_limit_mb,adaptive::vram_budget(hardware_device->dedicated_vram_mb,{},vram_limit_mb).mb);
    }
    if(peak_vram/(1024*1024)+(peak_vram%(1024*1024)!=0)>vram_limit_mb||!adaptive::vram_envelope_ok(hardware_device->dedicated_vram_mb,vram_limit_mb,
       sample?std::optional<adaptive::GpuMemoryTotals>{{sample->child_bytes,sample->adapter_bytes}}:std::nullopt))
     throw std::runtime_error("GPU memory exceeded conservative probe reservation; process tree aborted");
  }
  if(auto code=child.exit_code()){
   if(*code){std::string detail;try{detail=read_file(workspace/"process.log");}
    catch(...){}
    if(auto reason=child_memory_failure(detail,width,height,limits.ram_mb))throw std::runtime_error(*reason);
    if(detail.size()>2048)detail.erase(0,detail.size()-2048);
    throw std::runtime_error("FFmpeg functional probe failed with exit code "+std::to_string(*code)+(detail.empty()?std::string{}:": "+detail));}
   break;
  }
  if(on_activity&&!activity.active){try{const auto log=read_file(workspace/"process.log");static const std::regex frame(R"((?:^|\n)frame=[1-9][0-9]*\r?\n)");if(log.find("progress=end")==std::string::npos&&std::regex_search(log,frame)){activity.active=true;on_activity(true);}}catch(...){} }
  if(monotonic_ms()>=until)throw std::runtime_error("FFmpeg functional probe timed out");Sleep(50);
 }
 return {static_cast<std::uint64_t>(std::max<Tick>(1,monotonic_ms()-started)),peak_vram/(1024*1024)+(peak_vram%(1024*1024)!=0),hardware_device&&!observed_gpu,child.peak_commit_mb()};
}
std::optional<std::pair<std::uint32_t,std::uint32_t>> inspect_video_dimensions(const std::filesystem::path& exe,
 const std::filesystem::path& input,const std::filesystem::path& workspace,const std::function<bool()>& cancelled,Resources limits,StageTimer* timing=nullptr,std::string* source_info=nullptr,bool prepare_poster=false){
 auto args=rendition_inspection_arguments(workspace,prepare_poster);
 *(std::find(args.begin(),args.end(),L"-i")+1)=input.wstring();
 run_child_probe(exe,args,workspace,cancelled,limits,nullptr,UINT64_MAX,30000,0,0,timing);
 const auto log=read_file(workspace/"process.log");
 if(source_info)*source_info=log.substr(0,log.find("Output #"));
 static const std::regex dimensions(R"(Video:[^\r\n]*?([1-9][0-9]{0,4})x([1-9][0-9]{0,4})(?:[ ,]|$))");
 std::smatch match;if(!std::regex_search(log,match,dimensions))return {};
 try{return std::pair{static_cast<std::uint32_t>(std::stoul(match[1].str())),static_cast<std::uint32_t>(std::stoul(match[2].str()))};}
 catch(...){return {};}
}
std::string processor_model(){
 wchar_t value[512]{};DWORD bytes=sizeof(value);
 const auto status=RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",L"ProcessorNameString",RRF_RT_REG_SZ,nullptr,value,&bytes);
 return status==ERROR_SUCCESS?utf8(std::wstring(value)):"unavailable";
}
}
Config load_config(const std::filesystem::path& root){
 Config c;c.root=root;auto obj=validate_control_config(parse(read_file(root/"worker.json")).as_object());c.coordinator=text(obj,"coordinator_url",1024);
 for(const auto& v:obj.at("storage_hosts").as_array()){auto s=std::string(v.as_string());std::transform(s.begin(),s.end(),s.begin(),[](unsigned char x){return static_cast<char>(std::tolower(x));});if(!allowed_url("https://"+s+"/",{s}))throw std::runtime_error("Invalid approved storage host");c.storage_hosts.push_back(s);}
 c.ffmpeg=wide(text(obj,"ffmpeg_path",32700));c.ffmpeg_sha256=text(obj,"ffmpeg_sha256",64);
 const auto& limits=obj.at("limits").as_object();const auto vram_override=limits.find("vram_mb");c.vram_override_set=vram_override!=limits.end();c.budget={number(limits,"ram_mb",1024*1024),vram_override==limits.end()?0:number(limits,"vram_mb",UINT64_MAX),number(limits,"scratch_mb",1024*1024),number(limits,"cpu_threads",64)};c.reserve_ram_mb=number(limits,"reserve_ram_mb",1024*1024);c.max_jobs=static_cast<std::size_t>(number(limits,"max_jobs",64));
 if(c.budget.ram_mb<256||!c.budget.cpu_threads||!c.max_jobs||!c.budget.scratch_mb)throw std::runtime_error("Invalid resource budget");
 return c;
}
static Probe probe_ffmpeg_impl(const Config& c,const std::function<bool()>& cancelled,const ProbePhase& phase,const Probe* retained,const StageTimer::Sink& timing){
 auto run_child_probe=[&](const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& path,
  const std::function<bool()>& cancel,Resources limits,const adaptive::Device* device=nullptr,std::uint64_t cap=UINT64_MAX,Tick timeout=30000,std::uint32_t width=0,std::uint32_t height=0,const std::function<void(bool)>& activity={}){
   StageTimer stage("startup_child","none",timing,timing_wall_us,thread_cpu_us,true);
   return ::ow::win::run_child_probe(exe,args,path,cancel,limits,device,cap,timeout,width,height,&stage,activity);
 };
 auto progress=[&](std::string name,unsigned current=0,unsigned total=0){if(phase)phase(std::move(name),current,total);};
 progress("adapters");
 Probe p;if(retained){p.video=retained->video;p.image=retained->image;p.media_video=retained->media_video;p.media_image=retained->media_image;}
 std::vector<adaptive::Device> inventory;bool inventory_available=true;std::string inventory_failure;
 auto observed_json=[](std::optional<std::uint64_t> value)->json::value{return value?json::value(*value):json::value(nullptr);};
  try{inventory=enumerate_video_devices(!(c.vram_override_set&&c.budget.vram_mb==0));}catch(const std::exception& e){inventory_available=false;inventory_failure="Adapter inventory unavailable: "+std::string(e.what());p.reason=inventory_failure;}
 if(c.ffmpeg.empty()){
  p.reason+="; converter not configured: capabilities disabled; display inventory entries="+observed_count(inventory_available?std::optional<std::uint64_t>{inventory.size()}:std::nullopt)+"; configured_global_VRAM_override_MB="+(c.vram_override_set?std::to_string(c.budget.vram_mb):"not_set");
   const auto live=available_resources(c.root,c.reserve_ram_mb);MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);const bool memory_observed=GlobalMemoryStatusEx(&memory)!=FALSE;
   ULARGE_INTEGER disk_free{},disk_total{},disk_total_free{};const bool disk_observed=GetDiskFreeSpaceExW(c.root.c_str(),&disk_free,&disk_total,&disk_total_free)!=FALSE;
  json::array devices,reasons;
  reasons.emplace_back("Converter not configured; converter capabilities remain disabled");
  if(!inventory_available)reasons.emplace_back(inventory_failure);
  for(const auto& d:inventory){p.reason+=" ["+d.name+" id="+d.stable_id+" dedicated_VRAM_MB="+std::to_string(d.dedicated_vram_mb)+"; physical_id="+d.physical_id+"; encoder mapping="+(d.backend_identity_verified?"verified":"unavailable")+"; "+d.exclusion_reason+"]";
   devices.emplace_back(json::object{{"device_id",d.stable_id},{"name",d.name},{"physical_id",d.physical_id},{"exclusion_reason",d.exclusion_reason},{"backend",static_cast<int>(d.family)},{"driver_version",d.driver_version},{"dedicated_vram_mb",d.dedicated_vram_mb},{"adapter_index",d.adapter_index},{"backend_index",d.backend_index},{"backend_identity_verified",d.backend_identity_verified},{"driver_healthy",d.driver_healthy}});
  }
   try{std::filesystem::create_directories(c.root);atomic_write(c.root/"adaptive-hardware.json",json::serialize(json::object{{"schema",1},{"ffmpeg_sha256",""},{"processor",processor_model()},{"logical_cpu_count",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},{"total_ram_mb",observed_json(memory_observed?std::optional<std::uint64_t>{memory.ullTotalPhys/(1024*1024)}:std::nullopt)},{"available_ram_mb",live.ram_mb},{"total_disk_mb",observed_json(disk_observed?std::optional<std::uint64_t>{disk_total.QuadPart/(1024*1024)}:std::nullopt)},{"available_scratch_mb",live.scratch_mb},{"limits",{{"cpu_threads",c.budget.cpu_threads},{"ram_mb",c.budget.ram_mb},{"vram_mb_override",c.budget.vram_mb},{"vram_mb_override_set",c.vram_override_set},{"scratch_mb",c.budget.scratch_mb},{"max_jobs",c.max_jobs}}},{"devices",std::move(devices)},{"profiles",json::array{}},{"unavailable_reasons",std::move(reasons)}}));}catch(...){p.reason+="; could not persist NoConversion inventory";}
  return p;
 }
 auto workspace=c.root/"probes"/("probe-"+random_hex(8));
 try{
 progress("converter hash");
 auto live=available_resources(c.root,c.reserve_ram_mb);
 Resources probe_limits{std::min(c.budget.ram_mb,live.ram_mb),0,std::min(c.budget.scratch_mb,live.scratch_mb),std::min(c.budget.cpu_threads,live.cpu_threads)};
  auto run_probe=[&](const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& path,const std::function<bool()>& cancel,const adaptive::Device* device=nullptr,std::uint64_t vram_cap=UINT64_MAX){
   return run_child_probe(exe,args,path,cancel,probe_limits,device,vram_cap);
 };
 if(!c.ffmpeg.is_absolute()||!std::filesystem::is_regular_file(c.ffmpeg))throw std::runtime_error("Configured FFmpeg binary is missing");
 if(c.ffmpeg_sha256.size()!=64||sha256_file(c.ffmpeg)!=c.ffmpeg_sha256)throw std::runtime_error("FFmpeg SHA-256 differs from approved configuration");
 std::filesystem::create_directories(workspace);
 if(!retained){
 progress("CPU samples seed",1,5);
 run_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",L"color=c=black:s=32x32:r=1",L"-t",L"1",L"-an",L"-c:v",L"libx264",L"-threads",L"1",L"-pix_fmt",L"yuv420p",(workspace/"sample.mp4").wstring()},workspace,cancelled);
 std::filesystem::copy_file(workspace/"sample.mp4",workspace/"source.bin");
 progress("CPU samples h264",2,5);
 JobSpec spec;spec.offer.capability="conversion.video.h264";spec.offer.resources={512,0,128,1};run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.video=std::filesystem::file_size(workspace/"result.mp4")>0;
 progress("CPU samples jpeg",3,5);
 spec.offer.capability="conversion.image.jpeg";run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.image=std::filesystem::file_size(workspace/"result.jpg")>0;
 progress("CPU samples app video",4,5);
 try{for(const auto& rendition:rendition_plan(true,false,workspace)){run_probe(c.ffmpeg,rendition.arguments,workspace,cancelled);if(!std::filesystem::file_size(workspace/rendition.filename))throw std::runtime_error("App video probe produced no output");run_probe(c.ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(workspace/rendition.filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace,cancelled);}p.media_video=true;}catch(const std::exception&){p.media_video=false;}
 std::filesystem::copy_file(workspace/"result.jpg",workspace/"source.bin",std::filesystem::copy_options::overwrite_existing);
 progress("CPU samples app image",5,5);
 try{for(const auto& rendition:rendition_plan(false,false,workspace)){run_probe(c.ffmpeg,rendition.arguments,workspace,cancelled);if(!std::filesystem::file_size(workspace/rendition.filename))throw std::runtime_error("App image probe produced no output");run_probe(c.ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(workspace/rendition.filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace,cancelled);}p.media_image=true;}catch(const std::exception&){p.media_image=false;}
 }
 std::vector<std::string> unavailable;if(!inventory_available)unavailable.push_back(inventory_failure);std::map<std::string,std::string> budget_sources;Resources headroom{};std::vector<adaptive::Device> devices;
 std::optional<std::uint64_t> total_ram_mb,total_disk_mb;
 if(p.media_video){
  progress("tuning source");
  const auto tune_source=workspace/"adaptive-source.mp4";
  run_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",adaptive::tuning_source_filter(),L"-f",L"lavfi",L"-i",L"sine=frequency=880:sample_rate=48000",L"-t",L"3",L"-frames:v",L"72",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",L"-pix_fmt",L"yuv420p",L"-c:a",L"aac",(tune_source).wstring()},workspace,cancelled);
  std::filesystem::copy_file(tune_source,workspace/"source.bin",std::filesystem::copy_options::overwrite_existing);
  devices=std::move(inventory);adaptive::Device cpu{"cpu",adaptive::Backend::cpu,"",0,true,0,"CPU",0,true};
  for(const auto backend:{adaptive::Backend::nvidia_nvenc,adaptive::Backend::intel_qsv,adaptive::Backend::amd_amf})
   if(inventory_available&&std::none_of(devices.begin(),devices.end(),[&](const auto& device){return device.family==backend;}))unavailable.emplace_back(backend_name(backend)+" adapter not detected");
  MEMORYSTATUSEX physical{};physical.dwLength=sizeof(physical);if(GlobalMemoryStatusEx(&physical))total_ram_mb=physical.ullTotalPhys/(1024*1024);
  ULARGE_INTEGER disk_free{},disk_total{},disk_total_free{};if(GetDiskFreeSpaceExW(c.root.c_str(),&disk_free,&disk_total,&disk_total_free))total_disk_mb=disk_total.QuadPart/(1024*1024);
  std::vector<adaptive::Device> all{cpu};all.insert(all.end(),devices.begin(),devices.end());
   std::vector<adaptive::ProbeEvidence> evidence;std::vector<adaptive::TuningEvidence> tuning;json::array tuning_measurements;
  if(retained){
   auto saved=retain_current_profiles(all,retained->profiles,c.ffmpeg_sha256);
   evidence=std::move(saved.probes);tuning=std::move(saved.tuning);
   for(const auto& t:tuning){auto record=adaptive::tuning_record(t);record["source"]="retained_process_sample";tuning_measurements.emplace_back(std::move(record));
    budget_sources[t.stable_id+":"+std::to_string(static_cast<int>(t.codec))]="retained process sample; live budget rechecked at admission";}
  }
  auto already_proven=[&](const adaptive::Device& d,adaptive::Codec codec){return std::any_of(evidence.begin(),evidence.end(),[&](const auto& e){return e.stable_id==d.stable_id&&e.codec==codec;});};
  headroom=available_resources(c.root,c.reserve_ram_mb);
  const auto cpu_limit=std::min(c.budget.cpu_threads,headroom.cpu_threads);
  const auto ram_limit=std::min(c.budget.ram_mb,headroom.ram_mb);
  const auto scratch_limit=std::min(c.budget.scratch_mb,headroom.scratch_mb);
  const auto total_vram=std::accumulate(devices.begin(),devices.end(),std::uint64_t{},[](auto sum,const auto& d){return sum+(d.backend_identity_verified?d.dedicated_vram_mb:0);});
  adaptive::Limits limits{cpu_limit,ram_limit,c.vram_override_set?c.budget.vram_mb:total_vram/2,scratch_limit,static_cast<std::uint32_t>(std::min<std::size_t>(c.max_jobs,64))};
  auto decode_valid=[&](const std::filesystem::path& output,const std::string& codec,const std::filesystem::path& log_root,Resources child_limits){
   if(!std::filesystem::is_regular_file(output)||!std::filesystem::file_size(output))return false;
   run_child_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",output.wstring(),L"-f",L"null",L"-"},log_root,cancelled,child_limits,nullptr,UINT64_MAX,30000,adaptive::tuning_width,adaptive::tuning_height);
   const auto log=read_file(log_root/"process.log");
   return adaptive::tuning_output_valid(log,codec);
  };
  auto profile_for=[&](const adaptive::Device& d,adaptive::Codec codec){
   return adaptive::Profile{d.stable_id,d.driver_version,c.ffmpeg_sha256,d.family,codec,1,512,0,128,1,0,d.adapter_index,d.backend_index,d.backend_identity_verified,0,0,0,0,d.physical_id,d.name};
  };
  unsigned candidate=0;const auto candidates=static_cast<unsigned>((devices.size()+1)*2);
  std::string tuning_label;
  auto tuning_progress=[&](const std::string& step){progress(tuning_label+" "+step,candidate,candidates);};
  auto run_tuning=[&](const adaptive::Device& d,adaptive::Codec codec){
   const char* role=codec==adaptive::Codec::hevc?"feed_optimized":"feed";
   const std::string codec_name=codec==adaptive::Codec::hevc?"hevc":"h264";
   const auto output=workspace/(codec==adaptive::Codec::hevc?"optimized.mp4":"feed.mp4");
   auto plan=d.family==adaptive::Backend::cpu?rendition_plan(true,false,workspace):rendition_plan(true,false,workspace,std::vector<adaptive::Profile>{profile_for(d,codec)});
   auto rendition=std::find_if(plan.begin(),plan.end(),[&](const auto& item){return item.role==role;});
   if(rendition==plan.end()||(d.family!=adaptive::Backend::cpu&&!rendition->hardware))throw std::runtime_error("Adapter encoder command unavailable");
    auto child_ram=d.family==adaptive::Backend::cpu?std::uint64_t{512}:std::min(probe_limits.ram_mb,adaptive::gpu_child_memory_mb(adaptive::tuning_width,adaptive::tuning_height,codec));
    Resources candidate_limits{child_ram,0,128,1};
    tuning_progress("budget="+std::to_string(child_ram)+" MiB 1920x1080 host input");
    if(probe_limits.ram_mb<child_ram||probe_limits.scratch_mb<128||probe_limits.cpu_threads<1)throw std::runtime_error("live headroom cannot safely reserve one measured profile");
    std::uint64_t vram_probe_cap{};bool estimated_vram=false;
    if(d.family!=adaptive::Backend::cpu){
     if(c.vram_override_set&&!c.budget.vram_mb)throw std::runtime_error("explicit global VRAM limit is zero; GPU probes are disabled");
     auto live_sample=sample_gpu_memory(0,d);
     const auto budget=adaptive::vram_budget(d.dedicated_vram_mb,
      live_sample?std::optional<std::uint64_t>{live_sample->adapter_bytes}:std::nullopt,limits.vram_mb);
     vram_probe_cap=budget.mb;estimated_vram=budget.estimated;
     if(!vram_probe_cap)throw std::runtime_error("no safe VRAM budget: telemetry/declared VRAM unavailable or headroom/cap exhausted");
    }
    double slowest_fps=std::numeric_limits<double>::max();std::uint64_t peak_vram_mb=0,peak_commit_mb=0;
   for(unsigned repeat=0;repeat<2;++repeat){
    if(cancelled&&cancelled())throw std::runtime_error("Capability tuning cancelled");
     tuning_progress("encode "+std::to_string(repeat+1)+"/2");
     const auto sample=run_child_probe(c.ffmpeg,rendition->arguments,workspace,cancelled,candidate_limits,d.family==adaptive::Backend::cpu?nullptr:&d,vram_probe_cap,120000,adaptive::tuning_width,adaptive::tuning_height);
     tuning_progress("decode "+std::to_string(repeat+1)+"/2");
     const auto stage_started=monotonic_ms();
     if(!decode_valid(output,codec_name,workspace,candidate_limits))throw std::runtime_error("Synthetic app output decode/profile validation failed");
     slowest_fps=std::min(slowest_fps,72000.0/static_cast<double>(sample.elapsed_ms+(monotonic_ms()-stage_started)));
    peak_vram_mb=std::max(peak_vram_mb,sample.peak_vram_mb);
    peak_commit_mb=std::max(peak_commit_mb,sample.peak_commit_mb);
    if(d.family!=adaptive::Backend::cpu&&sample.estimated_vram)estimated_vram=true;
   }
   if(!slowest_fps)throw std::runtime_error("Synthetic tuning measured no useful throughput");
   const auto reference_hash=sha256_file(output);
   if(d.family!=adaptive::Backend::cpu){
    const auto measured_cap=std::max<std::uint64_t>(512,((peak_commit_mb+std::max<std::uint64_t>(128,peak_commit_mb/4)+127)/128)*128);
    if(measured_cap<child_ram){
     candidate_limits.ram_mb=measured_cap;
     try{run_child_probe(c.ffmpeg,rendition->arguments,workspace,cancelled,candidate_limits,&d,vram_probe_cap,120000,adaptive::tuning_width,adaptive::tuning_height);
      if(!decode_valid(output,codec_name,workspace,candidate_limits)||sha256_file(output)!=reference_hash)throw std::runtime_error("Reduced-cap output differs");child_ram=measured_cap;
     }catch(const std::exception& e){candidate_limits.ram_mb=child_ram;unavailable.push_back(d.stable_id+" reduced memory cap trial failed; retained original: "+e.what());}
    }
   }
   std::uint64_t vram_reservation=0;
   if(d.family!=adaptive::Backend::cpu){
    // Missing child samples reserve the entire bounded fallback, never a guessed footprint.
    if(estimated_vram){
     vram_probe_cap=std::min(vram_probe_cap,adaptive::vram_budget(d.dedicated_vram_mb,{},limits.vram_mb).mb);
     vram_reservation=vram_probe_cap;
     if(!vram_reservation||peak_vram_mb>vram_reservation)throw std::runtime_error("Observed GPU footprint exceeds declared-VRAM estimate");
    }else vram_reservation=std::min(vram_probe_cap,peak_vram_mb+std::max<std::uint64_t>(8,std::min<std::uint64_t>(128,peak_vram_mb/4)));
     if(vram_reservation>d.dedicated_vram_mb||vram_reservation>limits.vram_mb||vram_reservation>vram_probe_cap)throw std::runtime_error("Measured VRAM footprint exceeds configured, live, probe reservation or dedicated device caps");
   }
   std::uint32_t tested_concurrency=1;
   // Qualify simultaneous encodes, never infer sessions from core/card count.
   // Retain the last successful point if a larger bounded trial fails.
   double best_aggregate=slowest_fps;
   const auto maximum=static_cast<unsigned>(std::min({cpu_limit,ram_limit/child_ram,scratch_limit/128,static_cast<std::uint64_t>(c.max_jobs),std::uint64_t{64}}));
   for(unsigned parallel=2;parallel<=maximum;parallel=parallel==maximum?maximum+1:std::min(maximum,parallel*2)){
    if(d.family!=adaptive::Backend::cpu&&vram_reservation*parallel>vram_probe_cap)break;
    if(c.capacity_foreground?c.capacity_foreground():desktop_priority(Priority::normal)==Priority::low){unavailable.push_back("capacity sweep deferred for foreground activity: "+d.stable_id);break;}
    try{
     std::uint64_t total_ms=0,paced_ms=0;unsigned repeats=0,observed_concurrency=0;double worst_cpu=0,worst_disk=0,worst_ram=0;
     auto before=sample_runtime_pressure(c.root);(void)before;
     for(;repeats<3||total_ms<10000;++repeats){
      if(repeats>=30)throw std::runtime_error("capacity measurement window did not fill");
      const auto live_now=available_resources(c.root,c.reserve_ram_mb);
      if(live_now.ram_mb<child_ram*parallel||live_now.scratch_mb<128*parallel)throw std::runtime_error("live capacity headroom fell");
      std::stop_source abort;std::atomic_uint active_encodes{},peak_encodes{};
      auto activity=[&](bool enter){if(enter){const auto active=++active_encodes;auto peak=peak_encodes.load();while(peak<active&&!peak_encodes.compare_exchange_weak(peak,active)){} }else --active_encodes;};
      auto group_cancel=[&]{return abort.stop_requested()||(cancelled&&cancelled())||(c.capacity_foreground?c.capacity_foreground():desktop_priority(Priority::normal)==Priority::low);};
      double group_peak_ram=0;std::string pressure_reason;
      std::jthread monitor([&](std::stop_token stop){while(!stop.stop_requested()){
       const auto pressure=sample_runtime_pressure(c.root);if(pressure.host.ram_percent)group_peak_ram=std::max(group_peak_ram,*pressure.host.ram_percent);
       const bool foreground=c.capacity_foreground?c.capacity_foreground():pressure.host.foreground;
       if(foreground||(pressure.host.ram_percent&&*pressure.host.ram_percent>=90)){pressure_reason=foreground?"foreground responsiveness/input requires capacity":"RAM pressure reached 90 percent";abort.request_stop();}
       for(int pause=0;pause<20&&!stop.stop_requested();++pause)Sleep(10);
      }});
      std::vector<std::future<void>> children;children.reserve(parallel);const auto begin=monotonic_ms();
      for(unsigned index=0;index<parallel;++index){
       auto path=workspace/("capacity-"+codec_name+"-"+std::to_string(parallel)+"-"+std::to_string(repeats)+"-"+std::to_string(index));
       std::filesystem::create_directory(path);std::filesystem::create_hard_link(tune_source,path/"source.bin");
       auto child_plan=rendition_plan(true,false,path,std::vector<adaptive::Profile>{profile_for(d,codec)});
       auto command=encoder_progress_arguments(std::find_if(child_plan.begin(),child_plan.end(),[&](const auto& item){return item.role==role;})->arguments);
       if(repeats==0)command.insert(std::find(command.begin(),command.end(),L"-noautorotate"),{L"-readrate",L"1"});
       children.push_back(std::async(std::launch::async,[&,path,command=std::move(command)]{
        run_child_probe(c.ffmpeg,command,path,group_cancel,candidate_limits,d.family==adaptive::Backend::cpu?nullptr:&d,d.family==adaptive::Backend::cpu?UINT64_MAX:vram_reservation,120000,adaptive::tuning_width,adaptive::tuning_height,activity);
        const auto target=path/output.filename();
        if(!decode_valid(target,codec_name,path,candidate_limits)||sha256_file(target)!=reference_hash)throw std::runtime_error("simultaneous sample quality or decode differs");
       }));
      }
      try{for(auto& child:children)child.get();}catch(...){abort.request_stop();for(auto& child:children)if(child.valid())try{child.get();}catch(...){}monitor.request_stop();monitor.join();if(!pressure_reason.empty())throw std::runtime_error(pressure_reason);throw;}
      monitor.request_stop();monitor.join();if(!pressure_reason.empty())throw std::runtime_error(pressure_reason);
      worst_ram=std::max(worst_ram,group_peak_ram);
      const auto elapsed=static_cast<std::uint64_t>(std::max<Tick>(1,monotonic_ms()-begin));total_ms+=elapsed;
      observed_concurrency=std::max(observed_concurrency,peak_encodes.load());if(repeats==0){paced_ms=elapsed;if(peak_encodes.load()<parallel)throw std::runtime_error("simultaneous active encode count was not observed");}
      const auto measured=sample_runtime_pressure(c.root);
      if(!measured.host.cpu_percent||!measured.host.disk_percent||!measured.host.ram_percent)throw std::runtime_error("capacity telemetry unavailable");
      worst_cpu=std::max(worst_cpu,*measured.host.cpu_percent);worst_disk=std::max(worst_disk,*measured.host.disk_percent);worst_ram=std::max(worst_ram,*measured.host.ram_percent);
      if(worst_cpu>=90||worst_disk>=90||worst_ram>=90||(c.capacity_foreground?c.capacity_foreground():measured.host.foreground))throw std::runtime_error("capacity target/foreground headroom reached");
     }
     const double aggregate=static_cast<double>(parallel)*(repeats-1)*72000.0/static_cast<double>(total_ms-paced_ms);
     tuning_measurements.emplace_back(json::object{{"device_id",d.stable_id},{"codec",codec_name},{"simultaneous",parallel},{"validated_repeats",repeats},{"window_ms",total_ms},{"paced_qualification_ms",paced_ms},{"observed_active_encodes",observed_concurrency},{"aggregate_fps",aggregate},{"worst_window_cpu_percent",worst_cpu},{"worst_window_disk_percent",worst_disk},{"worst_window_ram_percent",worst_ram},{"artifact_bytes_equal",true}});
     if(aggregate<best_aggregate*1.02)break;
     best_aggregate=aggregate;tested_concurrency=parallel;
    }catch(const std::exception& e){unavailable.push_back("capacity "+d.stable_id+" "+codec_name+" n="+std::to_string(parallel)+": "+e.what()+"; retained proven n="+std::to_string(tested_concurrency));break;}
   }
   if(d.family!=adaptive::Backend::cpu)budget_sources[d.stable_id+":"+std::to_string(static_cast<int>(codec))]=estimated_vram?"declared-VRAM estimate":"live VRAM telemetry";
   evidence.push_back({d.stable_id,d.driver_version,c.ffmpeg_sha256,d.family,codec,true,true,true});
   tuning.push_back({d.stable_id,d.family,codec,slowest_fps,1,child_ram,vram_reservation,128,true,true,true,tested_concurrency,adaptive::tuning_width,adaptive::tuning_height});
   if(d.family!=adaptive::Backend::cpu)tuning.back().child_memory_overhead_mb=adaptive::gpu_card_overhead_mb(adaptive::tuning_width,adaptive::tuning_height,codec,adaptive::InputMemory::host,peak_commit_mb);
   auto measurement=adaptive::tuning_record(tuning.back());measurement["peak_child_commit_mb"]=peak_commit_mb;
   tuning_measurements.emplace_back(std::move(measurement));
  };
  if(ram_limit<512||scratch_limit<128||!cpu_limit)unavailable.emplace_back("live headroom below one CPU profile cost; adaptive app profiles withheld");
   else for(auto codec:{adaptive::Codec::h264,adaptive::Codec::hevc}){
   ++candidate;tuning_label="tuning cpu/"+std::string(codec==adaptive::Codec::hevc?"hevc":"h264");tuning_progress("sample");
   try{if(!already_proven(cpu,codec))run_tuning(cpu,codec);}catch(const std::exception& e){unavailable.emplace_back(std::string("CPU ")+(codec==adaptive::Codec::h264?"H.264: ":"HEVC: ")+e.what());}
   for(const auto& device:devices){
    ++candidate;tuning_label="tuning "+adaptive::backend_token(device.family)+"/"+(codec==adaptive::Codec::hevc?"hevc":"h264")+" adapter "+std::to_string(device.adapter_index);tuning_progress("candidate");
     if(c.vram_override_set&&!c.budget.vram_mb){unavailable.emplace_back(device.stable_id+" explicit global VRAM limit is zero; retained in inventory without GPU probing");continue;}
    if(!device.backend_identity_verified){unavailable.emplace_back(device.name+" device_id="+device.stable_id+" physical_id="+device.physical_id+" "+device.exclusion_reason);continue;}
    if(!device.driver_healthy){unavailable.emplace_back(device.stable_id+" driver or dedicated VRAM unavailable");continue;}
    if(already_proven(device,codec))continue;
    try{retry_probe([&]{run_tuning(device,codec);},[&]{return cancelled&&cancelled();},[&](unsigned ms){
     tuning_progress("retry after failed probe in "+std::to_string(ms/1000)+"s");
     for(unsigned elapsed=0;elapsed<ms;elapsed+=100){if(cancelled&&cancelled())throw std::runtime_error("Capability probe cancelled");Sleep(100);}
    });}catch(const std::exception& e){
     unavailable.emplace_back(device.stable_id+" "+(codec==adaptive::Codec::h264?"H.264: ":"HEVC: ")+e.what());
     p.gpu_failures.push_back({device.name,codec==adaptive::Codec::h264?"h264":"hevc",e.what(),device.stable_id});
    }
   }
  }
  for(const auto& measured:tuning)if(measured.backend!=adaptive::Backend::cpu){
   const auto cpu_measure=std::find_if(tuning.begin(),tuning.end(),[&](const auto& candidate){return candidate.backend==adaptive::Backend::cpu&&candidate.codec==measured.codec;});
   if(cpu_measure!=tuning.end()&&adaptive::comparable_resolution(measured,*cpu_measure)&&measured.frames_per_second<cpu_measure->frames_per_second*1.05){
    const auto reason=std::string(measured.frames_per_second<cpu_measure->frames_per_second?"measured slower than CPU":"measured gain below 5%")+" at "+std::to_string(measured.measured_width)+"x"+std::to_string(measured.measured_height);
    p.fallback_notes.push_back(adaptive::backend_token(measured.backend)+"/"+(measured.codec==adaptive::Codec::hevc?"hevc":"h264")+" "+reason+"; proven backend retained for contention");
   }
  }
  progress("selecting sample profiles");
  p.profiles=adaptive::choose_profiles(all,evidence,tuning,limits,c.ffmpeg_sha256);
  const auto cpu_h264=std::find_if(p.profiles.begin(),p.profiles.end(),[](const auto& profile){return profile.backend==adaptive::Backend::cpu&&profile.codec==adaptive::Codec::h264;});
  const auto cpu_hevc=std::find_if(p.profiles.begin(),p.profiles.end(),[](const auto& profile){return profile.backend==adaptive::Backend::cpu&&profile.codec==adaptive::Codec::hevc;});
  if(cpu_h264==p.profiles.end()||cpu_hevc==p.profiles.end()){
   p.media_video=false;unavailable.emplace_back("sampled CPU fallback profile missing for H.264 or HEVC; app video capability disabled");
  }
  progress("saving sample evidence");
  json::array saved_profiles,saved_devices;
  for(const auto& device:devices)saved_devices.emplace_back(json::object{{"device_id",device.stable_id},{"name",device.name},{"physical_id",device.physical_id},{"exclusion_reason",device.exclusion_reason},{"backend",static_cast<int>(device.family)},{"driver_version",device.driver_version},{"dedicated_vram_mb",device.dedicated_vram_mb},{"adapter_index",device.adapter_index},{"backend_index",device.backend_index},{"backend_identity_verified",device.backend_identity_verified},{"driver_healthy",device.driver_healthy}});
  for(const auto& profile:p.profiles)saved_profiles.emplace_back(adaptive::profile_record(profile));
  json::array saved_reasons;for(const auto& reason:unavailable)saved_reasons.emplace_back(reason);
   atomic_write(c.root/"adaptive-hardware.json",json::serialize(json::object{{"schema",1},{"ffmpeg_sha256",c.ffmpeg_sha256},{"processor",processor_model()},{"logical_cpu_count",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},{"total_ram_mb",observed_json(total_ram_mb)},{"available_ram_mb",headroom.ram_mb},{"total_disk_mb",observed_json(total_disk_mb)},{"available_scratch_mb",headroom.scratch_mb},{"limits",{{"cpu_threads",c.budget.cpu_threads},{"ram_mb",c.budget.ram_mb},{"vram_mb_override",c.budget.vram_mb},{"vram_mb_override_set",c.vram_override_set},{"vram_mb_effective",limits.vram_mb},{"scratch_mb",c.budget.scratch_mb},{"max_jobs",c.max_jobs}}},{"devices",std::move(saved_devices)},{"profiles",std::move(saved_profiles)},{"tuning_measurements",std::move(tuning_measurements)},{"unavailable_reasons",std::move(saved_reasons)}}));
  if(p.profiles.empty())unavailable.emplace_back("no codec profile passed probe and bounded tuning");
 }else unavailable.emplace_back("CPU app video probe failed; hardware remains disabled");
 p.reason="Adaptive media setup: CPU="+processor_model()+" logical="+std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS))+" RAM_total_MB="+observed_count(total_ram_mb)+" RAM_available_MB="+observed_count(total_ram_mb?std::optional<std::uint64_t>{headroom.ram_mb}:std::nullopt)+" disk_total_MB="+observed_count(total_disk_mb)+" disk_available_MB="+observed_count(total_disk_mb?std::optional<std::uint64_t>{headroom.scratch_mb}:std::nullopt)+" approved_ffmpeg_sha256="+c.ffmpeg_sha256+"; display inventory entries="+observed_count(inventory_available?std::optional<std::uint64_t>{devices.empty()?inventory.size():devices.size()}:std::nullopt);
 for(const auto& device:(devices.empty()?inventory:devices))p.reason+=" ["+device.name+" id="+device.stable_id+" dedicated_VRAM_MB="+std::to_string(device.dedicated_vram_mb)+" driver="+(device.driver_version.empty()?"unavailable":device.driver_version)+" physical_id="+device.physical_id+" "+device.exclusion_reason+"]";
 p.reason+="; schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(p.profiles))+"; retained sample profiles="+std::to_string(p.profiles.size())+"; app video="+(p.media_video?"CPU fallback sampled":"unavailable")+" app image="+(p.media_image?"sample passed":"unavailable");
 for(const auto& profile:p.profiles)p.reason+="; chosen "+backend_name(profile.backend)+" "+(profile.codec==adaptive::Codec::h264?"H.264":"HEVC")+" at "+std::to_string(profile.measured_frames_per_second)+" synthetic fps at "+std::to_string(profile.measured_width)+"x"+std::to_string(profile.measured_height)+" child_budget_MiB="+std::to_string(profile.ram_mb)+" with concurrency="+std::to_string(profile.max_concurrency)+(profile.backend==adaptive::Backend::cpu?"":" ("+budget_sources[profile.stable_id+":"+std::to_string(static_cast<int>(profile.codec))]+")");
 for(const auto& reason:unavailable)p.reason+="; fallback "+reason;
 p.complete=true;
 }catch(const std::exception& e){p.reason=e.what();}
 std::error_code error;std::filesystem::remove_all(workspace,error);return p;
}
Probe probe_ffmpeg(const Config& c,const std::function<bool()>& cancelled,const ProbePhase& phase,const Probe* retained,const ProbeTiming& timing){
 StageTotals totals;const auto main_thread=std::this_thread::get_id();
 StageTimer startup("startup_probe","none",[&](const StageSample& s){
  if(timing)timing("STARTUP "+totals.record(s.wall_us,s.controller_cpu_us));
 },timing_wall_us,thread_cpu_us);
 return probe_ffmpeg_impl(c,cancelled,phase,retained,[&](const StageSample& s){
  totals.add(s,std::this_thread::get_id()!=main_thread);if(timing)timing(stage_record(s));
 });
}
Worker::Worker(Config config):Worker(std::move(config),false){}
Worker::Worker(Config config,bool isolated):config_(std::move(config)),store_(config_.root/"journal.sqlite3"),log_(config_.root/"worker.log"),admission_(config_.budget,config_.max_jobs),capacity_(static_cast<unsigned>(std::min<std::uint64_t>(64,config_.budget.cpu_threads))),isolated_(isolated){
 std::optional<std::string> proven_origin;
 ProtectedPairingSecrets identity(config_.root);
 if(auto credential=identity.read(SecretSlot::credential)) {
  proven_origin=text(parse(*credential).as_object(),"coordinator_url",1024);
 }
 if(!config_.coordinator.empty())bind_journal_origin(store_,config_.coordinator,proven_origin);
 if(isolated_&&!std::filesystem::exists(config_.root/"worker.json")){
  auto doc=control_defaults();doc["coordinator_url"]=config_.coordinator;doc["ffmpeg_path"]=utf8(config_.ffmpeg.wstring());doc["ffmpeg_sha256"]=config_.ffmpeg_sha256;doc["limits"]=json::object{{"ram_mb",config_.budget.ram_mb},{"reserve_ram_mb",config_.reserve_ram_mb},{"scratch_mb",config_.budget.scratch_mb},{"cpu_threads",config_.budget.cpu_threads},{"max_jobs",config_.max_jobs}};atomic_write(config_.root/"worker.json",json::serialize(doc));
 }
 effective_config_=load_control_document(config_.root,store_);
 settings_=std::make_unique<ControlSettings>(effective_config_,[this](const json::object& doc){try{atomic_write(config_.root/"worker.json",json::serialize(doc));}catch(...){throw ControlFailure(5,"Atomic settings persistence failed");}});
 apply_operator_settings();admission_.defer_new_starts("telemetry_unavailable");
 for(const auto& id:store_.interrupted())store_.result(id,json::serialize(json::object{{"protocol",1},{"type","result"},{"attempt_id",id},{"status","abandoned"},{"reason","worker_restarted"}}));
 safe_log("WORKER STARTED | counts since this process; H for last 10 outcomes");
 {std::lock_guard lock(report_mutex_);report_.priority(priority_name(priority_));report_.state("paused",paused_?"operator":"starting",utc_ms(),monotonic_ms());publish_report(config_.root/"worker-status.txt","");publish_report(config_.root/"worker-history.txt","");}
 safe_log("STATE PRIORITY "+priority_name(priority_));
 report_status();
 if(isolated_)return;
 // Slow capability probes run on the background thread, never blocking tray creation.
 network_=std::jthread([this]{network();});try{watchdog_=std::jthread([this]{watchdog();});hash_watchdog_=std::jthread([this]{
  while(!stop_){for(unsigned elapsed=0;elapsed<60000&&!stop_;elapsed+=100)Sleep(100);if(stop_)break;
   if(config_.ffmpeg.empty()||config_.ffmpeg_sha256.size()!=64)continue;
   try{if(sha256_file(config_.ffmpeg)!=config_.ffmpeg_sha256){ffmpeg_hash_changed_=true;break;}}catch(...){ffmpeg_hash_changed_=true;break;}
  }
 });}catch(...){stop_=true;network_.join();if(recovery_.joinable())recovery_.join();if(watchdog_.joinable())watchdog_.join();throw;}
}
Worker::~Worker(){stop_=true;{std::lock_guard lock(mutex_);for(auto& [_,r]:running_)r->data->cancel=true;}if(network_.joinable())network_.join();if(recovery_.joinable())recovery_.join();if(watchdog_.joinable())watchdog_.join();if(hash_watchdog_.joinable())hash_watchdog_.join();std::unordered_map<std::string,std::unique_ptr<Running>> remaining;{std::lock_guard lock(mutex_);remaining.swap(running_);}remaining.clear();}
void Worker::safe_log(const std::string& s) noexcept {try{log_.write(s);}catch(...){paused_=true;admission_.pause(true);}}
void Worker::publish_report(const std::filesystem::path& path,const std::string& frame) noexcept {
 try{atomic_write(path,frame);report_io_failed_=false;}catch(...){if(!report_io_failed_)safe_log("Console status file unavailable; durable reporting continues");report_io_failed_=true;}
}
void Worker::report_status() noexcept {try{
 std::lock_guard lock(report_mutex_);const auto now=monotonic_ms(),wall=utc_ms();
 if(paused_&&report_.accepting())report_.state("paused","reason unknown",wall,now);
 auto due=report_.poll(now);
 if(!due.live&&!due.durable&&!due.summary)return;
 MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);readings_.ram_bytes=GlobalMemoryStatusEx(&memory)?std::optional<std::uint64_t>{memory.ullAvailPhys}:std::nullopt;
 ULARGE_INTEGER disk{};readings_.scratch_bytes=GetDiskFreeSpaceExW(config_.root.c_str(),&disk,nullptr,nullptr)?std::optional<std::uint64_t>{disk.QuadPart}:std::nullopt;
 // No coordinator queue depth exists in protocol v1; pending ACKs and local
 // reservations must not be relabelled as a measured remote queue.
 if(due.summary)safe_log(report_.summary(wall,now,readings_));
 const auto table=report_.table(wall,now,readings_);
 if(due.live)publish_report(config_.root/"worker-status.txt",status_frame(table));
 if(due.durable)for(const auto& line:table)safe_log(line.text);
 }catch(...){/* Reporting must never change admission or terminate execution. */}
}
void Worker::report_state(std::string name,std::string reason){
 if(exit_requested_&&name!="stopping")return;
 {std::lock_guard lock(report_mutex_);const auto before=report_.state_line();report_.state(std::move(name),std::move(reason),utc_ms(),monotonic_ms());if(before!=report_.state_line())safe_log("acceptance: "+report_.state_line());}
 report_status();
}
void Worker::state(std::string s){auto shown=s.starts_with("STATE ")?s.substr(6):s;bool changed{};{std::lock_guard lock(mutex_);changed=status_!=shown;status_=shown;}if(changed)safe_log(s);}
std::string Worker::status() const{std::lock_guard lock(mutex_);return status_;}
std::size_t Worker::active_count() const{return admission_.active().size();}
void Worker::apply_operator_settings(bool apply_pause){
 const auto op=settings_?settings_->current().at("operator").as_object():effective_config_.at("operator").as_object();
 priority_=parse_priority(text(op,"priority"));video_=parse_pref(text(op,"video_preference"));image_=parse_pref(text(op,"image_preference"));
 if(apply_pause){paused_=op.at("paused").as_bool();admission_.pause(paused_);}apply_preferences();
 {std::lock_guard lock(report_mutex_);report_.priority(priority_name(priority_));}
}
void Worker::update_settings(const json::object& values){
 std::lock_guard lock(control_mutex_);
 if(auto* paused=values.if_contains("operator.paused");paused&&paused->is_bool()&&!paused->as_bool()&&exit_requested_)throw std::runtime_error("Worker is exiting; restart it to resume");
 settings_->patch(values);apply_operator_settings(values.contains("operator.paused"));
 state(paused_?"STATE PAUSED":"STATE RESUMED");report_state(paused_||!report_connected_?"paused":"accepting",paused_?"operator":report_connected_?"resumed":"coordinator offline");
}
void Worker::pause(bool p){update_settings({{"operator.paused",p}});}
void Worker::priority(Priority p){update_settings({{"operator.priority",priority_name(p)}});}
void Worker::apply_preferences(){admission_.capability("conversion.video.h264",verified_video_.load(),video_);admission_.capability("conversion.image.jpeg",verified_image_.load(),image_);admission_.capability("media.video.renditions.v1",verified_media_video_.load(),video_);admission_.capability("media.image.renditions.v1",verified_media_image_.load(),image_);}
adaptive::Scheduler::Reservation Worker::reserve_hardware_profiles(const std::vector<adaptive::Codec>& codecs,Resources& cost,const std::string& attempt,const std::string& stage,bool cpu_only,std::uint32_t width,std::uint32_t height,std::uint64_t minimum_ram){
 std::lock_guard lock(mutex_);if(!adaptive_scheduler_)return {};
 if(monotonic_ms()>=cached_capacity_until_){
 std::vector<adaptive::Device> devices;
 try{devices=enumerate_video_devices();}catch(...){devices.clear();}
 std::map<std::string,std::uint64_t> device_caps;
 for(const auto& p:hardware_profiles_)if(p.backend!=adaptive::Backend::cpu)
  device_caps[p.stable_id]=std::max(device_caps[p.stable_id],p.vram_mb*p.proven_sessions);
 for(const auto& [id,cap]:device_caps){
  std::uint64_t available{};auto d=std::find_if(devices.begin(),devices.end(),[&](const auto& item){return item.stable_id==id&&item.backend_identity_verified;});
  if(d!=devices.end()){
   auto sample=sample_gpu_memory(0,*d);
   available=adaptive::vram_budget(d->dedicated_vram_mb,sample?std::optional<std::uint64_t>{sample->adapter_bytes}:std::nullopt,cap).mb;
  }
  adaptive_scheduler_->set_device_available_vram(id,std::min(cap,available));
 }
 cached_capacity_until_=monotonic_ms()+1000;
 }
 const auto vram=config_.vram_override_set?config_.budget.vram_mb:adaptive::profile_vram_capacity(hardware_profiles_);
 std::vector<std::string> exclusions;
 auto reservation=adaptive_scheduler_->reserve(codecs,adaptive::Limits{cost.cpu_threads,config_.budget.ram_mb,vram,config_.budget.scratch_mb,1},&exclusions,cpu_only,width,height,offline_pins_.contains(codecs.front())?offline_pins_.at(codecs.front()):std::string{});
 if(!reservation){admission_.waiting(attempt,stage,exclusions.empty()?"proven_encoder_slots":exclusions.front());return {};}
 if(width&&height&&!reservation.resize_memory(width,height,adaptive::Limits{cost.cpu_threads,config_.budget.ram_mb,vram,config_.budget.scratch_mb,1})){
  reservation.release();
  if(!cpu_only)reservation=adaptive_scheduler_->reserve(codecs,adaptive::Limits{cost.cpu_threads,config_.budget.ram_mb,vram,config_.budget.scratch_mb,1},nullptr,true,width,height);
  if(!reservation){admission_.waiting(attempt,stage,"child_memory_budget");return {};}
 }
 if(minimum_ram&&!reservation.grow_memory(minimum_ram,adaptive::Limits{cost.cpu_threads,config_.budget.ram_mb,vram,config_.budget.scratch_mb,1})){admission_.waiting(attempt,stage,"observed_allocation_failure_RAM_growth");return {};}
 cost.ram_mb=0;cost.cpu_threads=0;
 for(const auto& p:reservation.profiles()){cost.ram_mb=std::max(cost.ram_mb,p.ram_mb);cost.cpu_threads=std::max(cost.cpu_threads,p.cpu_threads);}
 const auto decision=admission_.stage(attempt,stage,Resources{cost.ram_mb,0,0,cost.cpu_threads},monotonic_ms(),stage_headroom(config_.root,config_.reserve_ram_mb),capacity_.stage_limit(monotonic_ms()));
 if(!decision.accepted)return {};
 capacity_.activity(admission_.stage_count());
 for(const auto& p:reservation.profiles())if(p.backend!=adaptive::Backend::cpu&&gpu_capacity_.contains(p.stable_id))gpu_capacity_.at(p.stable_id)->activity(adaptive_scheduler_->device_concurrency(p.stable_id));
 return reservation;
}
void Worker::wait_stage(const std::string& attempt,const std::string& stage,Resources cost,const std::function<bool()>& cancelled){
 std::string previous;
 for(;;){
  if(cancelled())throw std::runtime_error("Job cancelled while waiting for stage capacity");
  auto decision=admission_.stage(attempt,stage,Resources{cost.ram_mb,0,0,cost.cpu_threads},monotonic_ms(),stage_headroom(config_.root,config_.reserve_ram_mb),capacity_.stage_limit(monotonic_ms()));
  if(decision.accepted){capacity_.activity(admission_.stage_count());return;}
  if(previous!=decision.reason){previous=decision.reason;safe_log("JOB "+attempt+" QUEUED | "+stage+" | "+previous);}
  Sleep(100);
 }
}
void Worker::preference(bool video,Preference value){update_settings({{video?"operator.video_preference":"operator.image_preference",pref_name(value)}});}
void Worker::exit(bool kill){std::lock_guard control_lock(control_mutex_);pause(true);exit_requested_=true;{std::lock_guard lock(mutex_);if(kill)for(auto& [_,r]:running_)r->data->cancel=true;}state(kill?"STATE STOPPING":"STATE DRAINING");report_state("stopping",kill?"operator stop":"operator drain");}
void Worker::restart(){std::lock_guard lock(control_mutex_);if(exit_requested_&&!restart_requested_)throw std::runtime_error("Explicit operator exit already requested");restart_requested_=true;paused_=true;admission_.pause(true);exit_requested_=true;state("STATE RESTART_DRAIN");report_state("stopping","restart drain; desired admission retained");}
bool Worker::ready_to_exit(){if(!exit_requested_)return false;std::lock_guard lock(mutex_);return restart_requested_?restart_quiescent(running_.size(),admission_.active().size(),store_.unsettled_count()):running_.empty();}
json::object Worker::heartbeat(){
 auto free=available_resources(config_.root,config_.reserve_ram_mb);auto used=admission_.used();json::array active;
 for(const auto& a:admission_.active())active.emplace_back(json::object{{"attempt_id",a.offer.attempt_id},{"state",a.phase==Phase::reserved?"reserved":a.phase==Phase::running?"running":"cancelling"}});
 bool video,image,app_video,app_image;
 {std::lock_guard lock(mutex_);video=capability_report_.video();image=capability_report_.image();app_video=capability_report_.app_video();app_image=capability_report_.app_image();}
 return {{"protocol",1},{"type","heartbeat"},{"status",paused_?"paused":"ready"},{"priority",priority_name(priority_)},{"active",std::move(active)},
 {"available",{{"ram_mb",free.ram_mb},{"vram_mb",0},{"scratch_mb",free.scratch_mb},{"cpu_threads",free.cpu_threads}}},
 {"reserved",{{"ram_mb",used.ram_mb},{"scratch_mb",used.scratch_mb},{"cpu_threads",used.cpu_threads}}},
 {"capabilities",{{"conversion.video.h264",{{"verified",video},{"preference",pref_name(video_)}}},{"conversion.image.jpeg",{{"verified",image},{"preference",pref_name(image_)}}},{"media.video.renditions.v1",{{"verified",app_video},{"preference",pref_name(video_)}}},{"media.image.renditions.v1",{{"verified",app_image},{"preference",pref_name(image_)}}}}}};
}
std::string Worker::paired_token(){
 ProtectedPairingSecrets secrets(config_.root);
 NativePairingTransport transport;
 NativePairingKey key(config_.root);
 wchar_t name[256]{};DWORD length=256;
 require(GetComputerNameW(name,&length),"Read worker display name");
 auto already_paired=secrets.read(SecretSlot::credential).has_value();
 auto token=pairing_token(config_.coordinator,utf8(std::wstring(name,length)),secrets,transport,key,
  PairingEnvironment{[]{return utc_ms();},[this](unsigned ms){
    for(unsigned elapsed=0;elapsed<ms&&!stop_&&!exit_requested_;elapsed+=100)Sleep(std::min(100u,ms-elapsed));
   },
   [this]{return stop_.load()||exit_requested_.load();},
   [this](const std::string& code,unsigned seconds){
    {std::lock_guard lock(mutex_);pairing_user_code_=code;pairing_expires_utc_=utc_ms()+static_cast<std::int64_t>(seconds)*1000;}
    if(!paused_)report_state("paused","pairing required");state("PAIRING CODE "+code+" "+std::to_string(seconds));
   }});
 {std::lock_guard lock(mutex_);pairing_user_code_.clear();pairing_expires_utc_=0;}
 complete_owner_pairing(config_.root); // only explicit pairing_repair can change the public startup state
 if(!already_paired)safe_log("PAIRING APPROVED");
 return token;
}
void Worker::network(){
 safe_log("PROBE START");
 {std::lock_guard lock(mutex_);status_="PROBING";}
 try{
  auto probe=probe_ffmpeg(config_,[this]{return stop_||exit_requested_;},
   [this](std::string phase,unsigned current,unsigned total){probe_status_.phase(std::move(phase),current,total);},nullptr,
   [this](const std::string& record){try{log_.write(record);}catch(...){}});
  verified_video_=probe.video;verified_image_=probe.image;verified_media_video_=probe.media_video;verified_media_image_=probe.media_image;
  {
   std::lock_guard lock(mutex_);hardware_profiles_=std::move(probe.profiles);
   std::uint64_t runtime_vram=config_.budget.vram_mb;
   if(!config_.vram_override_set)runtime_vram=adaptive::profile_vram_capacity(hardware_profiles_);
   adaptive_scheduler_=std::make_unique<adaptive::Scheduler>(adaptive::Limits{config_.budget.cpu_threads,config_.budget.ram_mb,runtime_vram,config_.budget.scratch_mb,static_cast<std::uint32_t>(config_.max_jobs),adaptive::proven_nvenc_sessions(hardware_profiles_)},hardware_profiles_);
   capability_report_.probed(probe.video,probe.image,probe.media_video,probe.media_image,hardware_profiles_);
   gpu_recovery_.failed(std::move(probe.gpu_failures),monotonic_ms());
   for(const auto& line:gpu_recovery_.status(monotonic_ms()))safe_log("GPU RETRY "+line);
   probe_status_.finish();
   safe_log(config_.ffmpeg.empty()?"PROBE RESULT disabled":capability_report_.record());
   safe_log("PROBE DEVICES schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(hardware_profiles_)));
   std::set<std::string> logged_devices;
   for(const auto& profile:hardware_profiles_)if(profile.backend!=adaptive::Backend::cpu&&logged_devices.insert(profile.stable_id).second)
    safe_log("GPU DEVICE | "+adaptive::backend_token(profile.backend)+" GPU "+std::to_string(profile.backend_index)+" | "+profile.device_name+" | device_id="+profile.stable_id+" physical_id="+profile.physical_id);
   if(!config_.ffmpeg.empty())safe_log("PROBE SCOPE startup samples only; job support may vary");
   for(const auto& note:probe.fallback_notes)safe_log("PROBE FALLBACK "+note);
   if(!probe.reason.empty())safe_log("PROBE DETAIL "+probe.reason);
   {std::lock_guard report_lock(report_mutex_);readings_.encoders=capability_report_.encoders();if(readings_.encoders->empty())readings_.encoders="none";}
  }
  apply_preferences();
  recovery_=std::jthread([this]{recover_gpus();});
 }catch(const std::exception& e){
  {std::lock_guard lock(mutex_);probe_status_.finish();}
  state(std::string("STATE PROBE_FAILURE ")+e.what());
 }
 if(config_.coordinator.empty()){report_state("paused","pairing required");state("STATE UNCONFIGURED");while(!stop_)sleep_until_stop(stop_,500);return;}
 unsigned backoff=1000;
 while(!stop_){
 try{
  auto token=paired_token();if(stop_||exit_requested_)return;Http http;check_protocol(http.request("GET",config_.coordinator+"/v1/worker/protocol"));WebSocket socket(config_.coordinator+"/v1/worker/connect",token);SecureZeroMemory(token.data(),token.size());
  socket.send(json::serialize(json::object{{"protocol",1},{"type","hello"},{"agent_version","0.1.2"},{"state",heartbeat()}}));
  state(connected_once_?"STATE RECONNECTED":"STATE CONNECTED");report_connected_=true;last_contact_utc_=utc_ms();if(!paused_&&!exit_requested_)report_state("accepting","connected");connected_once_=true;backoff=1000;Tick next_heartbeat=0,next_results=0,next_status=monotonic_ms()+10000;
  while(!stop_){
   auto now=monotonic_ms();if(now>=next_heartbeat){
    // Specs for expired unstarted offers are not an unbounded in-memory queue.
    for(auto it=offered_.begin();it!=offered_.end();){if(it->second.offer.reserve_until<=now)it=offered_.erase(it);else ++it;}
    socket.send(json::serialize(heartbeat()));next_heartbeat=now+5000;
   }
   if(now>=next_results){send_pending([&](const std::string& body){socket.send(body);});next_results=now+2000;}
   if(now>=next_status){
    const auto active=active_count(),pending=store_.pending_count();auto record=connectionActivityRecord(active,pending);
    if(record!=last_activity_){if(active||pending||!last_activity_.empty())safe_log(record);last_activity_=record;}
    next_status=now+10000;
   }
   if(auto incoming=socket.receive()){auto value=parse(*incoming);message([&](const std::string& body){socket.send(body);},value.as_object());}
  }
 }catch(const PairingActionRequired& e){
  {std::lock_guard lock(mutex_);pairing_user_code_.clear();pairing_expires_utc_=0;}
  report_connected_=false;paused_=true;admission_.pause(true);try{pause(true);}catch(...){safe_log("STATE PAUSE_PERSISTENCE_FAILURE");}report_state("paused","pairing required");state(std::string("STATE ACTION ")+e.what());
  while(!stop_&&!exit_requested_)sleep_until_stop(stop_,500);
  return;
 }catch(const std::exception& e){
  report_connected_=false;if(!paused_)report_state("paused","coordinator offline");state(std::string("STATE OFFLINE ")+e.what());
  // Never bypass an explicit revocation by silently starting a new pairing.
  auto reason=std::string(e.what());if(reason.find("HTTP 401")!=std::string::npos||reason.find("HTTP 403")!=std::string::npos){paused_=true;admission_.pause(true);try{pause(true);}catch(...){safe_log("STATE PAUSE_PERSISTENCE_FAILURE");}report_state("paused","access revoked");state("STATE REVOKED");while(!stop_)sleep_until_stop(stop_,500);return;}
  sleep_until_stop(stop_,backoff+static_cast<unsigned>(GetTickCount64()%997));backoff=std::min(30000u,backoff*2);
 }
 }
}
void Worker::message(const Send& send,const json::object& m){
 if(number(m,"protocol",1)!=1)throw std::runtime_error("Unsupported coordinator protocol");auto type=text(m,"type",32);last_contact_utc_=utc_ms();
 if(type=="ping"){send(json::serialize(heartbeat()));return;}
 if(type=="offer"||type=="begin"){const auto pressure=capacity_.admit_offer(monotonic_ms());admission_.defer_new_starts(pressure.accepted?std::string{}:pressure.reason);}
 if(type=="result_ack"){auto id=text(m,"attempt_id",128);auto seq=number(m,"outbox_sequence",INT64_MAX);if(store_.acknowledge(static_cast<std::int64_t>(seq),id))safe_log("RESULT ACKNOWLEDGED "+id);return;}
 if(type=="offer"){
  std::string id;try{id=text(m,"attempt_id",128);auto spec=decode_offer(m,monotonic_ms(),config_.storage_hosts);
   if(store_.pending_count()>=512)throw std::runtime_error("Unacknowledged result backlog is full");
   if(auto s=store_.state(id);s&&*s!="reserved"&&*s!="running"){send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",false},{"reason","attempt_already_recorded"}}));return;}
   auto decision=admission_.offer(spec.offer,monotonic_ms(),available_resources(config_.root,config_.reserve_ram_mb));
   if(decision.accepted&&decision.reason!="duplicate"){
    try{if(!store_.reserve(id,spec.offer.job_id,spec.offer.fingerprint))throw std::runtime_error("journal attempt conflict");offered_.insert_or_assign(id,spec);}catch(...){admission_.cancel(id);throw;}
   }
   if(!decision.accepted)safe_log("JOB "+id+" DECLINED | "+decision.reason);
   send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",decision.accepted},{"reason",decision.reason}}));
  }catch(const std::exception&){if(safe_id(id)){send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",false},{"reason","invalid_or_unavailable"}}));safe_log("JOB "+id+" DECLINED | validation, dependency, capacity, or journal check failed");}else safe_log("OFFER DECLINED | invalid identity");}return;
 }
 auto id=text(m,"attempt_id",128);if(!safe_id(id))throw std::runtime_error("Invalid attempt identity");
 if(type=="begin"||type=="renew"){
  auto token=text(m,"lease_token",512);auto server_time=number(m,"server_time_ms",INT64_MAX),expires=number(m,"lease_until_ms",INT64_MAX);auto local=utc_ms();
  auto deadline=lease_deadline(static_cast<Tick>(server_time),static_cast<Tick>(expires),local,monotonic_ms());
  if(type=="renew"){admission_.renew(id,token,number(m,"sequence",UINT64_MAX),deadline,monotonic_ms());return;}
  auto started=admission_.begin(id,token,deadline,monotonic_ms());
  if(started==Start::started){
    auto i=offered_.find(id);if(i==offered_.end()){admission_.finish(id);store_.decline_reserved(id,"offer_spec_unavailable");throw std::runtime_error("No persisted spec for lease");}
   auto data=std::make_shared<TaskData>();data->spec=i->second;data->token=token;data->hard_deadline=monotonic_ms()+static_cast<Tick>(data->spec.timeout_ms);
   if(started==Start::started){
     if(!store_.start(id)){admission_.finish(id);store_.decline_reserved(id,"journal_start_failed");offered_.erase(i);throw std::runtime_error("No persisted job reservation for lease");}
    offered_.erase(i);auto running=std::make_unique<Running>();running->data=data;
    {std::lock_guard lock(report_mutex_);report_.start(id,job_subject(data->spec),utc_ms(),monotonic_ms());publish_report(config_.root/"worker-status.txt","");}
    safe_log("START "+id+" "+data->spec.offer.capability+" "+job_subject(data->spec)+" (outcome unknown)");
    {std::lock_guard lock(mutex_);running->thread=std::jthread([this,data]{execute(data);});running_.emplace(id,std::move(running));}
    report_status();
   }
  }
  send(json::serialize(json::object{{"protocol",1},{"type","begin_reply"},{"attempt_id",id},{"accepted",started!=Start::declined}}));return;
 }
 if(type=="cancel"){
  auto token=text(m,"lease_token",512);for(const auto& a:admission_.active())if(a.offer.attempt_id==id&&a.lease_token==token){admission_.cancel(id);std::lock_guard lock(mutex_);if(auto i=running_.find(id);i!=running_.end())i->second->data->cancel=true;}
  return;
 }
 throw std::runtime_error("Unknown coordinator message type");
}
void Worker::send_pending(const Send& send){
 for(const auto& p:store_.pending()){auto body=parse(p.body).as_object();body["outbox_sequence"]=p.sequence;send(json::serialize(body));}
}
void Worker::report_encoder_failure(const adaptive::Profile& profile){
 std::lock_guard lock(mutex_);
 if(!adaptive_scheduler_||!adaptive_scheduler_->demote_profile(profile))return;
 hardware_profiles_.erase(std::remove_if(hardware_profiles_.begin(),hardware_profiles_.end(),[&](const auto& p){
  return p.stable_id==profile.stable_id&&p.backend==profile.backend&&p.codec==profile.codec;
 }),hardware_profiles_.end());
 if(auto record=capability_report_.failed(profile,true))safe_log(*record);
 ++hardware_generation_;
 auto failures=gpu_recovery_.pending();failures.push_back({profile.device_name,profile.codec==adaptive::Codec::h264?"h264":"hevc","runtime encode or validation probe failed",profile.stable_id});
 gpu_recovery_.failed(std::move(failures),monotonic_ms());
 safe_log("PROBE DEVICES schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(hardware_profiles_)));
 try{
  auto state=parse(read_file(config_.root/"adaptive-hardware.json")).as_object();json::array profiles;
  for(const auto& p:hardware_profiles_)profiles.emplace_back(adaptive::profile_record(p));
  state["profiles"]=std::move(profiles);
  if(!state.contains("runtime_demotions"))state["runtime_demotions"]=json::array{};
  state["runtime_demotions"].as_array().emplace_back(adaptive::profile_record(profile));
  atomic_write(config_.root/"adaptive-hardware.json",json::serialize(state));
 }catch(...){safe_log("Adaptive demotion could not update its diagnostic profile file");}
}
void Worker::execute(std::shared_ptr<TaskData> data){
 const auto job_started=timing_wall_us();const auto cpu_started=thread_cpu_us();const auto main_thread=std::this_thread::get_id();StageTotals timings;
 const auto& s=data->spec;auto id=s.offer.attempt_id;auto workspace=config_.root/"jobs"/("job-"+id);json::object result{{"protocol",1},{"type","result"},{"attempt_id",id},{"job_id",s.offer.job_id},{"lease_token",data->token}};
 auto cancelled=[&]{return stop_||data->cancel||monotonic_ms()>=data->hard_deadline;};
 auto timing_sink=[&](const StageSample& sample){timings.add(sample,std::this_thread::get_id()!=main_thread);try{log_.write("JOB "+id+" "+stage_record(sample));}catch(...){}};
 JobOutcome outcome=JobOutcome::succeeded;std::string outcome_reason;
 auto phase=[&](const char* name,std::uint64_t total=0){data->progress.begin(name,monotonic_ms(),total);safe_log("JOB "+id+" STAGE | "+name+" | run "+std::to_string(data->progress.snapshot(monotonic_ms()).run));};
 auto bytes_progress=[&](std::uint64_t bytes){if(cancelled())throw std::runtime_error("Job cancelled or lease expired");data->progress.update(bytes,monotonic_ms());};
 try{
  {std::lock_guard lock(report_mutex_);report_.placement(id,"cpu");}
  phase("prepare source.bin workspace");
  if(std::filesystem::exists(workspace))throw std::runtime_error("Attempt workspace already exists; refusing unsafe reuse");std::filesystem::create_directories(workspace);
  Http http;phase("download source.bin",s.input_bytes);
  {StageTimer timing("download","source.bin",timing_sink,timing_wall_us,thread_cpu_us);if(offline_source_.empty())http.download(s.input_url,workspace/"source.bin",s.input_bytes,cancelled,bytes_progress);else{std::filesystem::copy_file(offline_source_,workspace/"source.bin");if(std::filesystem::file_size(workspace/"source.bin")!=s.input_bytes)throw std::runtime_error("Offline input size differs");bytes_progress(s.input_bytes);}}
  phase("verify source.bin sha256",s.input_bytes);
  {StageTimer timing("input_hash","source.bin",timing_sink,timing_wall_us,thread_cpu_us);if(sha256_file(workspace/"source.bin",bytes_progress)!=s.input_sha256)throw std::runtime_error("Input integrity check failed");}
  if(cancelled())throw std::runtime_error("Job cancelled before conversion");
  std::optional<std::pair<std::uint32_t,std::uint32_t>> input_dimensions;
  std::optional<VideoTiming> source_timing;
  std::string source_info;
  auto convert=[&](const std::vector<std::wstring>& arguments,const std::vector<std::wstring>& cpu_arguments,
                   const std::string& filename,unsigned seconds,bool hardware,std::stop_token pipeline_stop=std::stop_token{}){
   auto stopped=[&]{return cancelled()||pipeline_stop.stop_requested();};
   if(stopped())throw std::runtime_error("Job cancelled before conversion");
   const bool video=filename=="feed.mp4"||filename=="optimized.mp4";
   const auto codec=filename=="optimized.mp4"?adaptive::Codec::hevc:adaptive::Codec::h264;
   const auto key=filename=="optimized.mp4"?"optimized":filename=="poster.webp"?"poster":"feed";
   auto& progress=data->rendition_progress[filename=="optimized.mp4"?1:filename=="poster.webp"?2:0];
   const auto child_workspace=workspace/(filename+"-stage");std::filesystem::create_directory(child_workspace);
   StageClaim encode_claim{admission_,id,std::string("encode-")+key};
   std::optional<adaptive::Scheduler::Reservation> reservation;
   std::vector<adaptive::Profile> selected_profiles;std::optional<adaptive::Profile> failed_gpu;
   std::vector<std::wstring> selected_args=arguments;
   Resources cost=s.offer.resources;std::uint64_t validation_ram=cost.ram_mb,minimum_ram=0;
   bool source_timing_mismatch=false,coordinator_rate_mismatch=false;std::string retry_reason="GPU output failed validation";
   auto acquire=[&](bool cpu_only){
    cost=s.offer.resources;
    if(video&&data->compute_handoff){
     progress.begin("waiting encoder capacity "+filename,monotonic_ms());std::string previous;
     for(;;){
      if(stopped())throw std::runtime_error("Job cancelled while queued for encoder capacity");
      auto slot=reserve_hardware_profiles({codec},cost,id,encode_claim.name,cpu_only||!input_dimensions,
       input_dimensions?input_dimensions->first:0,input_dimensions?input_dimensions->second:0,minimum_ram);
      if(slot){reservation.emplace(std::move(slot));encode_claim.held=true;break;}
      const auto reason=admission_.queue_reason(id,encode_claim.name);if(previous!=reason){previous=reason;safe_log("JOB "+id+" QUEUED | "+encode_claim.name+" | "+reason);}
      Sleep(100);
     }
     selected_profiles=reservation->profiles();
     auto plan=rendition_plan(true,s.copy_audio,workspace,selected_profiles,source_info,source_timing?&*source_timing:nullptr);
     auto rendition=std::find_if(plan.begin(),plan.end(),[&](const auto& r){return r.filename==filename;});
     if(rendition==plan.end())throw std::runtime_error("Typed rendition unavailable");
     selected_args=rendition->arguments;hardware=rendition->hardware;
     for(const auto& p:selected_profiles)if(p.backend!=adaptive::Backend::cpu)failed_gpu=p;
    }else{
     selected_args=cpu_only?cpu_arguments:arguments;
     if(data->compute_handoff){wait_stage(id,encode_claim.name,cost,stopped);encode_claim.held=true;}
     hardware=false;
    }
    validation_ram=std::max(validation_ram,cost.ram_mb);
   };
   auto release_encode=[&]{reservation.reset();encode_claim.release();};
   acquire(false);
   auto run_encoder=[&](bool fallback){
    if(fallback){acquire(true);safe_log("JOB "+id+" RETRY | convert "+filename+" | "+retry_reason+"; CPU retry");}
    auto run=[&]{
     std::optional<adaptive::Device> device;std::uint64_t vram{};
     if(hardware){
      for(const auto& p:selected_profiles)if(p.backend!=adaptive::Backend::cpu&&reservation&&reservation->hardware_enabled(p)){
       for(const auto& d:enumerate_video_devices())if(adaptive::profile_current(p,d,config_.ffmpeg_sha256)){device=d;vram=p.vram_mb;break;}
      }
      if(!device)throw std::runtime_error("Proven GPU no longer current");
     }
     auto check_gpu=[&](std::uint32_t pid){if(device){auto sample=sample_gpu_memory(pid,*device);
      if(!adaptive::vram_envelope_ok(device->dedicated_vram_mb,vram,sample?std::optional<adaptive::GpuMemoryTotals>{{sample->child_bytes,sample->adapter_bytes}}:std::nullopt))throw std::runtime_error("GPU runtime memory envelope exceeded");}};
     check_gpu(0);const auto codec_name=codec==adaptive::Codec::hevc?"hevc":"h264";
     const auto encoder=hardware?adaptive::backend_token(device->family)+"/"+codec_name+" GPU "+std::to_string(device->backend_index):std::string("cpu/")+(video?codec_name:"image");
     progress.begin("convert "+filename,monotonic_ms());safe_log("JOB "+id+" STAGE | convert "+filename);
     StageTimer timing(filename=="poster.webp"?"poster":"transcode",filename,timing_sink,timing_wall_us,thread_cpu_us,true);
     const auto started=monotonic_ms();auto current=desktop_priority(priority_.load());
     Child child(config_.ffmpeg,encoder_progress_arguments(selected_args),child_workspace,cost,current);ChildTimingCapture capture(child,&timing);
     if(config_.observe_child)try{config_.observe_child(id,child.process_id());}catch(...){/* observational port cannot affect work */}
     {std::lock_guard lock(report_mutex_);safe_log(report_.encoder_line(id,encoder,filename,device?device->physical_id:"cpu",cost.ram_mb,readings_,false,device?device->name:"cpu"));}
     EncoderProgressFile encoder_progress(child_workspace/"process.log");
     for(;;){
      check_gpu(child.process_id());if(stopped()||monotonic_ms()-started>=static_cast<Tick>(seconds)*1000)throw std::runtime_error("Job cancelled or conversion timed out");
      if(auto code=child.exit_code()){if(*code){
       if(input_dimensions)if(auto reason=child_memory_failure(read_file(child_workspace/"process.log"),input_dimensions->first,input_dimensions->second,cost.ram_mb))throw std::runtime_error(*reason);
       throw std::runtime_error("Conversion process failed; see per-stage log");}break;}
      const auto latest=desktop_priority(priority_.load());if(latest!=current){child.priority(latest);current=latest;}
      encoder_progress.update(progress,monotonic_ms());std::error_code e;const auto bytes=std::filesystem::file_size(workspace/filename,e);
      if(!e&&bytes>s.output_max_bytes)throw std::runtime_error("Output exceeded declared byte limit");
      const auto log_bytes=std::filesystem::file_size(child_workspace/"process.log",e);if(!e&&log_bytes>5*1024*1024)throw std::runtime_error("Child output exceeded log budget");Sleep(100);
     }
     const auto peak_commit=child.peak_commit_mb();child.stop();encoder_progress.update(progress,monotonic_ms());
     safe_log("JOB "+id+" CHILD MEMORY | "+filename+" peak_commit_MiB="+std::to_string(peak_commit)+" hard_cap_MiB="+std::to_string(cost.ram_mb));
     {std::lock_guard lock(report_mutex_);safe_log(report_.encoder_line(id,encoder,filename,device?device->physical_id:"cpu",cost.ram_mb,readings_,true,device?device->name:"cpu"));}
    };
    try{run();}catch(...){release_encode();throw;} // child destructors have drained before release
    release_encode();
   };
   auto encode_with_growth=[&](bool fallback){
    for(unsigned retry=0;;++retry){
     try{run_encoder(fallback);return;}catch(const std::exception& e){
      if(!video||retry>=3||!insufficient_child_budget(e.what()))throw;
      const auto next=std::min(config_.budget.ram_mb,validation_ram*2);
      if(next<=validation_ram||next>stage_headroom(config_.root,config_.reserve_ram_mb).ram_mb)throw;
      minimum_ram=next;safe_log("JOB "+id+" RETRY | "+filename+" observed allocation failure; guarded child RAM="+std::to_string(next)+" MiB");
      acquire(fallback||!hardware);fallback=false;
     }
    }
   };
   auto validate=[&](const std::filesystem::path& path){
    if(stopped())return false;
    if(!video)return true;
    StageClaim validation{admission_,id,std::string("validate-")+key};
    auto limits=s.offer.resources;limits.ram_mb=validation_ram;
    if(data->compute_handoff){wait_stage(id,validation.name,limits,stopped);validation.held=true;}
    progress.begin("validate "+filename,monotonic_ms());
    StageTimer timing("validation",filename,timing_sink,timing_wall_us,thread_cpu_us,true);
    // A successful output is not invalid because one slow decoder needs >30s.
    // The original job/lease deadline and child resource limits still bind.
    run_child_probe(config_.ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",path.wstring(),L"-threads",L"1",L"-f",L"null",L"-"},child_workspace,stopped,limits,nullptr,UINT64_MAX,std::max<Tick>(1,data->hard_deadline-monotonic_ms()),input_dimensions?input_dimensions->first:0,input_dimensions?input_dimensions->second:0,&timing);
    if(input_dimensions&&!adaptive::video_geometry_valid(read_file(child_workspace/"process.log"),codec==adaptive::Codec::hevc?"hevc":"h264",input_dimensions->first,input_dimensions->second)){retry_reason="Output geometry or codec differs from source";return false;}
    if(source_timing){
     StageTimer packet_timing("output_timing",filename,timing_sink,timing_wall_us,thread_cpu_us,true);
     run_child_probe(config_.ffmpeg,video_timing_arguments(path),child_workspace,stopped,limits,nullptr,UINT64_MAX,std::max<Tick>(1,data->hard_deadline-monotonic_ms()),0,0,&packet_timing);
     auto output=video_timing(read_file(child_workspace/"process.log",5*1024*1024));
     if(!output||!same_video_timing(*source_timing,*output)){source_timing_mismatch=true;retry_reason="Output video presentation timeline differs from source";return false;}
     if(source_timing->codec_rate_hint()&&!source_timing->reported_rate.empty()&&!output->reported_rate.empty()&&source_timing->reported_rate!=output->reported_rate){coordinator_rate_mismatch=true;retry_reason="Legacy coordinator nominal-rate metadata differs; presentation timeline passed";return false;}
    }
    return true;
   };
   const bool initial_hardware=hardware;
   if(initial_hardware){
    adaptive::run_with_cpu_fallback(workspace/filename,true,stopped,[&](bool use_hardware){
     if(!use_hardware&&failed_gpu&&!source_timing_mismatch&&!coordinator_rate_mismatch&&!insufficient_child_budget(retry_reason))report_encoder_failure(*failed_gpu);
     try{encode_with_growth(!use_hardware);}catch(const std::exception& e){retry_reason=e.what();throw;}
    },validate);
   }else{encode_with_growth(false);if(!validate(workspace/filename))throw std::runtime_error(retry_reason);}
   progress.finish(true,monotonic_ms());capacity_.validated();
   if(hardware&&failed_gpu){std::lock_guard lock(mutex_);if(gpu_capacity_.contains(failed_gpu->stable_id))gpu_capacity_.at(failed_gpu->stable_id)->validated();}
  };
  auto upload=[&](const std::string& filename,const std::string& url,const std::string& mime,std::stop_token pipeline_stop=std::stop_token{}){
   auto transfer_cancelled=[&]{return cancelled()||pipeline_stop.stop_requested();};
   auto transfer_bytes=[&](std::uint64_t count){if(transfer_cancelled())throw std::runtime_error("Job cancelled during transfer");data->transfer_progress.update(count,monotonic_ms());};
   auto transfer_phase=[&](const std::string& name,std::uint64_t total){data->transfer_progress.begin(name,monotonic_ms(),total);safe_log("JOB "+id+" STAGE | "+name+" | run "+std::to_string(data->transfer_progress.snapshot(monotonic_ms()).run));};
   if(transfer_cancelled())throw std::runtime_error("Job cancelled before transfer");
   StageClaim transfer_claim{admission_,id,"transfer"};
   if(data->compute_handoff){wait_stage(id,transfer_claim.name,s.offer.resources,transfer_cancelled);transfer_claim.held=true;}
   auto output=workspace/filename;auto bytes=std::filesystem::file_size(output);if(!bytes||bytes>s.output_max_bytes)throw std::runtime_error("Invalid output size");
   auto label="verify "+filename+" SHA-256";transfer_phase(label,bytes);std::string hash;
   {StageTimer timing("output_hash",filename,timing_sink,timing_wall_us,thread_cpu_us);hash=sha256_file(output,transfer_bytes);}
   label="upload "+filename;transfer_phase(label,bytes);Http transfer_http;
   {StageTimer timing("upload",filename,timing_sink,timing_wall_us,thread_cpu_us);if(offline_output_.empty())transfer_http.upload(url,output,mime,transfer_cancelled,transfer_bytes);else{auto target=offline_output_/id/filename;std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(output,target);if(sha256_file(target)!=hash)throw std::runtime_error("Offline delivered file integrity differs");transfer_bytes(bytes);}}
   data->transfer_progress.finish(true,monotonic_ms());
   std::error_code e;if(std::filesystem::remove(output,e))admission_.release_scratch(id,s.output_max_bytes/(1024*1024));
   return json::object{{"sha256",hash},{"bytes",bytes},{"content_type",mime}};
  };
  if(!s.artifacts.empty()){
   const bool video=s.offer.capability=="media.video.renditions.v1";bool poster_ready=false;
   if(video){
    phase("inspect source video timing");
    {StageTimer timing("input_timing","source.bin",timing_sink,timing_wall_us,thread_cpu_us,true);
     run_child_probe(config_.ffmpeg,video_timing_arguments(workspace/"source.bin"),workspace,cancelled,s.offer.resources,nullptr,UINT64_MAX,30000,0,0,&timing);
     source_timing=video_timing(read_file(workspace/"process.log",5*1024*1024));}
    if(!source_timing)throw std::runtime_error("Source video timestamp evidence is missing or invalid; refusing unvalidated output");
    try{
     StageTimer timing("input_inspection_poster","source.bin+poster.webp",timing_sink,timing_wall_us,thread_cpu_us,true);
     input_dimensions=inspect_video_dimensions(config_.ffmpeg,workspace/"source.bin",workspace,cancelled,s.offer.resources,&timing,&source_info,true);
     poster_ready=inspection_poster_reusable(source_info)&&std::filesystem::is_regular_file(workspace/"poster.webp")&&std::filesystem::file_size(workspace/"poster.webp")>0;
    }catch(const std::exception&){input_dimensions.reset();}
    if(!input_dimensions&&source_timing->width&&source_timing->height)input_dimensions=std::pair{source_timing->width,source_timing->height};
    if(!poster_ready){std::error_code error;std::filesystem::remove(workspace/"poster.webp",error);}
   }
   if(!admission_.release_compute(id))throw std::runtime_error("Compute handoff unavailable after input drain");
   data->compute_handoff=true;data->progress.finish(true,monotonic_ms());
   auto plan=rendition_plan(video,s.copy_audio,workspace);
   if(poster_ready)std::rotate(plan.begin(),plan.end()-1,plan.end());
   std::vector<adaptive::Profile> cpu_profiles;{std::lock_guard lock(mutex_);for(const auto& profile:hardware_profiles_)if(profile.backend==adaptive::Backend::cpu)cpu_profiles.push_back(profile);}
   const auto cpu_plan=rendition_plan(video,s.copy_audio,workspace,cpu_profiles,source_info,source_timing?&*source_timing:nullptr);
   std::atomic_size_t remaining{plan.size()};
   auto prepared=[&]{if(remaining.fetch_sub(1)==1){
    std::error_code e;if(std::filesystem::remove(workspace/"source.bin",e))admission_.release_scratch(id,s.input_bytes/(1024*1024));
   }};
   auto metadata=run_rendition_pipeline(plan,[&](const Rendition& rendition,std::stop_token stop){
    if(rendition.filename=="poster.webp"&&poster_ready){
     if(cancelled()||stop.stop_requested())throw std::runtime_error("Job cancelled before poster reuse");
     safe_log("JOB "+id+" STAGE | reuse inspected poster.webp");prepared();return;
    }
    const auto cpu=std::find_if(cpu_plan.begin(),cpu_plan.end(),[&](const auto& value){return value.role==rendition.role;});
    if(cpu==cpu_plan.end())throw std::runtime_error("CPU fallback rendition missing");
    convert(rendition.arguments,cpu->arguments,rendition.filename,rendition.timeout_seconds,rendition.hardware,stop);prepared();
   },[&](const Rendition& rendition,std::stop_token stop){
    const auto target=std::find_if(s.artifacts.begin(),s.artifacts.end(),[&](const auto& t){return t.role==rendition.role;});
    if(target==s.artifacts.end())throw std::runtime_error("Rendition target missing");
    return upload(rendition.filename,target->url,rendition.mime,stop);
   },preparation_parallelism_);
   json::object outputs;for(std::size_t i=0;i<plan.size();++i)outputs[plan[i].role]=std::move(metadata[i]);
   result["outputs"]=std::move(outputs);
  }else{
   convert(ffmpeg_arguments(s,workspace),ffmpeg_arguments(s,workspace),output_name(s),static_cast<unsigned>(s.timeout_ms/1000),false);
   if(admission_.release_compute(id))data->compute_handoff=true;
   result["output"]=upload(output_name(s),s.output_url,s.offer.capability=="conversion.video.h264"?"video/mp4":"image/jpeg");
  }
  if(cancelled())throw std::runtime_error("Job cancelled before completion");
  result["status"]="succeeded";
 }catch(const std::exception& e){for(auto& progress:data->rendition_progress)progress.finish(false,monotonic_ms());data->progress.finish(false,monotonic_ms());data->transfer_progress.finish(false,monotonic_ms());result["status"]="failed";result["reason"]=std::string(e.what());const bool cancelled=data->cancel||stop_||monotonic_ms()>=data->hard_deadline;outcome=cancelled?JobOutcome::cancelled:JobOutcome::failed;outcome_reason=e.what();}
 // Media is not needed after an attempt: authoritative output is in object
 // storage, and any accepted completion is durably represented in the outbox.
 try{store_.result(id,json::serialize(result));safe_log("RESULT PENDING ACK "+id+" "+std::string(result.at("status").as_string()));}catch(...){paused_=true;admission_.pause(true);safe_log("STATE JOURNAL_FAILURE");report_state("paused","journal failure");}
 std::error_code error;
 try{
  if(result.at("status").as_string()!="succeeded"&&std::filesystem::exists(workspace/"process.log")){
   auto logs=config_.root/"logs";std::filesystem::create_directories(logs);
   auto source=workspace/"process.log";auto length=std::filesystem::file_size(source);
   if(length<=5*1024*1024)std::filesystem::copy_file(source,logs/("job-"+id+".log"),std::filesystem::copy_options::overwrite_existing);
   // Only 20 bounded process logs are retained across completed jobs.
   std::vector<std::filesystem::directory_entry> entries;
   for(const auto& entry:std::filesystem::directory_iterator(logs))if(entry.is_regular_file()&&entry.path().extension()==".log"&&entry.path().filename().string().starts_with("job-"))entries.push_back(entry);
   std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
   for(std::size_t i=0;i+20<entries.size();++i)std::filesystem::remove(entries[i].path(),error);
  }
 }catch(...){safe_log("Could not preserve child diagnostic log");}
 std::filesystem::remove_all(workspace,error);admission_.finish(id);data->progress.finish(result.at("status").as_string()=="succeeded",monotonic_ms());data->done=true;
 const auto cpu_finished=thread_cpu_us();
 try{log_.write("JOB "+id+" "+timings.record(timing_wall_us()-job_started,cpu_started&&cpu_finished?std::optional<std::uint64_t>{*cpu_finished-*cpu_started}:std::nullopt));}catch(...){}
 {std::lock_guard lock(report_mutex_);if(report_.finish(id,outcome,outcome_reason,utc_ms(),monotonic_ms())){safe_log(report_.completion());publish_report(config_.root/"worker-history.txt",status_frame(report_.history()));}}
 report_status();
}
void Worker::watchdog(){
 Tick next_progress=0,next_inventory=0,next_capacity=0,next_capacity_log=0,next_adaptive_check=monotonic_ms()+60000;
 while(!stop_){
 try{
  {std::lock_guard lock(mutex_);if(auto record=probe_status_.line())safe_log(*record);}
  for(const auto& e:admission_.expire(monotonic_ms())){
   if(e.stop_process){std::lock_guard lock(mutex_);if(auto i=running_.find(e.attempt_id);i!=running_.end())i->second->data->cancel=true;}
   else store_.result(e.attempt_id,json::serialize(json::object{{"protocol",1},{"type","result"},{"attempt_id",e.attempt_id},{"status","abandoned"},{"reason","offer_expired"}}));
  }
  report_status();
  auto now=monotonic_ms();
  if(now>=next_capacity){
   auto snapshot=sample_runtime_pressure(config_.root);if(config_.capacity_foreground)snapshot.host.foreground=config_.capacity_foreground();capacity_.observe(snapshot.host,now);const auto starts=capacity_.admit_offer(now);admission_.defer_new_starts(starts.accepted?std::string{}:starts.reason);
   auto lines=admission_.capacity_lines();
   {std::lock_guard lock(mutex_);pressure_=snapshot;if(adaptive_scheduler_){
    std::map<std::string,unsigned> proven;for(const auto& p:hardware_profiles_)if(p.backend!=adaptive::Backend::cpu){auto [i,inserted]=proven.emplace(p.stable_id,p.proven_sessions);if(!inserted)i->second=std::min(i->second,p.proven_sessions);}
    for(const auto& [id,ceiling]:proven){
     auto& control=gpu_capacity_[id];if(!control)control=std::make_unique<CapacityControl>(ceiling,10000,"gpu_engine");else control->ceiling(ceiling);
     std::optional<double> busy;const auto luid=id.find(":luid:");if(luid!=std::string::npos){auto reading=snapshot.gpu_busy.find(id.substr(luid+6));if(reading!=snapshot.gpu_busy.end())busy=reading->second;}
     control->observe(Pressure{busy,0.0,snapshot.host.foreground,0.0},now);adaptive_scheduler_->set_device_concurrency_limit(id,control->stage_limit(now));
     auto profile=std::find_if(hardware_profiles_.begin(),hardware_profiles_.end(),[&](const auto& p){return p.stable_id==id;});
     lines.push_back(profile->device_name+" busiest engine "+(busy?std::to_string(static_cast<int>(std::lround(*busy)))+"%":"unknown")+" sampled; "+control->reason());
     if(luid!=std::string::npos){auto bytes=snapshot.dedicated_vram_bytes.find(id.substr(luid+6));lines.push_back("  dedicated VRAM in use "+(bytes==snapshot.dedicated_vram_bytes.end()?std::string{"unknown"}:std::to_string((bytes->second+1048575)/1048576)+" MiB sampled"));}
    }
    const auto cards=adaptive_scheduler_->capacity_lines();lines.insert(lines.end(),cards.begin(),cards.end());
   }}
   lines.push_back("capacity stages "+std::to_string(admission_.stage_count())+"/"+std::to_string(capacity_.stage_limit(now))+" trial; qualified "+std::to_string(capacity_.qualified()));
   lines.push_back("target 90% sustained; "+capacity_.reason());
   auto percent=[](std::optional<double> value){return value?std::to_string(static_cast<int>(std::lround(*value)))+"%":"unknown";};
   lines.push_back("measured CPU "+percent(snapshot.host.cpu_percent)+" RAM "+percent(snapshot.host.ram_percent)+" disk "+percent(snapshot.host.disk_percent));
   std::vector<ConsoleLine> frame;for(const auto& line:lines)for(std::size_t at=0;at<line.size();at+=76)frame.push_back({"  "+line.substr(at,76),ConsoleColor::white});
   {std::lock_guard lock(report_mutex_);publish_report(config_.root/"worker-capacity.txt",status_frame(frame));}
   if(now>=next_capacity_log){for(const auto& line:lines)safe_log("CAPACITY | "+line);next_capacity_log=now+30000;}
   next_capacity=now+1000;
  }
  if(ffmpeg_hash_changed_.exchange(false)){
   {std::lock_guard lock(mutex_);++hardware_generation_;hardware_profiles_.clear();if(adaptive_scheduler_)adaptive_scheduler_->invalidate_all();if(auto record=capability_report_.disabled())safe_log(*record);safe_log("PROBE DEVICES schedulable GPUs=0");}
   verified_video_=false;verified_image_=false;verified_media_video_=false;verified_media_image_=false;apply_preferences();
   safe_log("ADAPTIVE INVALIDATION: approved FFmpeg changed or became unreadable; converter capabilities disabled until a fresh probe");
  }
  if(now>=next_inventory){
   try{auto inventory=enumerate_video_devices(false);std::vector<StatusCard> cards;for(const auto& d:inventory)if(adaptive::pci_instance_id(d.physical_id))cards.push_back({d.physical_id,d.name});std::lock_guard lock(report_mutex_);readings_.cards=std::move(cards);}catch(...){std::lock_guard lock(report_mutex_);readings_.cards.reset();}
   next_inventory=now+60000;
  }
  if(now>=next_adaptive_check){
   std::string invalidation;
   if(config_.ffmpeg.empty()||config_.ffmpeg_sha256.size()!=64){next_adaptive_check=now+60000;}
   else{
    try{
     const auto devices=enumerate_video_devices();bool changed=false;
     {std::lock_guard lock(mutex_);const auto old=hardware_profiles_.size();hardware_profiles_.erase(std::remove_if(hardware_profiles_.begin(),hardware_profiles_.end(),[&](const auto& profile){
       if(profile.backend==adaptive::Backend::cpu)return false;
       const auto device=std::find_if(devices.begin(),devices.end(),[&](const auto& current){return current.stable_id==profile.stable_id;});
       const bool stale=device==devices.end()||!adaptive::profile_current(profile,*device,config_.ffmpeg_sha256);
       if(stale&&adaptive_scheduler_)adaptive_scheduler_->demote_profile(profile);
       return stale;
      }),hardware_profiles_.end());changed=old!=hardware_profiles_.size();if(changed)++hardware_generation_;}
     if(changed){invalidation="adapter identity, driver version, or device capacity changed; stale hardware profiles removed; startup CPU evidence unchanged";}
    }catch(...){
     bool removed=false;{std::lock_guard lock(mutex_);const auto old=hardware_profiles_.size();hardware_profiles_.erase(std::remove_if(hardware_profiles_.begin(),hardware_profiles_.end(),[](const auto& profile){return profile.backend!=adaptive::Backend::cpu;}),hardware_profiles_.end());removed=old!=hardware_profiles_.size();if(removed)++hardware_generation_;}
     if(removed){std::lock_guard lock(mutex_);if(adaptive_scheduler_)adaptive_scheduler_->invalidate_hardware();invalidation="adapter inventory unavailable; hardware profiles removed; startup CPU evidence unchanged";}
    }
   }
   if(!invalidation.empty()){
    {std::lock_guard lock(mutex_);if(auto record=capability_report_.changed(hardware_profiles_))safe_log(*record);safe_log("PROBE DEVICES schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(hardware_profiles_)));}
    safe_log("ADAPTIVE INVALIDATION: "+invalidation);
    try{
     std::lock_guard lock(mutex_); // Serialize diagnostic writes with runtime demotion.
     auto state=parse(read_file(config_.root/"adaptive-hardware.json")).as_object();json::array current_profiles,reasons;
     for(const auto& profile:hardware_profiles_)current_profiles.emplace_back(adaptive::profile_record(profile));
     if(auto existing=state.find("unavailable_reasons");existing!=state.end())reasons=existing->value().as_array();
     reasons.emplace_back(invalidation);state["profiles"]=std::move(current_profiles);state["unavailable_reasons"]=std::move(reasons);atomic_write(config_.root/"adaptive-hardware.json",json::serialize(state));
    }catch(...){safe_log("Adaptive invalidation could not update its diagnostic profile file");}
   }
   next_adaptive_check=now+60000;
  }
  if(now>=next_progress){
   std::vector<std::shared_ptr<TaskData>> active;{std::lock_guard lock(mutex_);for(const auto& [_,r]:running_)if(!r->data->done)active.push_back(r->data);}
   for(const auto& data:active)for(auto* counter:{&data->progress,&data->transfer_progress,&data->rendition_progress[0],&data->rendition_progress[1],&data->rendition_progress[2]}){auto p=counter->snapshot(now);if(p.stage.empty()||p.finished)continue;
    if(auto line=progress_log_.line(data->spec.offer.attempt_id,job_subject(data->spec),p))safe_log(*line);
   }
   {std::lock_guard lock(mutex_);std::lock_guard report_lock(report_mutex_);if(capability_report_.processing_ready()){readings_.encoders=capability_report_.encoders();if(readings_.encoders->empty())readings_.encoders="none";}readings_.gpu_recovery=gpu_recovery_.status(now);}
   next_progress=now+2000;
  }
  std::vector<std::pair<std::string,std::unique_ptr<Running>>> finished;{std::lock_guard lock(mutex_);for(auto i=running_.begin();i!=running_.end();){if(i->second->data->done){finished.emplace_back(i->first,std::move(i->second));i=running_.erase(i);}else ++i;}}
  for(const auto& item:finished)progress_log_.forget(item.first);finished.clear();
 }catch(const std::exception& e){paused_=true;admission_.pause(true);state(std::string("STATE WATCHDOG_FAILURE ")+e.what());try{report_state("paused","watchdog failure");}catch(...){}}
 sleep_until_stop(stop_,100);
 }
}
}
