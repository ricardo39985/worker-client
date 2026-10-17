#include "process.hpp"
#include "ow/media_renditions.hpp"
#include "ow/rendition_pipeline.hpp"
#include "worker.hpp"
#include "hardware.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
using namespace ow;using namespace ow::win;
namespace {
void run_child(const std::filesystem::path& ffmpeg,const std::vector<std::wstring>& args,const std::filesystem::path& root,std::stop_token stop={},const std::function<void()>& tick={}) {
 Child child(ffmpeg,args,root,{512,0,512,1},Priority::normal);auto until=monotonic_ms()+30000;
 for(;;){if(tick)tick();if(stop.stop_requested()||monotonic_ms()>=until)throw std::runtime_error("Smoke conversion cancelled or timed out");
  if(auto exit=child.exit_code()){if(*exit)throw std::runtime_error("Smoke FFmpeg failed");return;}Sleep(10);}
}
struct Metadata {std::string role,hash;std::uintmax_t bytes;};
}
int wmain(int argc,wchar_t** argv) {
 try{
  if(argc!=4)throw std::runtime_error("Usage: ow_pipeline_smoke ffmpeg.exe source.mp4 NEW-workspace");
  const std::filesystem::path ffmpeg=argv[1],source=argv[2],root=argv[3];
  if(std::filesystem::exists(root))throw std::runtime_error("Smoke workspace must not exist");
  std::filesystem::create_directories(root);std::vector<Metadata> baseline;
  for(bool parallel:{false,true}){
   auto workspace=root/(parallel?"overlapped":"serial");std::filesystem::create_directory(workspace);
   std::filesystem::copy_file(source,workspace/"source.bin");auto plan=rendition_plan(true,false,workspace);
   std::atomic_bool transferring{},overlapped{};
   auto prepare=[&](const Rendition& item,std::stop_token stop){run_child(ffmpeg,item.arguments,workspace,stop,[&]{if(transferring)overlapped=true;});};
   // A local streaming hash and fixed simulated network delay, NOT a real upload.
   auto deliver=[&](const Rendition& item,std::stop_token stop){
    transferring=true;auto output=workspace/item.filename;
    auto hash=sha256_file(output,[&](std::uint64_t){if(stop.stop_requested())throw std::runtime_error("Smoke transfer cancelled");});
    auto until=monotonic_ms()+500;while(monotonic_ms()<until){if(stop.stop_requested())throw std::runtime_error("Smoke transfer cancelled");Sleep(10);}
    transferring=false;return Metadata{item.role,hash,std::filesystem::file_size(output)};
   };
   auto start=monotonic_ms();std::vector<Metadata> results;
   if(parallel)results=run_rendition_pipeline(plan,prepare,deliver);
   else for(const auto& item:plan){prepare(item,{});results.push_back(deliver(item,{}));}
   std::cout<<(parallel?"overlapped":"serial")<<" elapsed_ms="<<(monotonic_ms()-start)<<" artifacts="<<results.size()<<" overlap_observed="<<overlapped<<"\n";
   if(results.size()!=3||transferring)throw std::runtime_error("Incomplete smoke pipeline");
   if(parallel&&!overlapped)throw std::runtime_error("No encode/transfer overlap observed");
   for(std::size_t i=0;i<results.size();++i){
    const auto& m=results[i];if(!m.bytes||m.hash.size()!=64)throw std::runtime_error("Invalid artifact metadata");
    if(parallel&&(m.role!=baseline[i].role||m.bytes!=baseline[i].bytes||m.hash!=baseline[i].hash))throw std::runtime_error("Rendition output changed after overlap");
    run_child(ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(workspace/plan[i].filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},workspace);
    std::cout<<m.role<<" bytes="<<m.bytes<<" sha256="<<m.hash<<" decoded_ok\n";
   }
   if(!parallel)baseline=std::move(results);
  }
  auto retry=root/"validated-cpu-retry";std::filesystem::create_directory(retry);std::filesystem::copy_file(source,retry/"source.bin");
  auto cpu_plan=rendition_plan(true,false,retry);const auto cpu_rendition=cpu_plan.front();
  auto failing_hardware=cpu_rendition.arguments;auto encoder=std::find(failing_hardware.begin(),failing_hardware.end(),L"-c:v");
  if(encoder==failing_hardware.end()||encoder+1==failing_hardware.end())throw std::runtime_error("Smoke hardware command malformed");
  *(encoder+1)=L"ow_unavailable_encoder";const auto retry_output=retry/cpu_rendition.filename;
  std::size_t upload_observations=0;bool rejected_partial=false;
  const bool used_hardware=adaptive::run_with_cpu_fallback(retry_output,true,[]{return false;},[&](bool hardware){
   run_child(ffmpeg,hardware?failing_hardware:cpu_rendition.arguments,retry);
  },[&](const std::filesystem::path& path){
   if(!std::filesystem::is_regular_file(path)||!std::filesystem::file_size(path))return false;
   try{run_child(ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",path.wstring(),L"-map",L"0:v:0",L"-f",L"framemd5",L"-"},retry);}
   catch(...){return false;}
   const auto hashes=read_file(retry/"process.log");std::istringstream rows(hashes);std::string line;std::size_t frames=0;long long previous_pts=-1;bool dimensions=false,timing=false;
   while(std::getline(rows,line)){
    if(line.find("#dimensions 0: 640x360")!=std::string::npos)dimensions=true;
    if(line.find("#tb 0:")!=std::string::npos)timing=true;
    if(line.empty()||line.front()=='#')continue;
    std::istringstream fields(line);std::string stream,pts;std::getline(fields,stream,',');std::getline(fields,pts,',');
    try{const auto value=std::stoll(pts);if(value<=previous_pts)return false;previous_pts=value;++frames;}catch(...){return false;}
   }
   try{run_child(ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",path.wstring(),L"-f",L"null",L"-"},retry);}
   catch(...){return false;}
   const auto info=read_file(retry/"process.log");
   const bool valid=dimensions&&timing&&frames==72&&info.find("Input #0, mov,mp4")!=std::string::npos&&
    info.find("Video: h264")!=std::string::npos&&info.find("640x360")!=std::string::npos&&
    info.find("Audio: aac")!=std::string::npos&&std::regex_search(info,std::regex(R"(frame=\s*72(?:\s|$))"));
   if(!valid)rejected_partial=true;
   return valid;
  });
  if(used_hardware||rejected_partial)throw std::runtime_error("Synthetic failure did not produce a validated CPU retry");
  const auto retry_hash=sha256_file(retry_output);if(retry_hash.size()!=64)throw std::runtime_error("CPU retry artifact hash invalid");
  ++upload_observations;if(upload_observations!=1)throw std::runtime_error("Unvalidated or duplicate artifact reached upload observation");
  std::cout<<"adaptive_failure_cpu_retry validated_upload_observations="<<upload_observations<<" sha256="<<retry_hash<<"\n";
  Config config;config.ffmpeg=ffmpeg;config.ffmpeg_sha256=sha256_file(ffmpeg);config.root=root/"profile-doctor";config.budget={2048,0,4096,2};config.max_jobs=2;
  auto probed=probe_ffmpeg(config);if(!probed.media_video||probed.profiles.empty())throw std::runtime_error("Production profile probe did not qualify safe runtime profiles");
  std::uint64_t vram_cap=0;for(const auto& profile:probed.profiles)if(profile.backend!=adaptive::Backend::cpu)vram_cap+=profile.vram_mb*profile.max_concurrency;
  adaptive::Scheduler runtime(adaptive::Limits{2,2048,vram_cap,4096,2},probed.profiles);
  auto reservation=runtime.reserve({adaptive::Codec::h264,adaptive::Codec::hevc});if(!reservation)throw std::runtime_error("Production scheduler denied probed smoke profiles");
  auto selected=reservation.profiles();auto runtime_root=root/"production-scheduler-runtime";std::filesystem::create_directory(runtime_root);std::filesystem::copy_file(source,runtime_root/"source.bin");
  for(const auto& profile:selected)std::cout<<"runtime_selected_profile device="<<profile.stable_id<<" codec="<<(profile.codec==adaptive::Codec::h264?"h264":"hevc")<<" backend="<<static_cast<int>(profile.backend)<<" measured_fps="<<profile.measured_frames_per_second<<" admitted_vram_mb="<<profile.vram_mb<<"\n";
  auto selected_plan=rendition_plan(true,false,runtime_root,selected);auto fallback_plan=rendition_plan(true,false,runtime_root);
  std::vector<Metadata> runtime_outputs;std::uint64_t gpu_telemetry=0;std::size_t upload_observed=0;
  auto device_for=[&](const adaptive::Profile& profile)->std::optional<adaptive::Device>{for(const auto& device:enumerate_video_devices())if(device.stable_id==profile.stable_id)return device;return {};};
  auto run_profile_child=[&](const std::vector<std::wstring>& args,const std::filesystem::path& path,const adaptive::Profile* profile,std::stop_token stop){
   const auto device=profile&&profile->backend!=adaptive::Backend::cpu?device_for(*profile):std::optional<adaptive::Device>{};
   if(profile&&profile->backend!=adaptive::Backend::cpu&&!device)throw std::runtime_error("Selected GPU profile lost adapter identity before execution");
   Child child(ffmpeg,args,path,{512,0,512,1},Priority::normal);std::uint64_t peak=0;auto until=monotonic_ms()+30000;
   for(;;){if(stop.stop_requested())throw std::runtime_error("Adaptive smoke cancelled while child was contained");if(device){auto sample=sample_gpu_memory(child.process_id(),*device);if(sample){peak=std::max(peak,sample->child_bytes);if(peak>profile->vram_mb*1024*1024)throw std::runtime_error("Observed GPU allocation exceeded admitted profile reservation");}}
    if(auto code=child.exit_code()){if(*code)throw std::runtime_error("Selected profile FFmpeg process failed");break;}if(monotonic_ms()>=until)throw std::runtime_error("Selected profile child timed out");Sleep(25);}
   if(device){if(!peak)throw std::runtime_error("Selected GPU profile had no matching child dedicated-memory telemetry");++gpu_telemetry;std::cout<<"runtime_gpu_child_telemetry device="<<profile->stable_id<<" dedicated_peak_bytes="<<peak<<"\n";}
  };
  auto prepare_selected=[&](const Rendition& item,std::stop_token stop){
   auto cpu=std::find_if(fallback_plan.begin(),fallback_plan.end(),[&](const auto& x){return x.role==item.role;});if(cpu==fallback_plan.end())throw std::runtime_error("Production CPU retry plan missing");
   auto profile=std::find_if(selected.begin(),selected.end(),[&](const auto& x){return (item.role=="feed"&&x.codec==adaptive::Codec::h264)||(item.role=="feed_optimized"&&x.codec==adaptive::Codec::hevc);});
   const bool gpu=item.hardware&&profile!=selected.end()&&profile->backend!=adaptive::Backend::cpu;
   if(gpu){const bool used=adaptive::run_with_cpu_fallback(runtime_root/item.filename,true,[&]{return stop.stop_requested();},[&](bool hardware){
     if(!hardware&&!reservation.acquire_cpu_fallback([&]{return stop.stop_requested();},30000))throw std::runtime_error("CPU retry slot unavailable within bounded wait");
     run_profile_child(hardware?item.arguments:cpu->arguments,runtime_root,hardware?&*profile:nullptr,stop);
    },[&](const std::filesystem::path& file){if(stop.stop_requested())return false;try{run_child(ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",file.wstring(),L"-threads",L"1",L"-f",L"null",L"-"},runtime_root);return std::filesystem::file_size(file)>0;}catch(...){return false;}});
    if(!used)std::cout<<"runtime_gpu_selection_failed_safe_cpu_retry role="<<item.role<<"\n";
   }else run_profile_child(item.arguments,runtime_root,nullptr,stop);
  };
  auto deliver_local=[&](const Rendition& item,std::stop_token stop){if(stop.stop_requested())throw std::runtime_error("Upload observation cancelled");auto file=runtime_root/item.filename;run_child(ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",file.wstring(),L"-threads",L"1",L"-f",L"null",L"-"},runtime_root,stop);auto digest=sha256_file(file);++upload_observed;return Metadata{item.role,digest,std::filesystem::file_size(file)};};
  runtime_outputs=run_rendition_pipeline(selected_plan,prepare_selected,deliver_local);
  for(std::size_t i=0;i<runtime_outputs.size();++i){run_child(ffmpeg,{L"-nostdin",L"-v",L"error",L"-xerror",L"-threads",L"1",L"-i",(runtime_root/selected_plan[i].filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},runtime_root);}
  std::cout<<"production_scheduler_runtime_profiles="<<selected.size()<<" gpu_child_telemetry="<<gpu_telemetry<<" local_upload_observations="<<upload_observed<<" active_reservations="<<runtime.active_jobs()<<"\n";
  if(upload_observed!=runtime_outputs.size()||runtime.active_jobs()!=1)throw std::runtime_error("Production scheduler smoke output validation or held reservation failed");
  reservation.release();if(runtime.active_jobs()!=0)throw std::runtime_error("Scheduler reservation did not release after pipeline drain");
  std::cout<<"PIPELINE_SMOKE_PASS (real native FFmpeg; simulated transfer latency only)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
