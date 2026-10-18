// Opt-in isolated synthetic measurement, never registered as an automatic GPU test.
#include "process.hpp"
#include "hardware.hpp"
#include "ow/media_renditions.hpp"
#include "ow/adaptive_evidence.hpp"
#include "ow/child_memory.hpp"
#include <iostream>
#include <regex>
using namespace ow;using namespace ow::win;
namespace {
struct Run {DWORD exit;std::uint64_t peak;std::string log;};
Run run(const std::filesystem::path& exe,const std::vector<std::wstring>& args,const std::filesystem::path& dir,std::uint64_t cap){
 Child child(exe,args,dir,{cap,0,1024,64},Priority::maximum);
 const auto deadline=monotonic_ms()+90000;std::optional<DWORD> code;
 while(!(code=child.exit_code())){if(monotonic_ms()>=deadline)throw std::runtime_error("Measurement child timed out");Sleep(20);}
 const auto peak=child.peak_commit_mb();child.stop();return {*code,peak,read_file(dir/"process.log")};
}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=3)throw std::runtime_error("usage: ow_gpu_budget_probe approved-ffmpeg new-workspace");
 const std::filesystem::path exe=argv[1],root=argv[2];
 if(sha256_file(exe)!="f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4")throw std::runtime_error("Wrong approved FFmpeg hash");
 if(!std::filesystem::create_directory(root))throw std::runtime_error("Measurement workspace must be new");
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 auto devices=enumerate_video_devices();unsigned failures=0,count=0;
 for(const auto [width,height]:{std::pair{1280u,720u},{1536u,864u},{1568u,882u},{1920u,1080u},{2560u,1440u},{3840u,2160u}}){
  auto source=root/(std::to_string(width)+"x"+std::to_string(height));std::filesystem::create_directory(source);
  const auto seed=run(exe,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",L"testsrc2=size="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L":rate=24",L"-f",L"lavfi",L"-i",L"sine=frequency=880:sample_rate=48000",L"-t",L"3",L"-frames:v",L"72",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-threads",L"1",L"-pix_fmt",L"yuv420p",L"-c:a",L"aac",(source/"source.mp4").wstring()},source,2048);
  std::cout<<json::serialize(json::object{{"stage","source"},{"width",width},{"height",height},{"exit",seed.exit},{"peak_commit_mb",seed.peak}})<<std::endl;
  if(seed.exit)throw std::runtime_error("Synthetic source generation failed");
  for(const auto& device:devices){
   if(device.family!=adaptive::Backend::nvidia_nvenc||!device.backend_identity_verified)continue;
   for(const auto codec:{adaptive::Codec::h264,adaptive::Codec::hevc})for(const bool surfaces:{false,true}){
    // 1080p old-cap comparison; all other cases use the policy cap.
    for(const bool old:{true,false}){
     if(old&&(width!=1920||surfaces))continue;
     const auto memory=surfaces?adaptive::InputMemory::device:adaptive::InputMemory::host;
     const auto cap=old?512:adaptive::gpu_child_memory_mb(width,height,codec,memory);
     auto dir=source/(std::to_string(device.backend_index)+"-"+(codec==adaptive::Codec::h264?"h264":"hevc")+(surfaces?"-device":"-host")+(old?"-old":"-policy"));std::filesystem::create_directory(dir);
     std::filesystem::copy_file(source/"source.mp4",dir/"source.bin");
     adaptive::Profile profile;profile.backend=device.family;profile.codec=codec;profile.backend_index=device.backend_index;
     auto plan=rendition_plan(true,false,dir,profile);auto item=plan[codec==adaptive::Codec::h264?0:1];
     auto& args=item.arguments;auto level=std::find(args.begin(),args.end(),L"-loglevel");*(level+1)=L"info";
     if(surfaces){auto pixel=std::find(args.begin(),args.end(),L"-pix_fmt");args.erase(pixel,pixel+2);
      args.insert(std::find(args.begin(),args.end(),L"-noautorotate"),{L"-init_hw_device",L"cuda=ow:"+std::to_wstring(device.backend_index),L"-hwaccel",L"cuda",L"-hwaccel_device",L"ow",L"-hwaccel_output_format",L"cuda"});}
     auto encoded=run(exe,args,dir,cap);const auto reason=child_memory_failure(encoded.log,width,height,cap);
     std::filesystem::rename(dir/"process.log",dir/"encode.log");
     json::object record{{"stage","encode"},{"card",device.name},{"device_id",device.stable_id},{"width",width},{"height",height},{"codec",codec==adaptive::Codec::h264?"h264":"hevc"},{"input",surfaces?"device":"host"},{"old_cap",old},{"budget_mb",cap},{"peak_commit_mb",encoded.peak},{"exit",encoded.exit},{"classification",reason?*reason:""}};
     json::array command;for(const auto& arg:args)command.emplace_back(utf8(arg));record["argv"]=std::move(command);
     if(!encoded.exit){auto decode=run(exe,{L"-nostdin",L"-hide_banner",L"-loglevel",L"info",L"-xerror",L"-threads",L"1",L"-i",(dir/item.filename).wstring(),L"-threads",L"1",L"-f",L"null",L"-"},dir,cap);
      const bool valid=decode.exit==0&&adaptive::video_geometry_valid(decode.log,codec==adaptive::Codec::h264?"h264":"hevc",width,height)&&std::regex_search(decode.log,std::regex(R"(frame=\s*72(?:\s|$))"));
      record["decode_exit"]=decode.exit;record["valid_72_frames"]=valid;if(!old&&!valid)++failures;
     }else if(!old)++failures;
     ++count;std::cout<<json::serialize(record)<<std::endl;
    }
   }
  }
 }
 std::cout<<json::serialize(json::object{{"stage","summary"},{"encodes",count},{"policy_failures",failures}})<<std::endl;
 return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}}
