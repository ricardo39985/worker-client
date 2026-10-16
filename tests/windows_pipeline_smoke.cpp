#include "process.hpp"
#include "ow/media_renditions.hpp"
#include "ow/rendition_pipeline.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
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
  std::cout<<"PIPELINE_SMOKE_PASS (real native FFmpeg; simulated transfer latency only)\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
