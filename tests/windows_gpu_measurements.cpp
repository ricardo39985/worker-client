// Offline experiment only. No coordinator, configuration or installation access.
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
const Resources measured_limits{2048,0,16384,1};
bool normal_fixture=false;
std::string run(const std::filesystem::path& exe,const std::vector<std::wstring>& args,
 const std::filesystem::path& work,const std::string& stage,const std::string& artifact,
 StageTotals* totals=nullptr,bool measured=true){
 StageTimer timer(stage,artifact,[&](const StageSample& s){if(totals)totals->add(s);std::cout<<stage_record(s)<<std::endl;},timing_wall_us,thread_cpu_us,true);
 Child child(exe,args,work,measured?measured_limits:Resources{4096,0,16384,8},measured&&!normal_fixture?Priority::low:Priority::normal);
 ChildTimingCapture capture(child,&timer);const auto until=monotonic_ms()+900000;
 for(;;){if(auto code=child.exit_code()){if(*code){std::cerr<<read_file(work/"process.log",16*1024*1024)<<std::endl;throw std::runtime_error("FFmpeg exit "+std::to_string(*code));}break;}
  if(monotonic_ms()>=until)throw std::runtime_error("Measurement exceeded fifteen minutes");Sleep(100);}
 child.stop();auto log=read_file(work/"process.log",16*1024*1024);
 std::ofstream(work/(stage+"-"+artifact+".log"),std::ios::binary)<<log;return log;
}
std::vector<std::wstring> device_args(std::vector<std::wstring> args,unsigned ordinal){
 args.insert(std::find(args.begin(),args.end(),L"-i"),{L"-hwaccel",L"cuda",L"-hwaccel_device",std::to_wstring(ordinal),L"-hwaccel_output_format",L"cuda"});
 auto pix=std::find(args.begin(),args.end(),L"-pix_fmt");*(pix+1)=L"cuda";return args;
}
std::vector<std::wstring> validate(const std::filesystem::path& file){return {L"-nostdin",L"-hide_banner",L"-v",L"info",L"-xerror",L"-threads",L"1",L"-i",file.wstring(),L"-threads",L"1",L"-f",L"null",L"-"};}
std::vector<std::string> frames(const std::string& log){
 std::vector<std::string> out;std::istringstream rows(log);std::string line;
 while(std::getline(rows,line)){
  if(line.rfind("#tb ",0)==0||line.rfind("#dimensions ",0)==0)out.push_back(line);
  if(!std::regex_search(line,std::regex(R"(^[0-9]+,)")))continue;
  std::size_t pos=0;for(unsigned n=0;n<4;++n){pos=line.find(',',pos);if(pos==std::string::npos)break;++pos;}if(pos!=std::string::npos&&pos)out.push_back(line.substr(0,pos));
 }return out;
}
std::vector<std::wstring> frame_args(const std::filesystem::path& file){return {L"-nostdin",L"-hide_banner",L"-v",L"error",L"-nostats",L"-threads",L"4",L"-noautorotate",L"-i",file.wstring(),L"-map",L"0:v:0",L"-threads",L"4",L"-enc_time_base",L"1:1000000",L"-fps_mode",L"passthrough",L"-f",L"framehash",L"-"};}
}
int wmain(int argc,wchar_t** argv){try{
 if(argc!=5)throw std::runtime_error("Usage: ow_gpu_measurements pinned-ffmpeg existing-new-empty-workspace mode(generate|baseline|device|shared|quality|poster-reuse|negotiation|source-parity|baseline-full|fused-full|baseline-normal|fused-normal) repeats");
 const std::filesystem::path exe=argv[1],root=argv[2];const auto mode=utf8(argv[3]);const unsigned repeats=std::stoul(argv[4]);normal_fixture=mode.find("normal")!=std::string::npos;
 if(!root.is_absolute()||!std::filesystem::is_directory(root)||sha256_file(exe)!="f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4")throw std::runtime_error("Pinned executable or workspace mismatch");
 std::optional<adaptive::Device> gpu;for(const auto& d:enumerate_video_devices())if(d.backend_identity_verified&&d.family==adaptive::Backend::nvidia_nvenc&&d.name.find("RTX 3050")!=std::string::npos){if(gpu)throw std::runtime_error("Ambiguous 3050");gpu=d;}
 if(!gpu)throw std::runtime_error("Physically mapped RTX 3050 unavailable");
 std::cout<<"EXPERIMENT mode="<<mode<<" gpu="<<gpu->backend_index<<" card="<<gpu->name<<" physical_id="<<gpu->physical_id<<" measured_RAM_MB=2048 CPU=1 priority="<<(normal_fixture?"normal":"low")<<" poll_ms=100 frames=900 fps=30\n";
 for(unsigned size=0;size<2;++size){const auto label=size?"4k":"1080p";auto base=root/label;std::filesystem::create_directories(base);
  auto source=base/"source.bin";const unsigned width=size?3840:1920,height=size?2160:1080;
  if(mode=="generate"){
   if(std::filesystem::exists(source))throw std::runtime_error("Refusing to overwrite generated source");
   run(exe,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-f",L"lavfi",L"-i",L"testsrc2=size="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L":rate=30",L"-f",L"lavfi",L"-i",L"sine=frequency=997:sample_rate=48000",L"-t",L"30",L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-crf",L"18",L"-pix_fmt",L"yuv420p",L"-threads",L"4",L"-c:a",L"aac",L"-f",L"mp4",source.wstring()},base,"generation","source",nullptr,false);continue;
  }
  if(mode=="poster-reuse"){
   for(unsigned rep=0;rep<7;++rep)for(bool fused:{false,true}){
    auto work=base/(std::string(fused?"poster-fused-":"poster-separate-")+std::to_string(rep));if(!std::filesystem::create_directory(work))throw std::runtime_error("Poster experiment exists");
    std::filesystem::create_hard_link(source,work/"source.bin");StageTotals totals;const auto start=timing_wall_us();const auto cpu_start=thread_cpu_us();
    std::cout<<"JOB size="<<label<<" mode="<<(fused?"poster-fused":"poster-separate")<<" repeat="<<rep<<std::endl;
    run(exe,rendition_inspection_arguments(work,fused),work,fused?"inspection_poster":"input_inspection","source.bin",&totals);
    if(!fused)run(exe,rendition_plan(true,false,work).back().arguments,work,"poster","poster.webp",&totals);
    const auto cpu_end=thread_cpu_us();std::cout<<totals.record(timing_wall_us()-start,cpu_start&&cpu_end?std::optional<std::uint64_t>{*cpu_end-*cpu_start}:std::nullopt)<<std::endl;
    if(fused&&sha256_file(work/"poster.webp")!=sha256_file(base/("poster-separate-"+std::to_string(rep))/"poster.webp"))throw std::runtime_error("Poster fusion changed bytes");
   }continue;
  }
  if(mode=="negotiation"||mode=="source-parity"){
   for(const auto& input_file:{source,base/"baseline-0"/"optimized.mp4"})for(bool hevc:{false,true}){
    auto work=base/(std::string(mode=="source-parity"?"source-parity-":"trace-")+(input_file==source?"h264-":"hevc-")+(hevc?"hevc":"h264"));std::filesystem::create_directory(work);std::filesystem::create_hard_link(input_file,work/"source.bin");
    auto info_args=rendition_inspection_arguments(work);const auto info=run(exe,info_args,work,"inspection","source.bin",nullptr,false);
    adaptive::Profile p;p.backend=adaptive::Backend::nvidia_nvenc;p.backend_index=gpu->backend_index;p.codec=hevc?adaptive::Codec::hevc:adaptive::Codec::h264;
    const std::vector<adaptive::Profile> profiles{p};auto plan=rendition_plan(true,false,work,profiles,info);auto args=plan[hevc?1:0].arguments;
    if(args!=device_args(rendition_plan(true,false,work,profiles)[hevc?1:0].arguments,gpu->backend_index))throw std::runtime_error("Production and experiment command mismatch");
    if(mode=="source-parity"){
     auto reference=rendition_plan(true,false,work,profiles)[hevc?1:0].arguments;
     reference.back()=(work/"reference.mp4").wstring();reference.insert(reference.end()-1,{L"-frames:v",L"72",L"-an"});
     args.insert(args.end()-1,{L"-frames:v",L"72",L"-an"});
     run(exe,reference,work,"source_parity_encode","reference");run(exe,args,work,"source_parity_encode","candidate");
     const auto candidate=work/(hevc?"optimized.mp4":"feed.mp4");
     run(exe,validate(work/"reference.mp4"),work,"source_parity_validation","reference");run(exe,validate(candidate),work,"source_parity_validation","candidate");
     const auto metrics=run(exe,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-nostats",L"-threads",L"4",L"-i",(work/"reference.mp4").wstring(),L"-threads",L"4",L"-i",candidate.wstring(),L"-filter_complex_threads",L"4",L"-lavfi",L"[0:v]split=2[a][b];[1:v]split=2[c][d];[a][c]ssim[s];[b][d]psnr[p]",L"-map",L"[s]",L"-map",L"[p]",L"-an",L"-f",L"null",L"-"},work,"source_parity_quality","video",nullptr,false);
     std::istringstream rows(metrics);std::string line;unsigned count=0;
     while(std::getline(rows,line))if(line.find("SSIM Y:")!=std::string::npos||line.find("PSNR y:")!=std::string::npos){std::cout<<"SOURCE_QUALITY size="<<label<<" input="<<(input_file==source?"h264":"hevc")<<" output="<<(hevc?"hevc":"h264")<<" frames=72 "<<line<<std::endl;++count;}
     if(count!=2||sha256_file(candidate)!=sha256_file(work/"reference.mp4"))throw std::runtime_error("Source-codec parity failed");
     continue;
    }
    auto level=std::find(args.begin(),args.end(),L"-loglevel");*(level+1)=L"debug";args.insert(args.end()-1,{L"-frames:v",L"72"});
    const auto log=run(exe,args,work,"negotiation",hevc?"hevc":"h264");
    if(log.find("pixfmt:cuda")==std::string::npos||log.find("hwdownload")!=std::string::npos||log.find("auto_scale")!=std::string::npos)throw std::runtime_error("Device-frame graph evidence missing or a host conversion appeared");
    auto validation_log=run(exe,validate(work/(hevc?"optimized.mp4":"feed.mp4")),work,"trace_validation",hevc?"hevc":"h264");
    if(!adaptive::video_geometry_valid(validation_log,hevc?"hevc":"h264",width,height))throw std::runtime_error("Negotiation output failed full software validation");
    std::cout<<"NEGOTIATION size="<<label<<" input="<<(input_file==source?"h264":"hevc")<<" output="<<(hevc?"hevc":"h264")<<" pixfmt=cuda host_conversion=none production_arguments=equal"<<std::endl;
   }continue;
  }
  if(mode=="quality"){
   const auto original=frames(run(exe,frame_args(source),base,"parity","source",nullptr,false));
   if(original.size()!=902)throw std::runtime_error("Expected exactly 900 source frames");
   for(unsigned rep=0;rep<repeats;++rep)for(const auto variant:{"baseline","device","shared"})for(const auto file:{"feed.mp4","optimized.mp4"}){
    auto work=base/(std::string(variant)+"-"+std::to_string(rep));auto output=work/file;
    auto reference=base/"baseline-0"/file;
    const bool equal=sha256_file(reference)==sha256_file(output);
    // Validate each distinct byte sequence once. All outputs were already
    // fully software-decoded inside their timed jobs, including repetitions.
    if((rep==0&&std::string(variant)=="baseline")||!equal)
     if(frames(run(exe,frame_args(output),work,"parity",file,nullptr,false))!=original)throw std::runtime_error("Frame count, geometry or timestamps changed");
    std::cout<<"PARITY size="<<label<<" variant="<<variant<<" repeat="<<rep<<" file="<<file<<" frames=900 geometry_timing=equal bytes_equal="<<(equal?"true":"false")<<std::endl;
    if(equal&&(rep||std::string(variant)=="baseline")){
     std::cout<<"QUALITY_IDENTITY size="<<label<<" variant="<<variant<<" repeat="<<rep<<" file="<<file<<" validated_reference_bytes=equal"<<std::endl;continue;
    }
    auto log=run(exe,{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-nostats",L"-threads",L"4",L"-i",reference.wstring(),L"-threads",L"4",L"-i",output.wstring(),L"-filter_complex_threads",L"4",L"-lavfi",L"[0:v]split=2[a][b];[1:v]split=2[c][d];[a][c]ssim[s];[b][d]psnr[p]",L"-map",L"[s]",L"-map",L"[p]",L"-an",L"-f",L"null",L"-"},work,"quality",file,nullptr,false);
    std::istringstream rows(log);std::string line;unsigned metrics=0;while(std::getline(rows,line))if(line.find("SSIM Y:")!=std::string::npos||line.find("PSNR y:")!=std::string::npos){std::cout<<"QUALITY size="<<label<<" variant="<<variant<<" repeat="<<rep<<" file="<<file<<" "<<line<<std::endl;++metrics;}
    if(metrics!=2)throw std::runtime_error("Missing quality metric");
   }continue;
  }
  const bool full=mode=="baseline-full"||mode=="fused-full"||mode=="baseline-normal"||mode=="fused-normal";
  const bool fused=mode=="fused-full"||mode=="fused-normal";
  if(!full&&mode!="baseline"&&mode!="device"&&mode!="shared")throw std::runtime_error("Unknown experiment");
  for(unsigned rep=0;rep<repeats;++rep){auto work=base/(mode+"-"+std::to_string(rep));if(!std::filesystem::create_directory(work))throw std::runtime_error("Refusing to overwrite a measurement");
   std::filesystem::create_hard_link(source,work/"source.bin");
   adaptive::Profile h;h.backend=adaptive::Backend::nvidia_nvenc;h.codec=adaptive::Codec::h264;h.backend_index=gpu->backend_index;auto e=h;e.codec=adaptive::Codec::hevc;
   auto plan=rendition_plan(true,false,work,std::vector<adaptive::Profile>{h,e});
   if(!full&&mode!="baseline")for(unsigned n=0;n<2;++n)plan[n].arguments=device_args(plan[n].arguments,gpu->backend_index);
   StageTotals totals;const auto start=timing_wall_us();const auto cpu_start=thread_cpu_us();std::cout<<"JOB size="<<label<<" mode="<<mode<<" repeat="<<rep<<std::endl;
   bool poster_ready=false;
   if(full){
    {StageTimer hash("input_hash","source.bin",[&](const StageSample& sample){totals.add(sample);std::cout<<stage_record(sample)<<std::endl;},timing_wall_us,thread_cpu_us);sha256_file(work/"source.bin");}
    const auto info=run(exe,rendition_inspection_arguments(work,fused),work,fused?"inspection_poster":"input_inspection","source.bin",&totals);
    if(fused){plan=rendition_plan(true,false,work,std::vector<adaptive::Profile>{h,e},info);
     const auto header=info.substr(0,info.find("Output #"));poster_ready=inspection_poster_reusable(header);
     if(!poster_ready)std::filesystem::remove(work/"poster.webp");}
   }
   if(mode=="shared"){
    auto args=plan[0].arguments;const auto& second=plan[1].arguments;args.insert(args.end(),std::find(second.begin(),second.end(),L"-map"),second.end());
    run(exe,args,work,"transcode","both",&totals);
   }
   for(const auto& item:plan){
    if((mode!="shared"||!item.hardware)&&(!poster_ready||item.hardware))run(exe,item.arguments,work,item.hardware?"transcode":"poster",item.filename,&totals);
    if(item.hardware){auto log=run(exe,validate(work/item.filename),work,"validation",item.filename,&totals);
     if(!adaptive::video_geometry_valid(log,item.filename=="feed.mp4"?"h264":"hevc",width,height))throw std::runtime_error("Software validation geometry failed");}
   }
   if(full)for(const auto& item:plan){StageTimer hash("output_hash",item.filename,[&](const StageSample& sample){totals.add(sample);std::cout<<stage_record(sample)<<std::endl;},timing_wall_us,thread_cpu_us);sha256_file(work/item.filename);}
   const auto cpu_end=thread_cpu_us();std::cout<<totals.record(timing_wall_us()-start,cpu_start&&cpu_end?std::optional<std::uint64_t>{*cpu_end-*cpu_start}:std::nullopt)<<std::endl;
   if(full)for(const auto& item:plan){
    if(sha256_file(work/item.filename)!=sha256_file(base/"baseline-0"/item.filename))throw std::runtime_error("Complete candidate changed original artifact bytes");
    std::cout<<"FULL_PARITY size="<<label<<" mode="<<mode<<" repeat="<<rep<<" artifact="<<item.filename<<" bytes_equal_original=true"<<std::endl;
   }
  }
 }
 std::cout<<"EXPERIMENT_PASS mode="<<mode<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
