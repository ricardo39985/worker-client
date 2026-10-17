#include "test.hpp"
#include "ow/encoder_run.hpp"
#include "ow/media_renditions.hpp"
#include <fstream>
#include <future>
namespace {
struct Workspace {
 std::filesystem::path path=std::filesystem::temp_directory_path()/
  ("ow-encoder-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Workspace(){std::filesystem::create_directories(path);}
 ~Workspace(){std::error_code e;std::filesystem::remove_all(path,e);}
};
}
TEST("all hardware command plans retain device selection and measured timeline progress") {
 Workspace work;
 for(auto backend:{ow::adaptive::Backend::nvidia_nvenc,ow::adaptive::Backend::intel_qsv,ow::adaptive::Backend::amd_amf}){
  ow::adaptive::Profile profile;profile.backend=backend;profile.codec=ow::adaptive::Codec::hevc;
  profile.adapter_index=2;profile.backend_index=3;
  auto plan=ow::rendition_plan(true,false,work.path,profile);CHECK(plan[1].hardware);
  auto args=ow::encoder_progress_arguments(plan[1].arguments);
  CHECK(std::find(args.begin(),args.end(),L"pipe:1")!=args.end());
  CHECK(std::find(args.begin(),args.end(),L"libx265")==args.end());
  CHECK(args.back()==(work.path/"optimized.mp4").wstring());
  for(const auto& arg:plan[1].arguments)CHECK(std::find(args.begin(),args.end(),arg)!=args.end()||arg==L"error");
  ow::ProgressCounter progress;progress.begin("convert optimized.mp4",0);
  ow::EncoderProgressFile reader(work.path/"process.log");
  {std::ofstream log(work.path/"process.log");log<<"Duration: 00:02:00.00\ntotal_size=90000000\nout_time_us=3000";}
  reader.update(progress,100);CHECK(progress.snapshot(100).percent==0);
  {std::ofstream log(work.path/"process.log",std::ios::app);log<<"0000\n";}
  reader.update(progress,30000);CHECK(progress.snapshot(30000).percent==25);
 }
}
TEST("four hardware failures reset timeline and stage generation on CPU retry independently") {
 std::vector<std::future<void>> jobs;
 for(unsigned job=0;job<4;++job)jobs.push_back(std::async(std::launch::async,[]{
  Workspace work;ow::ProgressCounter progress;ow::ProgressLogPolicy policy;unsigned runs=0;
  const auto output=work.path/"feed.mp4";
  bool hardware=ow::adaptive::run_with_cpu_fallback(output,true,[]{return false;},[&](bool gpu){
   progress.begin("convert feed.mp4",++runs*1000);
   ow::EncoderProgressFile reader(work.path/"process.log");
   {std::ofstream log(work.path/"process.log");log<<"Duration: 00:02:00.00\nout_time_us="<<(gpu?118800000:12000000)<<"\ntotal_size=90000000\n";}
   reader.update(progress,runs*1000+100);
   CHECK(progress.snapshot(runs*1000+100).percent==(gpu?99:10));
   CHECK(progress.snapshot(runs*1000+100).run==runs);
   auto line=policy.line("attempt-1234","",progress.snapshot(runs*1000+100));CHECK(line);
   if(!gpu){CHECK(line->find("RETRY")!=std::string::npos);CHECK(!std::filesystem::exists(output));}
   std::ofstream(output)<<(gpu?"partial":"valid");
   if(gpu)throw std::runtime_error("synthetic hardware failure");
  },[](const auto&){return true;});
  CHECK(!hardware);CHECK(runs==2);CHECK(std::filesystem::file_size(output)==5);
 }));
 for(auto& job:jobs)job.get();
}
