#include "test.hpp"
#include "ow/adaptive_hardware.hpp"
#include "ow/media_renditions.hpp"
#include <algorithm>
#include <fstream>
using namespace ow::adaptive;
namespace {
constexpr const char* hash = "approved-ffmpeg-hash";
Device cpu(){return {"cpu",Backend::cpu,"",0,true,0,{},0,true};}
Device gpu(const char* id,Backend family,std::uint64_t vram=4096,const char* driver="driver-1"){
 return {id,family,driver,vram,true,0,{},0,true};
}
ProbeEvidence probe(const Device& d,Codec c,bool pass=true){
 return {d.stable_id,d.driver_version,hash,d.family,c,pass,pass,pass};
}
TuningEvidence tune(const Device& d,Codec c,double fps,std::uint64_t vram=0,bool completed=true){
 return {d.stable_id,d.family,c,fps,2,1024,vram,256,completed,completed,completed,1,
         d.family==Backend::cpu?0u:640u,d.family==Backend::cpu?0u:360u};
}
Limits limits(){return {8,4096,8192,4096,4};}
std::vector<Profile> run(const std::vector<Device>& devices,const std::vector<ProbeEvidence>& probes,
                         const std::vector<TuningEvidence>& tuning,Limits caps=limits()){
 return choose_profiles(devices,probes,tuning,caps,hash);
}
const Profile* find(const std::vector<Profile>& profiles,const std::string& id,Codec codec){
 auto i=std::find_if(profiles.begin(),profiles.end(),[&](const auto& p){return p.stable_id==id&&p.codec==codec;});
 return i==profiles.end()?nullptr:&*i;
}
}
TEST("CPU remains the safe profile when no supported GPU is installed"){
 auto c=cpu();auto profiles=run({c},{probe(c,Codec::h264),probe(c,Codec::hevc)},
  {tune(c,Codec::h264,20),tune(c,Codec::hevc,12)});
 CHECK(profiles.size()==2);CHECK(find(profiles,"cpu",Codec::h264));
 CHECK(find(profiles,"cpu",Codec::hevc));
}
TEST("GPU adapter and compiled encoder listing do not qualify a broken driver"){
 auto c=cpu(),g=gpu("pci:amd:694e",Backend::amd_amf);g.driver_healthy=false;
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,100)});
 CHECK(profiles.size()==1);CHECK(!find(profiles,g.stable_id,Codec::h264));
}
TEST("single GPU profile requires matching encode decode and validated output"){
 auto c=cpu(),g=gpu("pci:nvidia:2704",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,42,1024)});
 auto selected=find(profiles,g.stable_id,Codec::h264);
 CHECK(selected&&selected->backend==Backend::nvidia_nvenc);
 CHECK(selected->vram_mb==1024&&selected->max_concurrency>0);
 CHECK(selected->max_width==640&&selected->max_height==360);
 CHECK(profile_supports_dimensions(*selected,640,360));CHECK(!profile_supports_dimensions(*selected,1920,1080));
}
TEST("heterogeneous GPUs retain independent identities and schedules"){
 auto c=cpu(),a=gpu("pci:nvidia:2704",Backend::nvidia_nvenc,6144),b=gpu("pci:amd:694e",Backend::amd_amf,4096);
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,48,1536),tune(b,Codec::h264,34,1024)});
 CHECK(find(profiles,a.stable_id,Codec::h264));CHECK(find(profiles,b.stable_id,Codec::h264));
 CHECK(find(profiles,a.stable_id,Codec::h264)->stable_id!=find(profiles,b.stable_id,Codec::h264)->stable_id);
}
TEST("two synthetic NVIDIA GPUs schedule independently through the production scheduler"){
 auto c=cpu(),fast=gpu("fixture:rtx-3070",Backend::nvidia_nvenc,4096),small=gpu("fixture:rtx-3050",Backend::nvidia_nvenc,2048);
 fast.backend_index=0;small.backend_index=1;
 auto profiles=run({c,fast,small},{probe(c,Codec::h264),probe(fast,Codec::h264),probe(small,Codec::h264)},
  {tune(c,Codec::h264,20),tune(fast,Codec::h264,70,512),tune(small,Codec::h264,42,512)});
 for(auto& profile:profiles){profile.cpu_threads=1;profile.ram_mb=512;profile.scratch_mb=128;
  if(profile.backend!=Backend::cpu)profile.max_concurrency=1;else profile.max_concurrency=2;}
 Scheduler scheduler(Limits{4,2048,1024,512,4},profiles);
 auto first=scheduler.reserve({Codec::h264});auto second=scheduler.reserve({Codec::h264});
 CHECK(first&&second);CHECK(first.profiles().front().stable_id==fast.stable_id);
 CHECK(second.profiles().front().stable_id==small.stable_id);CHECK(scheduler.active_jobs()==2);
 first.release();second.release();CHECK(scheduler.active_jobs()==0);
}
TEST("codec-specific evidence keeps unsupported HEVC on a verified CPU profile"){
 auto c=cpu(),g=gpu("pci:intel:591b",Backend::intel_qsv);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,28,768)});
 CHECK(find(profiles,g.stable_id,Codec::h264));CHECK(!find(profiles,g.stable_id,Codec::hevc));
 CHECK(find(profiles,"cpu",Codec::hevc));
}
TEST("low VRAM and machine resource caps reject unsafe tuning"){
 auto c=cpu(),g=gpu("pci:amd:low-vram",Backend::amd_amf,512);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,55,1024)});
 CHECK(!find(profiles,g.stable_id,Codec::h264));
 auto roomy=gpu("pci:amd:ok",Backend::amd_amf,4096);
 auto limited=run({c,roomy},{probe(c,Codec::h264),probe(roomy,Codec::h264)},
  {tune(c,Codec::h264,20),tune(roomy,Codec::h264,55,2048)},Limits{2,1024,1024,512,2});
 CHECK(!find(limited,roomy.stable_id,Codec::h264));
}
TEST("measured concurrency stays under CPU RAM scratch VRAM and job ceilings"){
 auto c=cpu(),g=gpu("pci:nvidia:2704",Backend::nvidia_nvenc,4096);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,48,1024)},Limits{4,3072,4096,2300,3});
 auto p=find(profiles,g.stable_id,Codec::h264);CHECK(p);
 CHECK(p->max_concurrency<=3);CHECK(p->cpu_threads*p->max_concurrency<=4);
 CHECK(p->max_concurrency==1);
 CHECK(p->ram_mb*p->max_concurrency<=3072);CHECK(p->vram_mb*p->max_concurrency<=2300);
 CHECK(p->scratch_mb*p->max_concurrency<=4096);
}
TEST("profile invalidates on adapter identity driver or approved FFmpeg change"){
 auto c=cpu(),g=gpu("pci:intel:591b",Backend::intel_qsv);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,30,512)});auto p=find(profiles,g.stable_id,Codec::h264);CHECK(p);
 CHECK(profile_current(*p,g,hash));auto changed=g;changed.driver_version="driver-2";
 CHECK(!profile_current(*p,changed,hash));changed=g;changed.stable_id="pci:intel:other";
 CHECK(!profile_current(*p,changed,hash));changed=g;changed.adapter_index=1;
 CHECK(!profile_current(*p,changed,hash));changed=g;changed.backend_index=1;
 CHECK(!profile_current(*p,changed,hash));CHECK(!profile_current(*p,g,"new-ffmpeg-hash"));
}
TEST("cancelled or failed tuning falls back to CPU without leaking a device reservation"){
 auto c=cpu(),g=gpu("pci:amd:694e",Backend::amd_amf);
 auto failed=tune(g,Codec::h264,60,1024,false);failed.cancelled_cleanly=false;
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),failed});
 CHECK(profiles.size()==1);CHECK(find(profiles,"cpu",Codec::h264));
}
TEST("selected hardware rendition binds an explicit adapter and conservative codec mode"){
 auto c=cpu(),g=gpu("pci:nvidia:2704",Backend::nvidia_nvenc);g.adapter_index=1;g.backend_index=1;
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,42,1024)});
 auto p=find(profiles,g.stable_id,Codec::h264);CHECK(p);
 auto plan=ow::rendition_plan(true,false,"workspace",*p);
 CHECK(plan[0].arguments[0]==L"-nostdin");
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-gpu")!=plan[0].arguments.end());
 auto gpu_arg=std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-gpu");
 CHECK(gpu_arg!=plan[0].arguments.end()&&*(gpu_arg+1)==L"1");
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-crf")==plan[0].arguments.end());
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-cq")!=plan[0].arguments.end());
}
TEST("Intel QSV and AMD AMF commands bind their probed DirectX adapter"){
 for(const auto backend:{Backend::intel_qsv,Backend::amd_amf}){
  auto c=cpu(),g=gpu(backend==Backend::intel_qsv?"pci:intel:591b":"pci:amd:694e",backend);
  g.adapter_index=1;
  auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264)},
   {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,42,1024)});
  auto p=find(profiles,g.stable_id,Codec::h264);CHECK(p);
  auto plan=ow::rendition_plan(true,false,"workspace",*p);const auto& args=plan[0].arguments;
  CHECK(std::find(args.begin(),args.end(),L"-init_hw_device")!=args.end());
  const auto device=backend==Backend::intel_qsv?L"qsv=ow_qsv:hw_any,child_device=1,child_device_type=d3d11va":L"d3d11va=ow_d3d:1";
  CHECK(std::find(args.begin(),args.end(),device)!=args.end());
  if(backend==Backend::amd_amf){CHECK(std::find(args.begin(),args.end(),L"amf=ow_amf@ow_d3d")!=args.end());CHECK(std::find(args.begin(),args.end(),L"vpp_amf")!=args.end());}
  CHECK(std::find(args.begin(),args.end(),backend==Backend::intel_qsv?L"h264_qsv":L"h264_amf")!=args.end());
  CHECK(std::find(args.begin(),args.end(),L"-crf")==args.end());
 }
}
TEST("hardware failure removes partial output and validates a clean CPU retry before success"){
 auto file=std::filesystem::temp_directory_path()/"ow-adaptive-partial-test.bin";
 std::filesystem::remove(file);bool gpu_tried=false,cpu_tried=false;
 const auto used_gpu=ow::adaptive::run_with_cpu_fallback(file,true,[]{return false;},[&](bool hardware){
  if(hardware){gpu_tried=true;std::ofstream(file)<<"partial";throw std::runtime_error("device failed");}
  cpu_tried=true;std::ofstream(file)<<"complete";
 },[](const auto& path){std::ifstream in(path);std::string value;in>>value;return value=="complete";});
 CHECK(!used_gpu&&gpu_tried&&cpu_tried);std::ifstream in(file);std::string result;in>>result;
 CHECK(result=="complete");in.close();std::filesystem::remove(file);
}
TEST("cancelled hardware work does not start a CPU retry or retain partial output"){
 auto file=std::filesystem::temp_directory_path()/"ow-adaptive-cancel-test.bin";
 std::filesystem::remove(file);bool cpu_tried=false;
 THROWS(ow::adaptive::run_with_cpu_fallback(file,true,[]{return true;},[&](bool){std::ofstream(file)<<"partial";throw std::runtime_error("cancelled");},
  [&](const auto&){cpu_tried=true;return true;}));
 CHECK(!cpu_tried);CHECK(!std::filesystem::exists(file));
}
TEST("runtime scheduler atomically enforces global and per-device costs until drain"){
 auto c=cpu(),a=gpu("stable-a",Backend::nvidia_nvenc,4096),b=gpu("stable-b",Backend::amd_amf,4096);
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,50,1200),tune(b,Codec::h264,45,1200)});
 for(auto& p:profiles)if(p.backend==Backend::cpu){p.ram_mb=256;p.cpu_threads=1;p.scratch_mb=128;}
 auto pa=*find(profiles,"stable-a",Codec::h264),pb=*find(profiles,"stable-b",Codec::h264);
 pa.ram_mb=256;pa.cpu_threads=1;pa.scratch_mb=128;pb.ram_mb=256;pb.cpu_threads=1;pb.scratch_mb=128;
 pa.max_concurrency=pb.max_concurrency=2;
 auto pc=*find(profiles,"cpu",Codec::h264);
 Scheduler scheduler(Limits{4,1024,1500,1024,4},{pa,pb,pc});
 auto first=scheduler.reserve({Codec::h264});CHECK(first);CHECK(first.profiles().size()==2);
 auto second=scheduler.reserve({Codec::h264});CHECK(second); // CPU work may proceed; GPU capacity remains reserved
 CHECK(second.profiles().front().backend==Backend::cpu);
 first.release();second.release();auto after_drain=scheduler.reserve({Codec::h264});CHECK(after_drain);
}
TEST("task caps limit each reservation while global capacity admits four small jobs"){
 Profile profile{"cpu","","approved-ffmpeg-hash",Backend::cpu,Codec::h264,
                  1,512,0,128,4,20,0,0,true};
 Scheduler scheduler(Limits{4,2048,0,1024,4},{profile});
 const Limits task{1,512,0,256,1};
 std::vector<Scheduler::Reservation> reservations;
 for(int i=0;i<4;++i){
  auto reservation=scheduler.reserve({Codec::h264},task);
  CHECK(reservation);reservations.push_back(std::move(reservation));
 }
 CHECK(scheduler.active_jobs()==4);
 CHECK(!scheduler.reserve({Codec::h264},task));
 reservations.clear();CHECK(scheduler.active_jobs()==0);
}
TEST("native drain fixture keeps its GPU reservation while admitting independent CPU work"){
 Profile gpu{"synthetic-luid","driver","hash",Backend::nvidia_nvenc,Codec::h264,1,256,512,128,1,50,0,0,true};
 Profile cpu{"cpu","","hash",Backend::cpu,Codec::h264,1,256,0,128,1,20,0,0,true};
 Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
 auto held=scheduler.reserve({Codec::h264});CHECK(held);
 CHECK(held.profiles().front().backend==Backend::nvidia_nvenc);
 auto independent=scheduler.reserve({Codec::h264});CHECK(independent);
 CHECK(independent.profiles().size()==1);
 CHECK(independent.profiles().front().backend==Backend::cpu);
 CHECK(scheduler.active_jobs()==2);CHECK(!scheduler.reserve({Codec::h264}));
 independent.release();CHECK(scheduler.active_jobs()==1);
 // Releasing unrelated CPU work must not return the GPU's reserved capacity.
 auto replacement=scheduler.reserve({Codec::h264});CHECK(replacement);
 CHECK(replacement.profiles().front().backend==Backend::cpu);
 replacement.release();held.release();CHECK(scheduler.active_jobs()==0);
 auto after_drain=scheduler.reserve({Codec::h264});CHECK(after_drain);
 CHECK(after_drain.profiles().front().backend==Backend::nvidia_nvenc);
}
TEST("temporary CPU admission and GPU reservation moves preserve separate backend capacity"){
 Profile gpu{"synthetic-luid","driver","hash",Backend::nvidia_nvenc,Codec::h264,1,256,512,128,1,50,0,0,true};
 Profile cpu{"cpu","","hash",Backend::cpu,Codec::h264,1,256,0,128,1,20,0,0,true};
 Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
 auto held=scheduler.reserve({Codec::h264});CHECK(held);
 CHECK(held.profiles().front().backend==Backend::nvidia_nvenc);
 // CHECK evaluates once; this temporary releases only its own CPU reservation.
 CHECK(scheduler.reserve({Codec::h264}));CHECK(scheduler.active_jobs()==1);
 Scheduler::Reservation draining(std::move(held));CHECK(!held);CHECK(draining);
 auto independent=scheduler.reserve({Codec::h264});CHECK(independent);
 CHECK(independent.profiles().size()==1);
 CHECK(independent.profiles().front().backend==Backend::cpu);
 CHECK(scheduler.active_jobs()==2);CHECK(!scheduler.reserve({Codec::h264}));
 draining.release();CHECK(scheduler.active_jobs()==1);
 auto after_drain=scheduler.reserve({Codec::h264});CHECK(after_drain);
 CHECK(after_drain.profiles().front().backend==Backend::nvidia_nvenc);
 CHECK(scheduler.active_jobs()==2);CHECK(!scheduler.reserve({Codec::h264}));
}
TEST("exhausted backend capacity stays owned across reservation moves until release"){
 for(auto backend:{Backend::cpu,Backend::nvidia_nvenc,Backend::intel_qsv,Backend::amd_amf}){
  Profile cpu{"cpu","","hash",Backend::cpu,Codec::h264,1,256,0,128,1,20,0,0,true};
  Profile gpu{"synthetic-luid","driver","hash",backend,Codec::h264,1,256,512,128,1,50,0,0,true};
  std::vector<Profile> profiles{cpu};if(backend!=Backend::cpu)profiles.push_back(gpu);
  // One CPU thread is exhausted by either primary backend's job budget.
  Scheduler scheduler({1,1024,512,512,4},profiles);
  auto held=scheduler.reserve({Codec::h264});CHECK(held);
  CHECK(held.profiles().front().backend==backend);
  CHECK(!scheduler.reserve({Codec::h264}));
  Scheduler::Reservation draining(std::move(held));
  CHECK(!held);CHECK(draining);CHECK(scheduler.active_jobs()==1);
  CHECK(!scheduler.reserve({Codec::h264}));
  draining.release();draining.release();CHECK(scheduler.active_jobs()==0);
  auto after_drain=scheduler.reserve({Codec::h264});CHECK(after_drain);
  CHECK(after_drain.profiles().front().backend==backend);
 }
}
TEST("CUDA LUID mapping survives ordinal reorder and rejects duplicate or invalid mappings"){
 std::vector<BackendIdentity> mappings{{"luid-b",1,1},{"luid-a",0,1}};
 CHECK(resolve_unique_backend_ordinal(mappings,"luid-a")==0);
 CHECK(resolve_unique_backend_ordinal(mappings,"luid-b")==1);
 mappings.push_back({"luid-a",2,1});CHECK(!resolve_unique_backend_ordinal(mappings,"luid-a"));
 mappings={{"luid-a",0,1},{"luid-b",0,1}};CHECK(!resolve_unique_backend_ordinal(mappings,"luid-a"));
 mappings={{"luid-a",0,0}};CHECK(!resolve_unique_backend_ordinal(mappings,"luid-a"));
}
TEST("GPU memory telemetry is child and LUID scoped and rejects ambiguous physical nodes"){
 std::vector<GpuMemoryRecord> records{{100,0,200,"gpu-a"},{100,1,300,"gpu-a"},
  {200,0,1000,"gpu-a"},{100,0,500,"gpu-b"}};
 auto child=gpu_memory_totals(records,100,"gpu-a");CHECK(child);
 CHECK(child->child_bytes==500&&child->adapter_bytes==1500);
 auto idle=gpu_memory_totals(records,0,"gpu-a");CHECK(idle&&idle->adapter_bytes==1500); // only matching valid nonzero LUID counters qualify
 records.push_back({100,0,1,"gpu-a"});CHECK(!gpu_memory_totals(records,100,"gpu-a"));
 CHECK(!gpu_memory_totals(records,100,"missing-gpu"));
}
TEST("missing foreign invalid and zero GPU counters never mean idle headroom"){
 CHECK(!gpu_memory_totals({},0,"gpu-a"));
 CHECK(!gpu_memory_totals({{10,0,100,"gpu-b"}},0,"gpu-a"));
 CHECK(!gpu_memory_totals({{10,0,0,"gpu-a"}},0,"gpu-a"));
 CHECK(!gpu_memory_totals({{10,0,1,"gpu-a",false}},10,"gpu-a"));
 CHECK(!gpu_memory_totals({{10,0,0,"gpu-a"}},10,"gpu-a"));
 CHECK(gpu_memory_totals({{10,0,100,"gpu-a"}},10,"gpu-a"));
}
TEST("serial and parallel tuning compare equal work repeated stages and child budgets"){
 auto same=compare_tuning_runs({1000,1000},{1000,1000},2,2,144,144,1,1,512,512,128,128);
 CHECK(same.comparable&&same.serial_fps==72&&same.parallel_fps==72);
 CHECK(!compare_tuning_runs({1000,1000},{1000,1000},1,2,72,144,1,1,512,512,128,128).comparable);
 CHECK(!compare_tuning_runs({1000},{1000},1,1,72,72,2,1,512,512,128,128).comparable);
}
TEST("two independent GPU jobs fit with one CPU encoder slot and can wait for safe CPU retry"){
 auto c=cpu(),a=gpu("fixture:rtx-3070",Backend::nvidia_nvenc),b=gpu("fixture:rtx-3050",Backend::nvidia_nvenc);
 a.backend_index=0;b.backend_index=1;
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,70,512),tune(b,Codec::h264,42,512)});
 for(auto& p:profiles){p.cpu_threads=1;p.ram_mb=512;p.scratch_mb=128;p.max_concurrency=p.backend==Backend::cpu?1:1;}
 Scheduler scheduler(Limits{2,2048,1024,512,2},profiles);
 auto first=scheduler.reserve({Codec::h264}),second=scheduler.reserve({Codec::h264});
 CHECK(first&&second);CHECK(first.profiles().front().backend==Backend::nvidia_nvenc);
 CHECK(second.profiles().front().backend==Backend::nvidia_nvenc);
 CHECK(first.profiles().front().stable_id!=second.profiles().front().stable_id);
 CHECK(first.acquire_cpu_fallback([]{return false;},10));
 CHECK(!second.acquire_cpu_fallback([]{return false;},10));
 first.release();CHECK(second.acquire_cpu_fallback([]{return false;},10));
 second.release();CHECK(scheduler.active_jobs()==0);
}
TEST("GPU fallback slot acquisition ignores a CPU codec already owned by the same task"){
 auto c=cpu(),g=gpu("fixture:gpu-hevc",Backend::amd_amf);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::hevc)},
  {tune(c,Codec::h264,30),tune(c,Codec::hevc,10),tune(g,Codec::hevc,40,512)});
 for(auto& p:profiles){p.cpu_threads=1;p.ram_mb=512;p.scratch_mb=128;p.max_concurrency=1;}
 Scheduler scheduler(Limits{2,1024,1024,256,1},profiles);
 auto reservation=scheduler.reserve({Codec::h264,Codec::hevc});CHECK(reservation);
 CHECK(reservation.acquire_cpu_fallback([]{return false;},10));
 reservation.release();CHECK(scheduler.active_jobs()==0);
}
TEST("runtime profile ties resolve by stable device identity and CPU profile is selectable"){
 auto c=cpu(),a=gpu("z-device",Backend::nvidia_nvenc),b=gpu("a-device",Backend::amd_amf);
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,50,512),tune(b,Codec::h264,50,512)});
 Scheduler scheduler(Limits{4,2048,1024,2048,2},profiles);
 auto reservation=scheduler.reserve({Codec::h264});CHECK(reservation);
 CHECK(reservation.profiles().front().stable_id=="a-device");
}
TEST("hardware adapter mapping ambiguity fails closed independent of enumeration order"){
 auto c=cpu(),a=gpu("luid-a",Backend::nvidia_nvenc),b=gpu("luid-b",Backend::nvidia_nvenc);
 a.backend_index=0;b.backend_index=0;a.backend_identity_verified=false;b.backend_identity_verified=false;
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,50,512),tune(b,Codec::h264,48,512)});
 CHECK(!find(profiles,"luid-a",Codec::h264));CHECK(!find(profiles,"luid-b",Codec::h264));
}
TEST("runtime admission falls back to the measured CPU profile when configured VRAM is too small"){
 auto c=cpu(),g=gpu("gpu",Backend::amd_amf);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,50,1024)});
 Scheduler scheduler(Limits{8,4096,512,4096,2},profiles);
 auto reservation=scheduler.reserve({Codec::h264});CHECK(reservation);
 CHECK(reservation.profiles().size()==1);CHECK(reservation.profiles().front().backend==Backend::cpu);
}
TEST("live adapter headroom closes GPU admission and restores it after headroom returns"){
 auto c=cpu(),g=gpu("live-gpu",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,50,512)});
 Scheduler scheduler(Limits{8,4096,2048,4096,2},profiles);
 scheduler.set_device_available_vram(g.stable_id,0);auto safe=scheduler.reserve({Codec::h264});
 CHECK(safe&&safe.profiles().front().backend==Backend::cpu);safe.release();
 scheduler.set_device_available_vram(g.stable_id,512);auto gpu_job=scheduler.reserve({Codec::h264});
 CHECK(gpu_job&&gpu_job.profiles().front().backend==Backend::nvidia_nvenc);
}
TEST("CPU concurrency reflects repeated tuning candidates within every configured cap"){
 auto c=cpu();auto p=probe(c,Codec::h264);auto t=tune(c,Codec::h264,20);t.cpu_threads=1;t.ram_mb=512;t.scratch_mb=128;t.tested_concurrency=2;
 auto profiles=run({c},{p},{t},Limits{4,4096,0,1024,4});
 CHECK(find(profiles,"cpu",Codec::h264));CHECK(find(profiles,"cpu",Codec::h264)->max_concurrency==2);
 auto tight=run({c},{p},{t},Limits{1,4096,0,1024,4});
 CHECK(find(tight,"cpu",Codec::h264)->max_concurrency==1);
}
TEST("multi-codec runtime reservation uses peak per-job CPU memory and scratch costs"){
 auto h=tune(cpu(),Codec::h264,20),v=tune(cpu(),Codec::hevc,12);h.cpu_threads=v.cpu_threads=1;h.ram_mb=v.ram_mb=512;h.scratch_mb=v.scratch_mb=128;
 auto profiles=run({cpu()},{probe(cpu(),Codec::h264),probe(cpu(),Codec::hevc)},{h,v});
 Scheduler scheduler(Limits{1,512,0,128,2},profiles);
 auto first=scheduler.reserve({Codec::h264,Codec::hevc});CHECK(first);CHECK(first.profiles().size()==2);
 CHECK(!scheduler.reserve({Codec::h264,Codec::hevc}));first.release();CHECK(scheduler.reserve({Codec::h264,Codec::hevc}));
}
TEST("runtime scheduler invalidation stops stale hardware while active reservations drain"){
 auto c=cpu(),g=gpu("device-before-change",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,50,512)});
 for(auto& p:profiles)if(p.backend==Backend::cpu)p.max_concurrency=2;
 Scheduler scheduler(Limits{8,4096,2048,4096,4},profiles);auto running=scheduler.reserve({Codec::h264});CHECK(running);
 scheduler.invalidate_hardware();auto replacement=scheduler.reserve({Codec::h264});CHECK(replacement);
 for(const auto& p:replacement.profiles())CHECK(p.backend==Backend::cpu);
 CHECK(scheduler.active_jobs()==2);running.release();replacement.release();
 scheduler.invalidate_all();CHECK(!scheduler.reserve({Codec::h264}));
}
TEST("runtime command generation applies the scheduler-selected measured CPU profile"){
 auto c=cpu();auto p=run({c},{probe(c,Codec::h264)},{tune(c,Codec::h264,20)});
 CHECK(p.size()==1);p[0].cpu_threads=3;
 auto plan=ow::rendition_plan(true,false,"workspace",p);
 auto threads=std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-threads");
 CHECK(threads!=plan[0].arguments.end()&&threads+1!=plan[0].arguments.end());CHECK(*(threads+1)==L"3");
}
TEST("cancellation before an encode prevents every helper spawn"){
 auto file=std::filesystem::temp_directory_path()/"ow-adaptive-before-spawn.bin";std::filesystem::remove(file);
 bool spawned=false;THROWS(ow::adaptive::run_with_cpu_fallback(file,true,[]{return true;},[&](bool){spawned=true;},[](const auto&){return true;}));
 CHECK(!spawned);CHECK(!std::filesystem::exists(file));
}
TEST("cancellation during output validation cannot publish a successful artifact"){
 auto file=std::filesystem::temp_directory_path()/"ow-adaptive-during-validation.bin";std::filesystem::remove(file);
 bool cancelled=false;THROWS(ow::adaptive::run_with_cpu_fallback(file,true,[&]{return cancelled;},[&](bool){std::ofstream(file)<<"complete";},[&](const auto&){cancelled=true;return true;}));
 CHECK(cancelled);CHECK(!std::filesystem::exists(file));
}

