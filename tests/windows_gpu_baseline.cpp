#include "process.hpp"
#include "hardware.hpp"
#include "ow/media_renditions.hpp"
#include "ow/adaptive_evidence.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
using namespace ow;using namespace ow::win;
namespace {
// Offline evidence tool, never a capability probe or coordinator client.
const Resources limits{2048,0,8192,1};
std::string run(const std::filesystem::path& ffmpeg,const std::vector<std::wstring>& args,
 const std::filesystem::path& work,const char* stage,const std::string& artifact,StageTotals* totals=nullptr){
 StageTimer timing(stage,artifact,[&](const StageSample& sample){
  if(totals)totals->add(sample);std::cout<<stage_record(sample)<<std::endl;
 },timing_wall_us,thread_cpu_us,true);
 Child child(ffmpeg,args,work,limits,Priority::low);ChildTimingCapture capture(child,&timing);
 const auto until=monotonic_ms()+600000;
 for(;;){if(auto code=child.exit_code()){if(*code)throw std::runtime_error("FFmpeg failed; retained process.log in isolated workspace");break;}
  if(monotonic_ms()>=until)throw std::runtime_error("Baseline stage exceeded ten minutes");Sleep(100);}
 child.stop();return read_file(work/"process.log");
}
std::vector<std::wstring> validation(const std::filesystem::path& path){
 return {L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",path.wstring(),L"-threads",L"1",L"-f",L"null",L"-"};
}
std::vector<std::string> frame_timing(const std::string& log){
 std::vector<std::string> result;std::istringstream rows(log);std::string line;
 while(std::getline(rows,line)){
  if(line.rfind("#tb ",0)==0||line.rfind("#dimensions ",0)==0)result.push_back(line);
  if(!std::regex_search(line,std::regex(R"(^[0-9]+,)")))continue;
  // framehash rows: stream, dts, pts, duration, bytes, hash. Compare timing,
  // count and geometry; lossy output must not be expected to have source hashes.
  std::size_t pos=0;for(unsigned field=0;field<4;++field){pos=line.find(',',pos);if(pos==std::string::npos)break;++pos;}
  if(pos!=std::string::npos&&pos)result.push_back(line.substr(0,pos));
 }
 if(result.size()<3)throw std::runtime_error("No complete frame timing evidence");return result;
}
}
int wmain(int argc,wchar_t** argv){
 try{
  if(argc!=6)throw std::runtime_error("Usage: ow_gpu_baseline ffmpeg.exe real-1080.mp4 real-4k.mp4 NEW-workspace approved-sha256");
  const std::filesystem::path ffmpeg=argv[1],root=argv[4];
  if(!ffmpeg.is_absolute()||sha256_file(ffmpeg)!=utf8(argv[5]))throw std::runtime_error("Approved FFmpeg hash/path mismatch");
  const auto devices=enumerate_video_devices();std::optional<adaptive::Device> device;
  for(const auto& d:devices)if(d.family==adaptive::Backend::nvidia_nvenc&&d.backend_identity_verified&&d.name.find("RTX 3050")!=std::string::npos){
   if(device)throw std::runtime_error("Ambiguous RTX 3050 selection");device=d;
  }
  if(!device)throw std::runtime_error("No physically mapped RTX 3050; no other card will be used");
  if(!root.is_absolute()||!std::filesystem::create_directory(root))throw std::runtime_error("Workspace must be new and absolute");
  std::cout<<"BASELINE software_decode host_frames nvenc full_size no_scaler priority=low CPU_threads=1 RAM_MB=2048 poll_ms=100\n";
  std::cout<<"DEVICE "<<device->name<<" ordinal="<<device->backend_index<<" physical_id="<<device->physical_id<<std::endl;
  for(unsigned source_index=0;source_index<2;++source_index){
   const std::filesystem::path source=argv[2+source_index];const auto width=source_index?3840u:1920u,height=source_index?2160u:1080u;
   if(!source.is_absolute()||!std::filesystem::is_regular_file(source))throw std::runtime_error("Real source must be an existing absolute file");
   auto source_root=root/(source_index?"4k":"1080p");std::filesystem::create_directory(source_root);
   std::filesystem::copy_file(source,source_root/"source.bin");
   auto source_args=validation(source_root/"source.bin");source_args.erase(source_args.end()-3,source_args.end());
   source_args.insert(std::find(source_args.begin(),source_args.end(),L"-i"),L"-noautorotate");
   source_args.insert(source_args.end(),{L"-map",L"0:v:0",L"-enc_time_base",L"1:1000000",L"-fps_mode",L"passthrough",L"-f",L"framehash",L"-"});
   const auto source_log=run(ffmpeg,source_args,source_root,"parity_source","source.bin");
   const auto input_info=source_log.substr(0,source_log.find("Output #0"));
   if(!std::regex_search(input_info,std::regex(R"(Video:[^\r\n]*\byuv420p(?:[ ,(]|$))"))||
      input_info.find("bt2020")!=std::string::npos||input_info.find("smpte2084")!=std::string::npos||input_info.find("arib-std-b67")!=std::string::npos)
    throw std::runtime_error("Baseline requires SDR 8-bit yuv420p test media");
   const auto original_frames=frame_timing(source_log);
   if(original_frames.size()<902)throw std::runtime_error("Use at least 900 source frames to dilute launch and polling");
   for(unsigned repeat=0;repeat<3;++repeat){
    auto work=source_root/("run-"+std::to_string(repeat));std::filesystem::create_directory(work);
    std::filesystem::copy_file(source,work/"source.bin");
    StageTotals totals;const auto started=timing_wall_us();const auto cpu_start=thread_cpu_us();
    adaptive::Profile h264;h264.backend=adaptive::Backend::nvidia_nvenc;h264.codec=adaptive::Codec::h264;h264.backend_index=device->backend_index;
    auto hevc=h264;hevc.codec=adaptive::Codec::hevc;
    const auto plan=rendition_plan(true,false,work,std::vector<adaptive::Profile>{h264,hevc});
    std::cout<<"JOB source="<<(source_index?"4k":"1080p")<<" repeat="<<repeat<<std::endl;
    for(const auto& item:plan){
     run(ffmpeg,item.arguments,work,item.role=="thumbnail"?"poster":"transcode",item.filename,&totals);
     if(item.hardware){auto info=run(ffmpeg,validation(work/item.filename),work,"validation",item.filename,&totals);
      if(!adaptive::video_geometry_valid(info,item.role=="feed"?"h264":"hevc",width,height))throw std::runtime_error("Output codec or full-size geometry mismatch");}
    }
    const auto cpu_end=thread_cpu_us();
    std::cout<<totals.record(timing_wall_us()-started,cpu_start&&cpu_end?std::optional<std::uint64_t>{*cpu_end-*cpu_start}:std::nullopt)<<std::endl;
    // Expensive parity evidence is outside job timing, never hidden in validation.
    for(const auto& item:plan)if(item.hardware){
     auto args=source_args;auto input=std::find(args.begin(),args.end(),L"-i");*(input+1)=(work/item.filename).wstring();
     auto frames=frame_timing(run(ffmpeg,args,work,"parity_frames",item.filename));
     if(frames!=original_frames)throw std::runtime_error("Source/output frame count, dimensions or timing changed");
     std::cout<<"PARITY artifact="<<item.filename<<" frame_timing=equal frames="<<frames.size()-2<<std::endl;
     if(repeat){
      auto reference=source_root/"run-0"/item.filename;
      for(const auto* metric:{L"ssim",L"psnr"}){
       const auto info=run(ffmpeg,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-threads",L"1",L"-i",reference.wstring(),L"-threads",L"1",L"-i",(work/item.filename).wstring(),L"-filter_complex_threads",L"1",L"-lavfi",metric,L"-an",L"-f",L"null",L"-"},work,"quality",item.filename);
       std::istringstream rows(info);std::string line;bool found=false;
       while(std::getline(rows,line))if(line.find("SSIM Y:")!=std::string::npos||line.find("PSNR y:")!=std::string::npos){std::cout<<"QUALITY "<<line<<std::endl;found=true;}
       if(!found)throw std::runtime_error("Quality number missing");
      }
      if(sha256_file(reference)!=sha256_file(work/item.filename))throw std::runtime_error("Timing-only repeat changed encoded bytes; inspect quality evidence");
     }
    }
   }
  }
  std::cout<<"BASELINE_PASS (offline serial compute only; no network transfer or scheduler claim)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
