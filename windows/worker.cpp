#include <cctype>
#include "worker.hpp"
#include "ow/media_renditions.hpp"
#include "ow/progress.hpp"
#include "ow/encoder_run.hpp"
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
std::string backend_name(adaptive::Backend backend){switch(backend){case adaptive::Backend::nvidia_nvenc:return "NVIDIA NVENC";case adaptive::Backend::intel_qsv:return "Intel QSV";case adaptive::Backend::amd_amf:return "AMD AMF";default:return "CPU";}}
void sleep_until_stop(const std::atomic_bool& stop,unsigned milliseconds){for(unsigned elapsed=0;elapsed<milliseconds&&!stop;elapsed+=100)Sleep(std::min(100u,milliseconds-elapsed));}
void check_protocol(const Response& r){
 if(r.status==404)throw std::runtime_error("Coordinator worker endpoints are not deployed; no pairing performed");
 if(r.status!=200)throw std::runtime_error("Coordinator protocol check returned HTTP "+std::to_string(r.status));
 auto v=parse(r.body);if(number(v.as_object(),"protocol",1)!=1)throw std::runtime_error("Coordinator worker protocol is incompatible");
}
struct ProbeRun {std::uint64_t elapsed_ms{},peak_vram_mb{};bool estimated_vram{};};
ProbeRun run_child_probe(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& workspace,const std::function<bool()>& cancelled,Resources limits,const adaptive::Device* hardware_device=nullptr,std::uint64_t vram_limit_mb=UINT64_MAX){
 const auto started=monotonic_ms();
 if(!limits.ram_mb||!limits.cpu_threads||!limits.scratch_mb)throw std::runtime_error("Configured/live resource budget cannot run converter probe");
 Child child(exe,args,workspace,limits,Priority::low);auto until=started+30000;std::uint64_t peak_vram{};bool estimated_vram=false;
 for(;;){
  if(cancelled&&cancelled())throw std::runtime_error("Capability probe cancelled");
  if(hardware_device){
   auto sample=sample_gpu_memory(child.process_id(),*hardware_device);
    if(sample)peak_vram=std::max(peak_vram,sample->child_bytes);
    else{
     estimated_vram=true;
     vram_limit_mb=std::min(vram_limit_mb,adaptive::vram_budget(hardware_device->dedicated_vram_mb,{},vram_limit_mb).mb);
    }
    if(peak_vram/(1024*1024)+(peak_vram%(1024*1024)!=0)>vram_limit_mb||!adaptive::vram_envelope_ok(hardware_device->dedicated_vram_mb,vram_limit_mb,
       sample?std::optional<adaptive::GpuMemoryTotals>{{sample->child_bytes,sample->adapter_bytes}}:std::nullopt))
     throw std::runtime_error("GPU memory exceeded conservative probe reservation; process tree aborted");
  }
  if(auto code=child.exit_code()){
   if(*code){std::string detail;try{detail=read_file(workspace/"process.log");if(detail.size()>2048)detail.erase(0,detail.size()-2048);}
    catch(...){}throw std::runtime_error("FFmpeg functional probe failed with exit code "+std::to_string(*code)+(detail.empty()?std::string{}:": "+detail));}
   break;
  }
  if(monotonic_ms()>=until)throw std::runtime_error("FFmpeg functional probe timed out");Sleep(100);
 }
 return {static_cast<std::uint64_t>(std::max<Tick>(1,monotonic_ms()-started)),peak_vram/(1024*1024)+(peak_vram%(1024*1024)!=0),estimated_vram};
}
std::optional<std::pair<std::uint32_t,std::uint32_t>> inspect_video_dimensions(const std::filesystem::path& exe,
 const std::filesystem::path& input,const std::filesystem::path& workspace,const std::function<bool()>& cancelled,Resources limits){
 run_child_probe(exe,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-protocol_whitelist",L"file",L"-noautorotate",L"-i",input.wstring(),L"-map",L"0:v:0",L"-frames:v",L"1",L"-f",L"null",L"-"},workspace,cancelled,limits);
 const auto log=read_file(workspace/"process.log");
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
 Config c;c.root=root;auto obj=parse(read_file(root/"worker.json")).as_object();if(number(obj,"schema",1)!=1)throw std::runtime_error("Unsupported configuration version");c.coordinator=coordinator_origin(text(obj,"coordinator_url",1024));
 for(const auto& v:obj.at("storage_hosts").as_array()){auto s=std::string(v.as_string());std::transform(s.begin(),s.end(),s.begin(),[](unsigned char x){return static_cast<char>(std::tolower(x));});if(!allowed_url("https://"+s+"/",{s}))throw std::runtime_error("Invalid approved storage host");c.storage_hosts.push_back(s);}
 c.ffmpeg=wide(text(obj,"ffmpeg_path",32700));c.ffmpeg_sha256=text(obj,"ffmpeg_sha256",64);
 const auto& limits=obj.at("limits").as_object();const auto vram_override=limits.find("vram_mb");c.vram_override_set=vram_override!=limits.end();c.budget={number(limits,"ram_mb",1024*1024),vram_override==limits.end()?0:number(limits,"vram_mb",UINT64_MAX),number(limits,"scratch_mb",1024*1024),number(limits,"cpu_threads",64)};c.reserve_ram_mb=number(limits,"reserve_ram_mb",1024*1024);c.max_jobs=static_cast<std::size_t>(number(limits,"max_jobs",64));
 if(c.budget.ram_mb<256||!c.budget.cpu_threads||!c.max_jobs||!c.budget.scratch_mb)throw std::runtime_error("Invalid resource budget");
 return c;
}
Probe probe_ffmpeg(const Config& c,const std::function<bool()>& cancelled){
 Probe p;std::vector<adaptive::Device> inventory;
  try{inventory=enumerate_video_devices(!(c.vram_override_set&&c.budget.vram_mb==0));}catch(const std::exception& e){p.reason="Adapter inventory unavailable at setup start: "+std::string(e.what());}
 if(c.ffmpeg.empty()){
  p.reason+="; NoConversion: converter capabilities disabled; detected adapters="+std::to_string(inventory.size())+"; configured_global_VRAM_override_MB="+std::to_string(c.budget.vram_mb);
   const auto live=available_resources(c.root,c.reserve_ram_mb);MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);GlobalMemoryStatusEx(&memory);
   ULARGE_INTEGER disk_free{},disk_total{},disk_total_free{};GetDiskFreeSpaceExW(c.root.c_str(),&disk_free,&disk_total,&disk_total_free);
  json::array devices,reasons;
  reasons.emplace_back("NoConversion selected; converter capabilities remain disabled");
  for(const auto& d:inventory){p.reason+=" ["+d.name+" id="+d.stable_id+" dedicated_VRAM_MB="+std::to_string(d.dedicated_vram_mb)+"; encoder mapping="+(d.backend_identity_verified?"verified":"unavailable")+"]";
   devices.emplace_back(json::object{{"device_id",d.stable_id},{"name",d.name},{"backend",static_cast<int>(d.family)},{"driver_version",d.driver_version},{"dedicated_vram_mb",d.dedicated_vram_mb},{"adapter_index",d.adapter_index},{"backend_index",d.backend_index},{"backend_identity_verified",d.backend_identity_verified},{"driver_healthy",d.driver_healthy}});
  }
   try{std::filesystem::create_directories(c.root);atomic_write(c.root/"adaptive-hardware.json",json::serialize(json::object{{"schema",1},{"ffmpeg_sha256",""},{"processor",processor_model()},{"logical_cpu_count",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},{"total_ram_mb",memory.ullTotalPhys/(1024*1024)},{"available_ram_mb",live.ram_mb},{"total_disk_mb",disk_total.QuadPart/(1024*1024)},{"available_scratch_mb",live.scratch_mb},{"limits",{{"cpu_threads",c.budget.cpu_threads},{"ram_mb",c.budget.ram_mb},{"vram_mb_override",c.budget.vram_mb},{"vram_mb_override_set",c.vram_override_set},{"scratch_mb",c.budget.scratch_mb},{"max_jobs",c.max_jobs}}},{"devices",std::move(devices)},{"profiles",json::array{}},{"unavailable_reasons",std::move(reasons)}}));}catch(...){p.reason+="; could not persist NoConversion inventory";}
  return p;
 }
 auto workspace=c.root/"probes"/("probe-"+random_hex(8));
 try{
 auto live=available_resources(c.root,c.reserve_ram_mb);
 Resources probe_limits{std::min(c.budget.ram_mb,live.ram_mb),0,std::min(c.budget.scratch_mb,live.scratch_mb),std::min(c.budget.cpu_threads,live.cpu_threads)};
  auto run_probe=[&](const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& path,const std::function<bool()>& cancel,const adaptive::Device* device=nullptr,std::uint64_t vram_cap=UINT64_MAX){
   return run_child_probe(exe,args,path,cancel,probe_limits,device,vram_cap);
 };
 if(!c.ffmpeg.is_absolute()||!std::filesystem::is_regular_file(c.ffmpeg))throw std::runtime_error("Configured FFmpeg binary is missing");
 if(c.ffmpeg_sha256.size()!=64||sha256_file(c.ffmpeg)!=c.ffmpeg_sha256)throw std::runtime_error("FFmpeg SHA-256 differs from approved configuration");
 std::filesystem::create_directories(workspace);
 run_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",L"color=c=black:s=32x32:r=1",L"-t",L"1",L"-an",L"-c:v",L"libx264",L"-threads",L"1",L"-pix_fmt",L"yuv420p",(workspace/"sample.mp4").wstring()},workspace,cancelled);
 std::filesystem::copy_file(workspace/"sample.mp4",workspace/"source.bin");
 JobSpec spec;spec.offer.capability="conversion.video.h264";spec.offer.resources={512,0,128,1};run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.video=std::filesystem::file_size(workspace/"result.mp4")>0;
 spec.offer.capability="conversion.image.jpeg";run_probe(c.ffmpeg,ffmpeg_arguments(spec,workspace),workspace,cancelled);p.image=std::filesystem::file_size(workspace/"result.jpg")>0;
 try{for(const auto& rendition:rendition_plan(true,false,workspace)){run_probe(c.ffmpeg,rendition.arguments,workspace,cancelled);if(!std::filesystem::file_size(workspace/rendition.filename))throw std::runtime_error("App video probe produced no output");run_probe(c.ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(workspace/rendition.filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace,cancelled);}p.media_video=true;}catch(const std::exception&){p.media_video=false;}
 std::filesystem::copy_file(workspace/"result.jpg",workspace/"source.bin",std::filesystem::copy_options::overwrite_existing);
 try{for(const auto& rendition:rendition_plan(false,false,workspace)){run_probe(c.ffmpeg,rendition.arguments,workspace,cancelled);if(!std::filesystem::file_size(workspace/rendition.filename))throw std::runtime_error("App image probe produced no output");run_probe(c.ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(workspace/rendition.filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace,cancelled);}p.media_image=true;}catch(const std::exception&){p.media_image=false;}
 std::vector<std::string> unavailable;std::map<std::string,std::string> budget_sources;Resources headroom{};std::vector<adaptive::Device> devices;
 std::uint64_t total_ram_mb{},total_disk_mb{};
 if(p.media_video){
  const auto tune_source=workspace/"adaptive-source.mp4";
  run_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",L"testsrc2=size=640x360:rate=24",L"-f",L"lavfi",L"-i",L"sine=frequency=880:sample_rate=48000",L"-t",L"3",L"-frames:v",L"72",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",L"-pix_fmt",L"yuv420p",L"-c:a",L"aac",(tune_source).wstring()},workspace,cancelled);
  std::filesystem::copy_file(tune_source,workspace/"source.bin",std::filesystem::copy_options::overwrite_existing);
  devices=std::move(inventory);adaptive::Device cpu{"cpu",adaptive::Backend::cpu,"",0,true,0,"CPU",0,true};
  for(const auto backend:{adaptive::Backend::nvidia_nvenc,adaptive::Backend::intel_qsv,adaptive::Backend::amd_amf})
   if(std::none_of(devices.begin(),devices.end(),[&](const auto& device){return device.family==backend;}))unavailable.emplace_back(backend_name(backend)+" adapter not detected; verified CPU fallback remains active");
  MEMORYSTATUSEX physical{};physical.dwLength=sizeof(physical);if(GlobalMemoryStatusEx(&physical))total_ram_mb=physical.ullTotalPhys/(1024*1024);
  ULARGE_INTEGER disk_free{},disk_total{},disk_total_free{};if(GetDiskFreeSpaceExW(c.root.c_str(),&disk_free,&disk_total,&disk_total_free))total_disk_mb=disk_total.QuadPart/(1024*1024);
  std::vector<adaptive::Device> all{cpu};all.insert(all.end(),devices.begin(),devices.end());
   std::vector<adaptive::ProbeEvidence> evidence;std::vector<adaptive::TuningEvidence> tuning;json::array tuning_measurements;
  headroom=available_resources(c.root,c.reserve_ram_mb);
  const auto cpu_limit=std::min(c.budget.cpu_threads,headroom.cpu_threads);
  const auto ram_limit=std::min(c.budget.ram_mb,headroom.ram_mb);
  const auto scratch_limit=std::min(c.budget.scratch_mb,headroom.scratch_mb);
  const auto total_vram=std::accumulate(devices.begin(),devices.end(),std::uint64_t{},[](auto sum,const auto& d){return sum+d.dedicated_vram_mb;});
  adaptive::Limits limits{cpu_limit,ram_limit,c.vram_override_set?c.budget.vram_mb:total_vram/2,scratch_limit,static_cast<std::uint32_t>(std::min<std::size_t>(c.max_jobs,64))};
  auto decode_valid=[&](const std::filesystem::path& output,const std::string& codec,const std::filesystem::path& log_root,Resources child_limits){
   if(!std::filesystem::is_regular_file(output)||!std::filesystem::file_size(output))return false;
   run_child_probe(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",output.wstring(),L"-f",L"null",L"-"},log_root,cancelled,child_limits);
   const auto log=read_file(log_root/"process.log");
   const std::regex stream("Video: "+codec+R"([^\r\n]*\b640x360\b)");
   const std::regex frames(R"(frame=\s*72(?:\s|$))");
   return std::regex_search(log,stream)&&std::regex_search(log,frames)&&
     log.find("Input #0, mov,mp4")!=std::string::npos&&log.find("Audio: aac")!=std::string::npos;
  };
  auto profile_for=[&](const adaptive::Device& d,adaptive::Codec codec){
   return adaptive::Profile{d.stable_id,d.driver_version,c.ffmpeg_sha256,d.family,codec,1,512,0,128,1,0,d.adapter_index,d.backend_index};
  };
   auto run_tuning=[&](const adaptive::Device& d,adaptive::Codec codec){
   const char* role=codec==adaptive::Codec::hevc?"feed_optimized":"feed";
   const std::string codec_name=codec==adaptive::Codec::hevc?"hevc":"h264";
   const auto output=workspace/(codec==adaptive::Codec::hevc?"optimized.mp4":"feed.mp4");
   auto plan=d.family==adaptive::Backend::cpu?rendition_plan(true,false,workspace):rendition_plan(true,false,workspace,std::vector<adaptive::Profile>{profile_for(d,codec)});
   auto rendition=std::find_if(plan.begin(),plan.end(),[&](const auto& item){return item.role==role;});
   if(rendition==plan.end()||(d.family!=adaptive::Backend::cpu&&!rendition->hardware))throw std::runtime_error("Adapter encoder command unavailable");
    const Resources candidate_limits{512,0,128,1};
    if(probe_limits.ram_mb<512||probe_limits.scratch_mb<128||probe_limits.cpu_threads<1)throw std::runtime_error("live headroom cannot safely reserve one measured profile");
    std::uint64_t vram_probe_cap{};bool estimated_vram=false;
    if(d.family!=adaptive::Backend::cpu){
     if(c.vram_override_set&&!c.budget.vram_mb)throw std::runtime_error("explicit global VRAM limit is zero; GPU probes are disabled");
     auto live_sample=sample_gpu_memory(0,d);
     const auto budget=adaptive::vram_budget(d.dedicated_vram_mb,
      live_sample?std::optional<std::uint64_t>{live_sample->adapter_bytes}:std::nullopt,limits.vram_mb);
     vram_probe_cap=budget.mb;estimated_vram=budget.estimated;
     if(!vram_probe_cap)throw std::runtime_error("no safe VRAM budget: telemetry/declared VRAM unavailable or headroom/cap exhausted");
    }
    double slowest_fps=std::numeric_limits<double>::max();std::uint64_t peak_vram_mb=0;
   for(unsigned repeat=0;repeat<2;++repeat){
    if(cancelled&&cancelled())throw std::runtime_error("Capability tuning cancelled");
     const auto sample=run_child_probe(c.ffmpeg,rendition->arguments,workspace,cancelled,candidate_limits,d.family==adaptive::Backend::cpu?nullptr:&d,vram_probe_cap);
     const auto stage_started=monotonic_ms();
     if(!decode_valid(output,codec_name,workspace,candidate_limits))throw std::runtime_error("Synthetic app output decode/profile validation failed");
     slowest_fps=std::min(slowest_fps,72000.0/static_cast<double>(sample.elapsed_ms+(monotonic_ms()-stage_started)));
    peak_vram_mb=std::max(peak_vram_mb,sample.peak_vram_mb);
    if(d.family!=adaptive::Backend::cpu&&sample.estimated_vram)estimated_vram=true;
   }
   if(!slowest_fps)throw std::runtime_error("Synthetic tuning measured no useful throughput");
   std::uint64_t vram_reservation=0;
   if(d.family!=adaptive::Backend::cpu){
    // Missing child samples reserve the entire bounded fallback, never a guessed footprint.
    if(estimated_vram){
     vram_probe_cap=std::min(vram_probe_cap,adaptive::vram_budget(d.dedicated_vram_mb,{},limits.vram_mb).mb);
     vram_reservation=vram_probe_cap;
     if(!vram_reservation||peak_vram_mb>vram_reservation)throw std::runtime_error("Observed GPU footprint exceeds declared-VRAM estimate");
    }else vram_reservation=peak_vram_mb+std::max<std::uint64_t>(128,peak_vram_mb/4);
     if(vram_reservation>d.dedicated_vram_mb||vram_reservation>limits.vram_mb||vram_reservation>vram_probe_cap)throw std::runtime_error("Measured VRAM footprint exceeds configured, live, probe reservation or dedicated device caps");
   }
   std::uint32_t tested_concurrency=1;
    if(d.family==adaptive::Backend::cpu&&cpu_limit>=2&&ram_limit>=1024&&scratch_limit>=256&&c.max_jobs>=2){
     std::vector<std::uint64_t> serial_stages,parallel_stages;
     auto prepare_child=[&](unsigned repeat,unsigned index,const char* mode){
      auto child_workspace=workspace/("tuning-"+codec_name+"-"+mode+"-"+std::to_string(repeat)+"-"+std::to_string(index));
      std::filesystem::create_directories(child_workspace);std::filesystem::copy_file(tune_source,child_workspace/"source.bin");
      auto child_plan=rendition_plan(true,false,child_workspace);auto child_rendition=std::find_if(child_plan.begin(),child_plan.end(),[&](const auto& item){return item.role==role;});
      if(child_rendition==child_plan.end())throw std::runtime_error("CPU concurrency candidate unavailable");
      auto target=child_workspace/child_rendition->filename;return std::tuple{child_workspace,child_rendition->arguments,target};
     };
     for(unsigned repeat=0;repeat<2;++repeat){
      auto live_now=available_resources(c.root,c.reserve_ram_mb);if(live_now.ram_mb<1024||live_now.cpu_threads<2||live_now.scratch_mb<256)throw std::runtime_error("stable live headroom fell below two equal candidate reservations");
      const auto serial_started=monotonic_ms();
      for(unsigned index=0;index<2;++index){auto [child_workspace,args,target]=prepare_child(repeat,index,"serial");run_child_probe(c.ffmpeg,args,child_workspace,cancelled,candidate_limits);if(!decode_valid(target,codec_name,child_workspace,candidate_limits))throw std::runtime_error("Serial CPU output failed strict validation");}
      serial_stages.push_back(static_cast<std::uint64_t>(std::max<Tick>(1,monotonic_ms()-serial_started)));
      std::vector<std::future<void>> runs;const auto parallel_started=monotonic_ms();
      for(unsigned index=0;index<2;++index){auto [child_workspace,args,target]=prepare_child(repeat,index,"parallel");runs.push_back(std::async(std::launch::async,[&,args=std::move(args),target,child_workspace]{run_child_probe(c.ffmpeg,args,child_workspace,cancelled,candidate_limits);if(!decode_valid(target,codec_name,child_workspace,candidate_limits))throw std::runtime_error("Concurrent CPU output failed strict validation");}));}
      for(auto& run:runs)run.get();parallel_stages.push_back(static_cast<std::uint64_t>(std::max<Tick>(1,monotonic_ms()-parallel_started)));
      if(cancelled&&cancelled())throw std::runtime_error("CPU tuning cancelled after contained children drained");
      auto after=available_resources(c.root,c.reserve_ram_mb);if(after.ram_mb<1024||after.cpu_threads<2||after.scratch_mb<256)throw std::runtime_error("live headroom changed during tuning");
     }
     const auto comparison=adaptive::compare_tuning_runs(serial_stages,parallel_stages,2,2,288,288,1,1,512,512,128,128);
     if(!comparison.comparable)throw std::runtime_error("serial and parallel candidates did not use equal work and budgets");
     tuning_measurements.emplace_back(json::object{{"codec",codec_name},{"serial_stage_ms",json::array{serial_stages[0],serial_stages[1]}},{"parallel_stage_ms",json::array{parallel_stages[0],parallel_stages[1]}},{"repeats",2},{"total_frames_per_candidate",288},{"child_cpu_threads",1},{"child_ram_mb",512},{"child_scratch_mb",128},{"serial_aggregate_fps",comparison.serial_fps},{"parallel_aggregate_fps",comparison.parallel_fps}});
     if(comparison.parallel_fps>=comparison.serial_fps*1.05){slowest_fps=comparison.parallel_fps/2.0;tested_concurrency=2;}
     else unavailable.emplace_back(std::string("CPU ")+(codec==adaptive::Codec::h264?"H.264":"HEVC")+" concurrency 2 measured aggregate "+std::to_string(comparison.parallel_fps)+" fps versus equal-work serial aggregate "+std::to_string(comparison.serial_fps)+" fps; below 5% gain, retained concurrency 1");
    }
   if(d.family!=adaptive::Backend::cpu)budget_sources[d.stable_id+":"+std::to_string(static_cast<int>(codec))]=estimated_vram?"declared-VRAM estimate":"live VRAM telemetry";
   evidence.push_back({d.stable_id,d.driver_version,c.ffmpeg_sha256,d.family,codec,true,true,true});
   tuning.push_back({d.stable_id,d.family,codec,slowest_fps,1,512,vram_reservation,128,true,true,true,tested_concurrency,640,360});
  };
  if(ram_limit<512||scratch_limit<128||!cpu_limit)unavailable.emplace_back("live headroom below one CPU profile cost; adaptive app profiles withheld");
   else for(auto codec:{adaptive::Codec::h264,adaptive::Codec::hevc}){
   try{run_tuning(cpu,codec);}catch(const std::exception& e){unavailable.emplace_back(std::string("CPU ")+(codec==adaptive::Codec::h264?"H.264: ":"HEVC: ")+e.what());}
   for(const auto& device:devices){
     if(c.vram_override_set&&!c.budget.vram_mb){unavailable.emplace_back(device.stable_id+" explicit global VRAM limit is zero; retained in inventory without GPU probing");continue;}
    if(!device.backend_identity_verified){unavailable.emplace_back(device.stable_id+" installed driver could not map this DXGI adapter unambiguously to an encoder device; GPU profile withheld");continue;}
    if(!device.driver_healthy){unavailable.emplace_back(device.stable_id+" driver or dedicated VRAM unavailable");continue;}
    try{run_tuning(device,codec);}catch(const std::exception& e){unavailable.emplace_back(device.stable_id+" "+(codec==adaptive::Codec::h264?"H.264: ":"HEVC: ")+e.what());}
   }
  }
  for(const auto& measured:tuning)if(measured.backend!=adaptive::Backend::cpu){
   const auto cpu_measure=std::find_if(tuning.begin(),tuning.end(),[&](const auto& candidate){return candidate.backend==adaptive::Backend::cpu&&candidate.codec==measured.codec;});
   if(cpu_measure!=tuning.end()&&measured.frames_per_second<cpu_measure->frames_per_second*1.05)
    unavailable.emplace_back(measured.stable_id+" "+(measured.codec==adaptive::Codec::h264?"H.264":"HEVC")+" measured less than 5% faster than CPU; profile withheld");
  }
  p.profiles=adaptive::choose_profiles(all,evidence,tuning,limits,c.ffmpeg_sha256);
  const auto cpu_h264=std::find_if(p.profiles.begin(),p.profiles.end(),[](const auto& profile){return profile.backend==adaptive::Backend::cpu&&profile.codec==adaptive::Codec::h264;});
  const auto cpu_hevc=std::find_if(p.profiles.begin(),p.profiles.end(),[](const auto& profile){return profile.backend==adaptive::Backend::cpu&&profile.codec==adaptive::Codec::hevc;});
  if(cpu_h264==p.profiles.end()||cpu_hevc==p.profiles.end()){
   p.media_video=false;unavailable.emplace_back("verified CPU fallback profile missing for H.264 or HEVC; app video capability disabled");
  }
  json::array saved_profiles,saved_devices;
  for(const auto& device:devices)saved_devices.emplace_back(json::object{{"device_id",device.stable_id},{"name",device.name},{"backend",static_cast<int>(device.family)},{"driver_version",device.driver_version},{"dedicated_vram_mb",device.dedicated_vram_mb},{"adapter_index",device.adapter_index},{"backend_index",device.backend_index},{"backend_identity_verified",device.backend_identity_verified},{"driver_healthy",device.driver_healthy}});
  for(const auto& profile:p.profiles)saved_profiles.emplace_back(json::object{{"device_id",profile.stable_id},{"driver_version",profile.driver_version},{"ffmpeg_sha256",profile.ffmpeg_sha256},{"backend",static_cast<int>(profile.backend)},{"codec",static_cast<int>(profile.codec)},{"cpu_threads",profile.cpu_threads},{"ram_mb",profile.ram_mb},{"vram_mb",profile.vram_mb},{"scratch_mb",profile.scratch_mb},{"max_concurrency",profile.max_concurrency},{"measured_fps",profile.measured_frames_per_second},{"adapter_index",profile.adapter_index},{"backend_index",profile.backend_index},{"backend_identity_verified",profile.backend_identity_verified},{"max_width",profile.max_width},{"max_height",profile.max_height}});
  json::array saved_reasons;for(const auto& reason:unavailable)saved_reasons.emplace_back(reason);
   atomic_write(c.root/"adaptive-hardware.json",json::serialize(json::object{{"schema",1},{"ffmpeg_sha256",c.ffmpeg_sha256},{"processor",processor_model()},{"logical_cpu_count",GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)},{"total_ram_mb",total_ram_mb},{"available_ram_mb",headroom.ram_mb},{"total_disk_mb",total_disk_mb},{"available_scratch_mb",headroom.scratch_mb},{"limits",{{"cpu_threads",c.budget.cpu_threads},{"ram_mb",c.budget.ram_mb},{"vram_mb_override",c.budget.vram_mb},{"vram_mb_override_set",c.vram_override_set},{"vram_mb_effective",limits.vram_mb},{"scratch_mb",c.budget.scratch_mb},{"max_jobs",c.max_jobs}}},{"devices",std::move(saved_devices)},{"profiles",std::move(saved_profiles)},{"tuning_measurements",std::move(tuning_measurements)},{"unavailable_reasons",std::move(saved_reasons)}}));
  for(const auto& profile:p.profiles)if(profile.backend!=adaptive::Backend::cpu)p.reason+=" selected "+profile.stable_id+" "+(profile.codec==adaptive::Codec::h264?"H.264":"HEVC")+" measured "+std::to_string(profile.measured_frames_per_second)+" fps";
  if(p.profiles.empty())unavailable.emplace_back("no codec profile passed probe and bounded tuning");
 }else unavailable.emplace_back("CPU app video probe failed; hardware remains disabled");
 p.reason="Adaptive media setup: CPU="+processor_model()+" logical="+std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS))+" RAM_total_MB="+std::to_string(total_ram_mb)+" RAM_available_MB="+std::to_string(headroom.ram_mb)+" disk_total_MB="+std::to_string(total_disk_mb)+" disk_available_MB="+std::to_string(headroom.scratch_mb)+" approved_ffmpeg_sha256="+c.ffmpeg_sha256+"; detected adapters="+std::to_string(devices.size());
 for(const auto& device:devices)p.reason+=" ["+device.name+" id="+device.stable_id+" dedicated_VRAM_MB="+std::to_string(device.dedicated_vram_mb)+" driver="+(device.driver_version.empty()?"unavailable":device.driver_version)+"]";
 p.reason+="; tested profiles="+std::to_string(p.profiles.size())+"; app video="+(p.media_video?"CPU fallback verified":"unavailable")+" app image="+(p.media_image?"verified":"unavailable");
 for(const auto& profile:p.profiles)p.reason+="; chosen "+backend_name(profile.backend)+" "+(profile.codec==adaptive::Codec::h264?"H.264":"HEVC")+" at "+std::to_string(profile.measured_frames_per_second)+" measured fps with concurrency="+std::to_string(profile.max_concurrency)+(profile.backend==adaptive::Backend::cpu?"":" ("+budget_sources[profile.stable_id+":"+std::to_string(static_cast<int>(profile.codec))]+")");
 for(const auto& reason:unavailable)p.reason+="; fallback "+reason;
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
 safe_log("WORKER STARTED");
 // Slow capability probes run on the background thread, never blocking tray creation.
 network_=std::jthread([this]{network();});try{watchdog_=std::jthread([this]{watchdog();});hash_watchdog_=std::jthread([this]{
  while(!stop_){for(unsigned elapsed=0;elapsed<60000&&!stop_;elapsed+=100)Sleep(100);if(stop_)break;
   if(config_.ffmpeg.empty()||config_.ffmpeg_sha256.size()!=64)continue;
   try{if(sha256_file(config_.ffmpeg)!=config_.ffmpeg_sha256){ffmpeg_hash_changed_=true;break;}}catch(...){ffmpeg_hash_changed_=true;break;}
  }
 });}catch(...){stop_=true;network_.join();if(watchdog_.joinable())watchdog_.join();throw;}
}
Worker::~Worker(){stop_=true;{std::lock_guard lock(mutex_);for(auto& [_,r]:running_)r->data->cancel=true;}if(network_.joinable())network_.join();if(watchdog_.joinable())watchdog_.join();if(hash_watchdog_.joinable())hash_watchdog_.join();std::unordered_map<std::string,std::unique_ptr<Running>> remaining;{std::lock_guard lock(mutex_);remaining.swap(running_);}remaining.clear();}
void Worker::safe_log(const std::string& s) noexcept {try{log_.write(s);}catch(...){paused_=true;admission_.pause(true);}}
void Worker::state(std::string s){auto shown=s.starts_with("STATE ")?s.substr(6):s;bool changed{};{std::lock_guard lock(mutex_);changed=status_!=shown;status_=shown;}if(changed)safe_log(s);}
std::string Worker::status() const{std::lock_guard lock(mutex_);return status_;}
std::size_t Worker::active_count() const{return admission_.active().size();}
void Worker::pause(bool p){if(exit_requested_&&!p)throw std::runtime_error("Worker is exiting; restart it to resume");paused_=p;admission_.pause(p);store_.set("paused",p?"true":"false");state(p?"STATE PAUSED":"STATE RESUMED");}
void Worker::priority(Priority p){priority_=p;store_.set("priority",priority_name(p));state("STATE PRIORITY "+priority_name(p));}
void Worker::apply_preferences(){admission_.capability("conversion.video.h264",verified_video_.load(),video_);admission_.capability("conversion.image.jpeg",verified_image_.load(),image_);admission_.capability("media.video.renditions.v1",verified_media_video_.load(),video_);admission_.capability("media.image.renditions.v1",verified_media_image_.load(),image_);}
adaptive::Scheduler::Reservation Worker::reserve_hardware_profiles(const std::vector<adaptive::Codec>& codecs,const Resources& offer){
 std::lock_guard lock(mutex_);if(!adaptive_scheduler_)return {};
 std::vector<adaptive::Device> devices;
 try{devices=enumerate_video_devices();}catch(...){devices.clear();}
 std::map<std::string,std::uint64_t> device_profile_caps;
 for(const auto& profile:hardware_profiles_)if(profile.backend!=adaptive::Backend::cpu)
  device_profile_caps[profile.stable_id]=std::max(device_profile_caps[profile.stable_id],profile.vram_mb*profile.max_concurrency);
 for(const auto& [id,profile_cap]:device_profile_caps){
  std::uint64_t available_mb{};
  auto device=std::find_if(devices.begin(),devices.end(),[&](const auto& current){return current.stable_id==id&&current.backend_identity_verified;});
  if(device!=devices.end()){
   auto sample=sample_gpu_memory(0,*device);
   available_mb=adaptive::vram_budget(device->dedicated_vram_mb,
    sample?std::optional<std::uint64_t>{sample->adapter_bytes}:std::nullopt,profile_cap).mb;
  }
  adaptive_scheduler_->set_device_available_vram(id,std::min(profile_cap,available_mb));
 }
 std::uint64_t vram=config_.budget.vram_mb;if(!config_.vram_override_set)for(const auto& profile:hardware_profiles_)if(profile.backend!=adaptive::Backend::cpu)vram+=profile.vram_mb*profile.max_concurrency;
 return adaptive_scheduler_->reserve(codecs,adaptive::Limits{offer.cpu_threads,offer.ram_mb,vram,offer.scratch_mb,1});
}
void Worker::preference(bool video,Preference value){if(video)video_=value;else image_=value;store_.set(video?"video_preference":"image_preference",pref_name(value));apply_preferences();state(std::string("STATE PREFERENCE ")+(video?"video=":"image=")+pref_name(value));}
void Worker::exit(bool kill){pause(true);exit_requested_=true;{std::lock_guard lock(mutex_);if(kill)for(auto& [_,r]:running_)r->data->cancel=true;}state(kill?"STATE STOPPING":"STATE DRAINING");}
bool Worker::ready_to_exit() const{if(!exit_requested_)return false;std::lock_guard lock(mutex_);return running_.empty();}
json::object Worker::heartbeat(){
 auto free=available_resources(config_.root,config_.reserve_ram_mb);auto used=admission_.used();json::array active;
 for(const auto& a:admission_.active())active.emplace_back(json::object{{"attempt_id",a.offer.attempt_id},{"state",a.phase==Phase::reserved?"reserved":a.phase==Phase::running?"running":"cancelling"}});
 return {{"protocol",1},{"type","heartbeat"},{"status",paused_?"paused":"ready"},{"priority",priority_name(priority_)},{"active",std::move(active)},
 {"available",{{"ram_mb",free.ram_mb},{"vram_mb",0},{"scratch_mb",free.scratch_mb},{"cpu_threads",free.cpu_threads}}},
 {"reserved",{{"ram_mb",used.ram_mb},{"scratch_mb",used.scratch_mb},{"cpu_threads",used.cpu_threads}}},
 {"capabilities",{{"conversion.video.h264",{{"verified",verified_video_.load()},{"preference",pref_name(video_)}}},{"conversion.image.jpeg",{{"verified",verified_image_.load()},{"preference",pref_name(image_)}}},{"media.video.renditions.v1",{{"verified",verified_media_video_.load()},{"preference",pref_name(video_)}}},{"media.image.renditions.v1",{{"verified",verified_media_image_.load()},{"preference",pref_name(image_)}}}}}};
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
    state("PAIRING CODE "+code+" "+std::to_string(seconds));
   }});
 if(!already_paired)safe_log("PAIRING APPROVED");
 return token;
}
void Worker::network(){
 safe_log("PROBE START");
 try{
  auto probe=probe_ffmpeg(config_,[this]{return stop_||exit_requested_;});
  verified_video_=probe.video;verified_image_=probe.image;verified_media_video_=probe.media_video;verified_media_image_=probe.media_image;{std::lock_guard lock(mutex_);hardware_profiles_=std::move(probe.profiles);std::uint64_t runtime_vram=config_.budget.vram_mb;if(!config_.vram_override_set)for(const auto& profile:hardware_profiles_)if(profile.backend!=adaptive::Backend::cpu)runtime_vram+=profile.vram_mb*profile.max_concurrency;adaptive_scheduler_=std::make_unique<adaptive::Scheduler>(adaptive::Limits{config_.budget.cpu_threads,config_.budget.ram_mb,runtime_vram,config_.budget.scratch_mb,static_cast<std::uint32_t>(config_.max_jobs)},hardware_profiles_);}apply_preferences();
  std::string record;
  if(config_.ffmpeg.empty())record="PROBE RESULT disabled";
  else record="PROBE RESULT app_video="+std::to_string(probe.media_video?1:0)+" app_image="+std::to_string(probe.media_image?1:0)
   +" video="+std::to_string(probe.video?1:0)+" image="+std::to_string(probe.image?1:0);
  if(record!=last_capability_){safe_log(record);last_capability_=record;}
 }catch(const std::exception& e){state(std::string("STATE PROBE_FAILURE ")+e.what());}
 if(config_.coordinator.empty()){state("STATE UNCONFIGURED");while(!stop_)sleep_until_stop(stop_,500);return;}
 unsigned backoff=1000;
 while(!stop_){
 try{
  auto token=paired_token();if(stop_||exit_requested_)return;Http http;check_protocol(http.request("GET",config_.coordinator+"/v1/worker/protocol"));WebSocket socket(config_.coordinator+"/v1/worker/connect",token);SecureZeroMemory(token.data(),token.size());
  socket.send(json::serialize(json::object{{"protocol",1},{"type","hello"},{"agent_version","0.1.2"},{"state",heartbeat()}}));
  state(connected_once_?"STATE RECONNECTED":"STATE CONNECTED");connected_once_=true;backoff=1000;Tick next_heartbeat=0,next_results=0,next_status=monotonic_ms()+10000;
  while(!stop_){
   auto now=monotonic_ms();if(now>=next_heartbeat){
    // Specs for expired unstarted offers are not an unbounded in-memory queue.
    for(auto it=offered_.begin();it!=offered_.end();){if(it->second.offer.reserve_until<=now)it=offered_.erase(it);else ++it;}
    socket.send(json::serialize(heartbeat()));next_heartbeat=now+5000;
   }
   if(now>=next_results){for(const auto& p:store_.pending()){auto body=parse(p.body).as_object();body["outbox_sequence"]=p.sequence;socket.send(json::serialize(body));}next_results=now+2000;}
   if(now>=next_status){
    const auto active=active_count(),pending=store_.pending_count();auto record=connectionActivityRecord(active,pending);
    if(record!=last_activity_){if(active||pending||!last_activity_.empty())safe_log(record);last_activity_=record;}
    next_status=now+10000;
   }
   if(auto incoming=socket.receive()){auto value=parse(*incoming);message(socket,value.as_object());}
  }
 }catch(const PairingActionRequired& e){
  pause(true);state(std::string("STATE ACTION ")+e.what());
  while(!stop_&&!exit_requested_)sleep_until_stop(stop_,500);
  return;
 }catch(const std::exception& e){
  state(std::string("STATE OFFLINE ")+e.what());
  // Never bypass an explicit revocation by silently starting a new pairing.
  auto reason=std::string(e.what());if(reason.find("HTTP 401")!=std::string::npos||reason.find("HTTP 403")!=std::string::npos){pause(true);state("STATE REVOKED");while(!stop_)sleep_until_stop(stop_,500);return;}
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
  }catch(const std::exception&){if(safe_id(id)){socket.send(json::serialize(json::object{{"protocol",1},{"type","offer_reply"},{"attempt_id",id},{"accepted",false},{"reason","invalid_or_unavailable"}}));safe_log("JOB "+id+" DECLINED | validation, dependency, capacity, or journal check failed");}else safe_log("OFFER DECLINED | invalid identity");}return;
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
   const bool video=!data->spec.artifacts.empty()&&data->spec.offer.capability=="media.video.renditions.v1";
   if(video){auto reservation=reserve_hardware_profiles({adaptive::Codec::h264,adaptive::Codec::hevc},data->spec.offer.resources);
     if(!reservation){admission_.finish(id);store_.decline_reserved(id,"profile_capacity_unavailable");offered_.erase(i);started=Start::declined;}
    else data->profile_reservation.emplace(std::move(reservation));
   }
   if(started==Start::started){
     if(!store_.start(id)){admission_.finish(id);store_.decline_reserved(id,"journal_start_failed");offered_.erase(i);throw std::runtime_error("No persisted job reservation for lease");}
    offered_.erase(i);auto running=std::make_unique<Running>();running->data=data;
    {std::lock_guard lock(mutex_);running->thread=std::jthread([this,data]{execute(data);});running_.emplace(id,std::move(running));}
    safe_log("START "+id+" "+data->spec.offer.capability+" "+job_subject(data->spec));
   }
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
 auto subject=job_subject(s);
 auto phase=[&](const char* name,std::uint64_t total=0){data->progress.begin(name,monotonic_ms(),total);safe_log("JOB "+id+" STAGE | "+name+" | run "+std::to_string(data->progress.snapshot(monotonic_ms()).run));};
 auto bytes_progress=[&](std::uint64_t bytes){if(cancelled())throw std::runtime_error("Job cancelled or lease expired");data->progress.update(bytes,monotonic_ms());};
 try{
  phase("prepare workspace");
  if(std::filesystem::exists(workspace))throw std::runtime_error("Attempt workspace already exists; refusing unsafe reuse");std::filesystem::create_directories(workspace);
  Http http;phase("download input",s.input_bytes);http.download(s.input_url,workspace/"source.bin",s.input_bytes,cancelled,bytes_progress);
  phase("verify input SHA-256",s.input_bytes);if(sha256_file(workspace/"source.bin",bytes_progress)!=s.input_sha256)throw std::runtime_error("Input integrity check failed");if(cancelled())throw std::runtime_error("Job cancelled before conversion");
  auto convert=[&](const std::vector<std::wstring>& arguments,const std::vector<std::wstring>& cpu_arguments,
                   const std::string& filename,unsigned seconds,bool hardware,std::stop_token pipeline_stop=std::stop_token{}){
   if(cancelled()||pipeline_stop.stop_requested())throw std::runtime_error("Job cancelled before conversion");
   auto label="convert "+filename;
   auto run_encoder=[&](const std::vector<std::wstring>& selected,bool use_hardware){
    std::optional<adaptive::Device> gpu_device;std::uint64_t gpu_reserved{};
    if(use_hardware){
     const auto codec=filename=="optimized.mp4"?adaptive::Codec::hevc:adaptive::Codec::h264;
     if(!data->profile_reservation)throw std::runtime_error("GPU reservation missing");
     for(const auto& profile:data->profile_reservation->profiles())if(profile.backend!=adaptive::Backend::cpu&&profile.codec==codec){
      for(const auto& device:enumerate_video_devices())if(adaptive::profile_current(profile,device,config_.ffmpeg_sha256)){gpu_device=device;gpu_reserved=profile.vram_mb;break;}
     }
     if(!gpu_device)throw std::runtime_error("GPU profile no longer matches installed adapter");
    }
    auto check_gpu=[&](std::uint32_t pid){
     if(!gpu_device)return;
     auto sample=sample_gpu_memory(pid,*gpu_device);
     if(!adaptive::vram_envelope_ok(gpu_device->dedicated_vram_mb,gpu_reserved,
       sample?std::optional<adaptive::GpuMemoryTotals>{{sample->child_bytes,sample->adapter_bytes}}:std::nullopt))
      throw std::runtime_error("GPU runtime memory envelope exceeded; process tree aborted");
    };
    check_gpu(0);
    phase(label.c_str());
    const auto started=monotonic_ms();auto current=priority_.load();Child child(config_.ffmpeg,encoder_progress_arguments(selected),workspace,s.offer.resources,current);
    EncoderProgressFile encoder(workspace/"process.log");
    for(;;){check_gpu(child.process_id());if(cancelled()||pipeline_stop.stop_requested()||monotonic_ms()-started>=static_cast<Tick>(seconds)*1000)throw std::runtime_error("Job cancelled or conversion timed out");if(auto code=child.exit_code()){if(*code)throw std::runtime_error("Conversion process failed; see per-job log");break;}
     auto latest=priority_.load();if(latest!=current){child.priority(latest);current=latest;}
     encoder.update(data->progress,monotonic_ms());std::error_code error;auto size=std::filesystem::file_size(workspace/filename,error);if(!error&&size>s.output_max_bytes)throw std::runtime_error("Output exceeded declared byte limit");
     auto log_bytes=std::filesystem::file_size(workspace/"process.log",error);if(!error&&log_bytes>5*1024*1024)throw std::runtime_error("Child output exceeded log budget");Sleep(100);
    }
    encoder.update(data->progress,monotonic_ms());
   };
   if(hardware){
    const auto output=workspace/filename;
    adaptive::run_with_cpu_fallback(output,true,[&]{return cancelled()||pipeline_stop.stop_requested();},
      [&](bool use_hardware){
       if(!use_hardware&&data->profile_reservation&&!data->profile_reservation->acquire_cpu_fallback([&]{return cancelled()||pipeline_stop.stop_requested();},30000))
        throw std::runtime_error("CPU fallback encoder slot unavailable before lease or cancellation deadline");
       run_encoder(use_hardware?arguments:cpu_arguments,use_hardware);
      },
     [&](const std::filesystem::path& path){
      if(cancelled()||pipeline_stop.stop_requested())return false;
      run_child_probe(config_.ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",path.wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace,[&]{return cancelled()||pipeline_stop.stop_requested();},s.offer.resources);
      return std::filesystem::file_size(path)>0;
     });
   }else run_encoder(arguments,false);
   data->progress.finish(true,monotonic_ms());
  };
  auto upload=[&](const std::string& filename,const std::string& url,const std::string& mime,std::stop_token pipeline_stop=std::stop_token{}){
   auto transfer_cancelled=[&]{return cancelled()||pipeline_stop.stop_requested();};
   auto transfer_bytes=[&](std::uint64_t count){if(transfer_cancelled())throw std::runtime_error("Job cancelled during transfer");data->transfer_progress.update(count,monotonic_ms());};
   auto transfer_phase=[&](const std::string& name,std::uint64_t total){data->transfer_progress.begin(name,monotonic_ms(),total);safe_log("JOB "+id+" STAGE | "+name+" | run "+std::to_string(data->transfer_progress.snapshot(monotonic_ms()).run));};
   if(transfer_cancelled())throw std::runtime_error("Job cancelled before transfer");
   auto output=workspace/filename;auto bytes=std::filesystem::file_size(output);if(!bytes||bytes>s.output_max_bytes)throw std::runtime_error("Invalid output size");
   auto label="verify "+filename+" SHA-256";transfer_phase(label,bytes);auto hash=sha256_file(output,transfer_bytes);
   label="upload "+filename;transfer_phase(label,bytes);Http transfer_http;transfer_http.upload(url,output,mime,transfer_cancelled,transfer_bytes);
   data->transfer_progress.finish(true,monotonic_ms());return json::object{{"sha256",hash},{"bytes",bytes},{"content_type",mime}};
  };
  if(!s.artifacts.empty()){
   const bool video=s.offer.capability=="media.video.renditions.v1";
   const auto selected_profiles=data->profile_reservation?data->profile_reservation->profiles():std::vector<adaptive::Profile>{};
   auto applicable_profiles=selected_profiles;
   const auto has_hardware=std::any_of(applicable_profiles.begin(),applicable_profiles.end(),[](const auto& p){return p.backend!=adaptive::Backend::cpu;});
   if(video&&has_hardware){
    std::optional<std::pair<std::uint32_t,std::uint32_t>> dimensions;
    try{dimensions=inspect_video_dimensions(config_.ffmpeg,workspace/"source.bin",workspace,cancelled,Resources{s.offer.resources.ram_mb,0,s.offer.resources.scratch_mb,s.offer.resources.cpu_threads});}
    catch(const std::exception&){dimensions.reset();}
    const auto original_size=applicable_profiles.size();
    applicable_profiles.erase(std::remove_if(applicable_profiles.begin(),applicable_profiles.end(),[&](const auto& profile){
     return profile.backend!=adaptive::Backend::cpu&&(!dimensions||!adaptive::profile_supports_dimensions(profile,dimensions->first,dimensions->second));
    }),applicable_profiles.end());
    if(applicable_profiles.size()!=original_size)safe_log("JOB "+id+" CPU fallback: input outside GPU profile");
   }
   if(std::any_of(applicable_profiles.begin(),applicable_profiles.end(),[](const auto& p){return p.backend!=adaptive::Backend::cpu;}))safe_log("JOB "+id+" using GPU profiles");
   auto plan=rendition_plan(video,s.copy_audio,workspace,applicable_profiles);
   std::vector<adaptive::Profile> cpu_profiles;{std::lock_guard lock(mutex_);for(const auto& profile:hardware_profiles_)if(profile.backend==adaptive::Backend::cpu)cpu_profiles.push_back(profile);}
   auto cpu_plan=rendition_plan(video,s.copy_audio,workspace,cpu_profiles);json::object outputs;
   auto metadata=run_rendition_pipeline(plan,[&](const Rendition& rendition,std::stop_token stop){
    auto cpu=std::find_if(cpu_plan.begin(),cpu_plan.end(),[&](const auto& value){return value.role==rendition.role;});
    if(cpu==cpu_plan.end())throw std::runtime_error("CPU fallback rendition missing");
    convert(rendition.arguments,cpu->arguments,rendition.filename,rendition.timeout_seconds,rendition.hardware,stop);
   },[&](const Rendition& rendition,std::stop_token stop){
    const auto target=std::find_if(s.artifacts.begin(),s.artifacts.end(),[&](const auto& t){return t.role==rendition.role;});
    if(target==s.artifacts.end())throw std::runtime_error("Rendition target missing");
    return upload(rendition.filename,target->url,rendition.mime,stop);
   });
   for(std::size_t i=0;i<plan.size();++i)outputs[plan[i].role]=std::move(metadata[i]);
   result["outputs"]=std::move(outputs);
  }else{
   convert(ffmpeg_arguments(s,workspace),ffmpeg_arguments(s,workspace),output_name(s),static_cast<unsigned>(s.timeout_ms/1000),false);
   result["output"]=upload(output_name(s),s.output_url,s.offer.capability=="conversion.video.h264"?"video/mp4":"image/jpeg");
  }
  if(cancelled())throw std::runtime_error("Job cancelled before completion");
  result["status"]="succeeded";
 }catch(const std::exception& e){data->progress.finish(false,monotonic_ms());data->transfer_progress.finish(false,monotonic_ms());result["status"]="failed";result["reason"]=std::string(e.what());const bool cancelled=data->cancel||stop_||monotonic_ms()>=data->hard_deadline;safe_log("JOB "+id+(cancelled?" CANCELLED | ":" FAILED | ")+subject+" | "+e.what());}
 // Media is not needed after an attempt: authoritative output is in object
 // storage, and any accepted completion is durably represented in the outbox.
 try{store_.result(id,json::serialize(result));safe_log("RESULT PENDING ACK "+id+" "+std::string(result.at("status").as_string()));}catch(...){paused_=true;admission_.pause(true);safe_log("STATE JOURNAL_FAILURE");}
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
}
void Worker::watchdog(){
 Tick next_progress=0,next_adaptive_check=monotonic_ms()+60000;
 while(!stop_){
 try{
  for(const auto& e:admission_.expire(monotonic_ms())){
   if(e.stop_process){std::lock_guard lock(mutex_);if(auto i=running_.find(e.attempt_id);i!=running_.end())i->second->data->cancel=true;}
   else store_.result(e.attempt_id,json::serialize(json::object{{"protocol",1},{"type","result"},{"attempt_id",e.attempt_id},{"status","abandoned"},{"reason","offer_expired"}}));
  }
  auto now=monotonic_ms();if(ffmpeg_hash_changed_.exchange(false)){
   {std::lock_guard lock(mutex_);hardware_profiles_.clear();if(adaptive_scheduler_)adaptive_scheduler_->invalidate_all();}
   verified_video_=false;verified_image_=false;verified_media_video_=false;verified_media_image_=false;apply_preferences();
   safe_log("ADAPTIVE INVALIDATION: approved FFmpeg changed or became unreadable; converter capabilities disabled until a fresh probe");
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
       return device==devices.end()||!adaptive::profile_current(profile,*device,config_.ffmpeg_sha256);
      }),hardware_profiles_.end());changed=old!=hardware_profiles_.size();}
     if(changed){std::lock_guard lock(mutex_);if(adaptive_scheduler_)adaptive_scheduler_->invalidate_hardware();invalidation="adapter identity, driver version, or device capacity changed; stale hardware profiles disabled and CPU fallback retained";}
    }catch(...){
     bool removed=false;{std::lock_guard lock(mutex_);const auto old=hardware_profiles_.size();hardware_profiles_.erase(std::remove_if(hardware_profiles_.begin(),hardware_profiles_.end(),[](const auto& profile){return profile.backend!=adaptive::Backend::cpu;}),hardware_profiles_.end());removed=old!=hardware_profiles_.size();}
     if(removed){std::lock_guard lock(mutex_);if(adaptive_scheduler_)adaptive_scheduler_->invalidate_hardware();invalidation="adapter inventory unavailable; hardware profiles disabled and CPU fallback retained";}
    }
   }
   if(!invalidation.empty()){
    safe_log("ADAPTIVE INVALIDATION: "+invalidation);
    try{
     auto state=parse(read_file(config_.root/"adaptive-hardware.json")).as_object();json::array current_profiles,reasons;
     {std::lock_guard lock(mutex_);for(const auto& profile:hardware_profiles_)current_profiles.emplace_back(json::object{{"device_id",profile.stable_id},{"driver_version",profile.driver_version},{"ffmpeg_sha256",profile.ffmpeg_sha256},{"backend",static_cast<int>(profile.backend)},{"codec",static_cast<int>(profile.codec)},{"cpu_threads",profile.cpu_threads},{"ram_mb",profile.ram_mb},{"vram_mb",profile.vram_mb},{"scratch_mb",profile.scratch_mb},{"max_concurrency",profile.max_concurrency},{"measured_fps",profile.measured_frames_per_second},{"adapter_index",profile.adapter_index},{"backend_index",profile.backend_index},{"max_width",profile.max_width},{"max_height",profile.max_height}});}
     if(auto existing=state.find("unavailable_reasons");existing!=state.end())reasons=existing->value().as_array();
     reasons.emplace_back(invalidation);state["profiles"]=std::move(current_profiles);state["unavailable_reasons"]=std::move(reasons);atomic_write(config_.root/"adaptive-hardware.json",json::serialize(state));
    }catch(...){safe_log("Adaptive invalidation could not update its diagnostic profile file");}
   }
   next_adaptive_check=now+60000;
  }
  if(now>=next_progress){
   std::vector<std::shared_ptr<TaskData>> active;{std::lock_guard lock(mutex_);for(const auto& [_,r]:running_)if(!r->data->done)active.push_back(r->data);}
   for(const auto& data:active)for(auto* counter:{&data->progress,&data->transfer_progress}){auto p=counter->snapshot(now);if(p.stage.empty()||p.finished)continue;
    if(auto line=progress_log_.line(data->spec.offer.attempt_id,job_subject(data->spec),p))safe_log(*line);
   }
   next_progress=now+2000;
  }
  std::vector<std::pair<std::string,std::unique_ptr<Running>>> finished;{std::lock_guard lock(mutex_);for(auto i=running_.begin();i!=running_.end();){if(i->second->data->done){finished.emplace_back(i->first,std::move(i->second));i=running_.erase(i);}else ++i;}}
  for(const auto& item:finished)progress_log_.forget(item.first);finished.clear();
 }catch(const std::exception& e){paused_=true;admission_.pause(true);state(std::string("STATE WATCHDOG_FAILURE ")+e.what());}
 sleep_until_stop(stop_,100);
 }
}
}
