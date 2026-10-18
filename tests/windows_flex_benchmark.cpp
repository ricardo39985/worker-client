// Opt-in real worker execution with local storage ports; never a CTest GPU test.
#include "offline_media.hpp"
#include "hardware.hpp"
#include "capacity.hpp"
#include "ow/adaptive_evidence.hpp"
#include "windows_crash_trace.hpp"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <thread>
using namespace ow;using namespace ow::win;
namespace {
bool below(const std::filesystem::path& path,const std::filesystem::path& parent){
 const auto relative=path.lexically_normal().lexically_relative(parent.lexically_normal());
 return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!=L"..";
}
struct Peaks {
 double cpu{};std::uint64_t host_ram{},process_commit{},working_set{};std::map<std::string,std::uint64_t> vram;
 bool external_ffmpeg{};unsigned samples{},cpu_samples{},vram_samples{},foreground_samples{},unknown_foreground_samples{},no_foreground_window_samples{};std::vector<std::uint64_t> message_latency;
 void sample(const std::filesystem::path& root,const std::vector<adaptive::Device>& devices){
  const auto pressure=sample_runtime_pressure(root);
  try{const auto staging=data_directory().parent_path()/"Temp"/"ow-worker-flex-20261018-r2";const auto state=parse(read_file(staging/"foreground.json",4096)).as_object();
   const auto observed=static_cast<Tick>(number(state,"observed_utc_ms",INT64_MAX));
   if(utc_ms()<observed||utc_ms()-observed>2000)++unknown_foreground_samples;
   else{if(state.at("foreground_active").as_bool())++foreground_samples;if(state.contains("window_present")&&!state.at("window_present").as_bool())++no_foreground_window_samples;else if(state.at("message_response_known").as_bool())message_latency.push_back(number(state,"message_response_ms",1000));else ++unknown_foreground_samples;}
  }catch(...){++unknown_foreground_samples;}
  if(pressure.host.cpu_percent){cpu=std::max(cpu,*pressure.host.cpu_percent);++cpu_samples;}
  MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(GlobalMemoryStatusEx(&memory))host_ram=std::max(host_ram,static_cast<std::uint64_t>(memory.ullTotalPhys-memory.ullAvailPhys));
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));std::uint64_t commit=0,rss=0;
  PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
  if(snapshot&&Process32FirstW(snapshot.get(),&entry))do{
   const bool ours=entry.th32ProcessID==GetCurrentProcessId()||entry.th32ParentProcessID==GetCurrentProcessId();
   if(_wcsicmp(entry.szExeFile,L"ffmpeg.exe")==0&&!ours)external_ffmpeg=true;
   if(!ours)continue;
   Handle process(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,entry.th32ProcessID));PROCESS_MEMORY_COUNTERS_EX counters{};
   if(process&&GetProcessMemoryInfo(process.get(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))){commit+=counters.PrivateUsage;rss+=counters.WorkingSetSize;}
  }while(Process32NextW(snapshot.get(),&entry));
  process_commit=std::max(process_commit,commit);working_set=std::max(working_set,rss);
  for(const auto& d:devices)if(d.backend_identity_verified){const auto marker=d.stable_id.find(":luid:");if(marker==std::string::npos)continue;auto value=pressure.dedicated_vram_bytes.find(d.stable_id.substr(marker+6));if(value!=pressure.dedicated_vram_bytes.end()){vram[d.name]=std::max(vram[d.name],value->second);++vram_samples;}}
  ++samples;
 }
 json::object json_value() const{
  json::object g;for(const auto& [name,value]:vram)g[name]=value;
  return {{"cpu_percent_sampled",cpu},{"host_used_ram_bytes_sampled",host_ram},{"harness_and_children_commit_bytes_sampled",process_commit},{"harness_and_children_working_set_bytes_sampled",working_set},{"adapter_dedicated_vram_bytes_sampled",std::move(g)},{"samples",samples},{"cpu_samples",cpu_samples},{"vram_samples",vram_samples},{"external_ffmpeg_observed",external_ffmpeg},{"nominal_sample_interval_ms",200},{"includes_warmup",true},{"foreground_active_samples",foreground_samples},{"foreground_unknown_samples",unknown_foreground_samples},{"no_foreground_window_samples",no_foreground_window_samples},{"foreground_message_samples",message_latency.size()},{"foreground_message_max_ms",message_latency.empty()?0:*std::max_element(message_latency.begin(),message_latency.end())}};
 }
};
}
int wmain(int argc,wchar_t** argv){try{
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
 if(argc<7||argc>8)throw std::runtime_error("usage: ow_flex_benchmark new-root source-or-generate minimum-jobs minimum-ms h264-backend:ordinal hevc-backend:ordinal [warmups]");
 const auto staging=data_directory().parent_path()/"Temp"/"ow-worker-flex-20261018-r2";
 const std::filesystem::path root=argv[1];
 if(!root.is_absolute()||!below(root,staging)||!std::filesystem::create_directory(root))throw std::runtime_error("New root below the approved staging directory required");
#ifndef OW_FLEX_BASELINE
 install_measurement_crash_trace(root);
#endif
 auto config=load_config(data_directory());
 if(sha256_file(config.ffmpeg)!="f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4")throw std::runtime_error("Reviewed FFmpeg hash mismatch");
 const auto installed_budget=config.budget;const auto installed_max_jobs=config.max_jobs;
 wchar_t auto_mode[8]{};const bool automatic=GetEnvironmentVariableW(L"OW_FLEX_AUTO",auto_mode,8)&&std::wstring(auto_mode)==L"1";
 if(automatic){
  MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(!GlobalMemoryStatusEx(&memory)||memory.dwMemoryLoad>=90)throw std::runtime_error("Insufficient quiet RAM headroom for a capacity sweep");
  config.budget.cpu_threads=std::clamp<std::uint64_t>(static_cast<std::uint64_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS))*9/10,1,64);
  const auto total=memory.ullTotalPhys/(1024*1024),available=memory.ullAvailPhys/(1024*1024);
  const auto free=available>config.reserve_ram_mb?available-config.reserve_ram_mb:0;
  const auto ram=total<=16384?free*9/10:std::min(total/2,free*3/4);
  if(ram<512)throw std::runtime_error("Insufficient RAM for app converter qualification");config.budget.ram_mb=ram/128*128;
  config.max_jobs=static_cast<std::size_t>(std::clamp<std::uint64_t>(config.budget.cpu_threads*2,4,32));
 }
 const auto trial_budget=config.budget;const auto trial_max_jobs=config.max_jobs;
 config.capacity_foreground=[staging]{try{
  const auto state=parse(read_file(staging/"foreground.json",4096)).as_object();const auto observed=static_cast<Tick>(number(state,"observed_utc_ms",INT64_MAX));
  const bool window=state.contains("window_present")?state.at("window_present").as_bool():true;
  return utc_ms()<observed||utc_ms()-observed>2000||state.at("foreground_active").as_bool()||(window&&(!state.at("message_response_known").as_bool()||number(state,"message_response_ms",1000)>=16));
 }catch(...){return true;}};
 if(config.capacity_foreground())throw std::runtime_error("Foreground observation requests capacity; measurement deferred");
 auto source=std::filesystem::path(argv[2]);
 if(source==L"generate"){
  source=root/"source.bin";const auto directory=root/"generation";std::filesystem::create_directory(directory);
  Child seed(config.ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"error",L"-f",L"lavfi",L"-i",L"testsrc2=size=1920x1080:rate=24",L"-f",L"lavfi",L"-i",L"sine=frequency=880:sample_rate=48000",L"-frames:v",L"72",L"-t",L"3",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",L"-pix_fmt",L"yuv420p",L"-c:a",L"aac",L"-f",L"mp4",source.wstring()},directory,{512,0,128,1},Priority::low);
  const auto deadline=monotonic_ms()+120000;
  for(;;){if(auto code=seed.exit_code()){if(*code)throw std::runtime_error("Synthetic generation failed");break;}if(monotonic_ms()>deadline)throw std::runtime_error("Synthetic generation timeout");Sleep(100);}seed.stop();
 }
 if(!source.is_absolute()||!below(source,staging))throw std::runtime_error("Source must stay in the approved measurement directory");
 config.root=root/"proof";std::filesystem::create_directory(config.root);
 auto qualification=config;bool baseline_single=false;
#ifndef OW_FLEX_BASELINE
 wchar_t fast[8]{};if(GetEnvironmentVariableW(L"OW_FLEX_FAST_PROOF",fast,8)&&std::wstring(fast)==L"1"){qualification.budget.cpu_threads=1;qualification.max_jobs=1;}
#endif
#ifdef OW_FLEX_BASELINE
 wchar_t single[8]{};baseline_single=GetEnvironmentVariableW(L"OW_FLEX_BASELINE_SINGLE",single,8)&&std::wstring(single)==L"1";
 if(baseline_single)qualification.budget.cpu_threads=1; // proof only; runtime hard budgets stay unchanged
#endif
 auto proof=probe_ffmpeg(qualification,{},[](const std::string& name,unsigned,unsigned){std::cout<<"PROBE "<<name<<std::endl;});
 if(!proof.complete||!proof.media_video)throw std::runtime_error("Current converter probe did not qualify app video");
 json::array profiles;for(const auto& p:proof.profiles)profiles.emplace_back(adaptive::profile_record(p));
 std::cout<<"CURRENT_PROOF "<<json::serialize(profiles)<<std::endl;
 std::map<adaptive::Codec,std::string> pins;
 for(const auto codec:{adaptive::Codec::h264,adaptive::Codec::hevc}){
  const auto token=utf8(argv[codec==adaptive::Codec::h264?5:6]);const auto colon=token.find(':');
  if(colon==std::string::npos)throw std::runtime_error("Explicit backend and ordinal required for identical-quality comparison");
  const auto backend=token.substr(0,colon);const auto ordinal=std::stoul(token.substr(colon+1));
  for(const auto& p:proof.profiles)if(p.codec==codec&&adaptive::backend_token(p.backend)==backend&&(p.backend==adaptive::Backend::nvidia_nvenc?p.backend_index:p.adapter_index)==ordinal){pins[codec]=p.stable_id;break;}
  if(!pins.contains(codec))throw std::runtime_error("Requested identical-quality backend not proven: "+token);
 }
 const auto devices=enumerate_video_devices();Peaks peaks;
 config.root=root/"worker";
 std::jthread sampler([&](std::stop_token stop){while(!stop.stop_requested()){peaks.sample(root,devices);for(int n=0;n<20&&!stop.stop_requested();++n)Sleep(10);}});
 json::object result;
 try{result=OfflineMediaHarness::run(config,proof,source,pins,std::stoul(argv[3]),static_cast<unsigned>(config.max_jobs),std::stoull(argv[4]),argc==8?std::stoul(argv[7]):2);}catch(...){sampler.request_stop();sampler.join();throw;}
 sampler.request_stop();sampler.join();
 if(peaks.external_ffmpeg)throw std::runtime_error("External FFmpeg overlapped this measurement; do not compare contaminated results");
 auto& jobs=result.at("jobs").as_array();std::vector<std::uint64_t> latency;double sum=0;
 for(const auto& job:jobs){const auto ms=job.as_object().at("wall_ms").as_int64();latency.push_back(static_cast<std::uint64_t>(ms));sum+=static_cast<double>(ms);}
 std::sort(latency.begin(),latency.end());
 const auto wall=result.at("wall_ms").as_int64();
 result["jobs_per_hour_observed_batch"]=static_cast<double>(jobs.size())*3600000.0/static_cast<double>(wall);
 result["job_wall_mean_ms"]=sum/static_cast<double>(jobs.size());result["job_wall_p50_ms"]=latency[latency.size()/2];result["job_wall_p95_ms"]=latency[std::min(latency.size()-1,(latency.size()*95)/100)];
 result["baseline_single_session_qualification"]=baseline_single;
 result["peaks"]=peaks.json_value();result["encoder_pins"]=json::object{{"h264",pins.at(adaptive::Codec::h264)},{"hevc",pins.at(adaptive::Codec::hevc)}};
 result["operator_limits"]=json::object{{"cpu_threads",installed_budget.cpu_threads},{"ram_mb",installed_budget.ram_mb},{"scratch_mb",installed_budget.scratch_mb},{"max_jobs",installed_max_jobs}};
 result["trial_limits"]=json::object{{"cpu_threads",trial_budget.cpu_threads},{"ram_mb",trial_budget.ram_mb},{"scratch_mb",trial_budget.scratch_mb},{"max_jobs",trial_max_jobs},{"automatic_requested",automatic}};
 result["source_asset_reproduction"]=result.at("source_bytes").as_uint64()==24609052&&result.at("source_sha256").as_string()=="43806f9d921afe0def16befac5e6a95374f9571f2ae480c6950e9e0f3f85ef27";
 std::ofstream(root/"result.json",std::ios::binary)<<json::serialize(result)<<'\n';
 std::cout<<"OFFLINE_RESULT "<<json::serialize(result)<<std::endl;
 wchar_t asset_path[32768]{};
 if(GetEnvironmentVariableW(L"OW_FLEX_ASSET",asset_path,32768)){
  const std::filesystem::path asset=asset_path;
  if(!below(asset,staging)||std::filesystem::file_size(asset)!=24609052||sha256_file(asset)!="43806f9d921afe0def16befac5e6a95374f9571f2ae480c6950e9e0f3f85ef27")throw std::runtime_error("Exact regression asset size/SHA mismatch");
  config.root=root/"actual-worker";Peaks actual_peaks;
  std::jthread actual_sampler([&](std::stop_token stop){while(!stop.stop_requested()){actual_peaks.sample(root,devices);for(int pause=0;pause<20&&!stop.stop_requested();++pause)Sleep(10);}});
  json::object actual;
  try{actual=OfflineMediaHarness::run(config,proof,asset,pins,1,1,0,0);}catch(...){actual_sampler.request_stop();actual_sampler.join();throw;}
  actual_sampler.request_stop();actual_sampler.join();actual["peaks"]=actual_peaks.json_value();actual["source_asset_reproduction"]=true;
  std::ofstream(root/"actual-result.json",std::ios::binary)<<json::serialize(actual)<<'\n';std::cout<<"OFFLINE_ASSET_RESULT "<<json::serialize(actual)<<std::endl;
  if(actual_peaks.external_ffmpeg)throw std::runtime_error("External FFmpeg overlapped the asset measurement");
 }
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