TEST("declared VRAM fallback permits proven hardware and retains CPU and drain budgets"){
 auto c=cpu(),g=gpu("radeon",Backend::amd_amf,4043);
 auto budget=vram_budget(g.dedicated_vram_mb,{},8192);
 CHECK(budget.estimated&&budget.mb==1024);
 auto profiles=run({c,g},{probe(c,Codec::hevc),probe(g,Codec::hevc)},
  {tune(c,Codec::hevc,20),tune(g,Codec::hevc,50,budget.mb)});
 CHECK(find(profiles,g.stable_id,Codec::hevc));CHECK(find(profiles,"cpu",Codec::hevc));
 Scheduler scheduler(limits(),profiles);scheduler.set_device_available_vram(g.stable_id,budget.mb);
 auto running=scheduler.reserve({Codec::hevc});CHECK(running);
 CHECK(running.profiles().front().backend==Backend::amd_amf);
 auto second=scheduler.reserve({Codec::hevc});CHECK(second);
 CHECK(second.profiles().front().backend==Backend::cpu);
 CHECK(vram_envelope_ok(4043,budget.mb,{}));
 CHECK(!vram_envelope_ok(4043,budget.mb,GpuMemoryTotals{1025ull*1024*1024,1025ull*1024*1024}));
 CHECK(!vram_envelope_ok(4043,budget.mb,GpuMemoryTotals{1,4044ull*1024*1024}));
 CHECK(!vram_envelope_ok(0,budget.mb,{}));
 CHECK(!vram_envelope_ok(4043,1025,{}));
}
TEST("VRAM budgets preserve live headroom and fail closed without either source"){
 CHECK(vram_budget(4043,1000ull*1024*1024,8192).mb==3043);
 CHECK(!vram_budget(4043,1000ull*1024*1024,8192).estimated);
 CHECK(vram_budget(4043,4044ull*1024*1024,8192).mb==0);
 CHECK(vram_budget(0,{},8192).mb==0);
 CHECK(vram_budget(4043,{},0).mb==0);
 CHECK(vram_budget(128,{},8192).mb==64);
 for(auto declared:std::vector<std::uint64_t>{1,2,127,128,4043,UINT64_MAX}){
  auto b=vram_budget(declared,{},UINT64_MAX);CHECK(b.mb<=declared/2);CHECK(b.mb<=1024);
 }
 CHECK(vram_budget(4043,{},256).mb==256);
 CHECK(vram_budget(4043,1,8192).mb==4042); // fractional usage rounds up
}

TEST("declared budget never substitutes for encode decode identity or tuning proof"){
 auto c=cpu(),g=gpu("radeon",Backend::amd_amf,4043);
 auto t=tune(g,Codec::h264,50,vram_budget(4043,{},8192).mb);
 for(unsigned failure=0;failure<5;++failure){
  auto d=g;auto proof=probe(g,Codec::h264);auto tuning=t;
  if(failure==0)proof.encoded=false;
  if(failure==1)proof.decoded=false;
  if(failure==2)proof.output_valid=false;
  if(failure==3)tuning.completed=false;
  if(failure==4)d.backend_identity_verified=false;
  auto profiles=run({c,d},{probe(c,Codec::h264),proof},{tune(c,Codec::h264,20),tuning});
  CHECK(!find(profiles,g.stable_id,Codec::h264));CHECK(find(profiles,"cpu",Codec::h264));
 }
 CHECK(!vram_envelope_ok(4043,1024,GpuMemoryTotals{1024ull*1024*1024+1,1024ull*1024*1024+1}));
}
