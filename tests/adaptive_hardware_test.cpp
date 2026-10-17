#include "test.hpp"
#include "ow/capability_report.hpp"
#include "ow/operator_status.hpp"
#include "ow/adaptive_hardware.hpp"
#include "ow/device_identity.hpp"
#include "ow/media_renditions.hpp"
#include "ow/adaptive_evidence.hpp"
#include <algorithm>
#include <fstream>
#include <future>
using namespace ow::adaptive;
namespace {
constexpr const char* hash = "approved-ffmpeg-hash";
Device cpu(){return {"cpu",Backend::cpu,"",0,true,0,{},0,true};}
Device gpu(const char* id,Backend family,std::uint64_t vram=4096,const char* driver="driver-1"){
 auto d=Device{id,family,driver,vram,true,0,{},0,true};d.physical_id=id;return d;
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
 CHECK(selected->max_width>=1920&&selected->max_height>=1080);
 CHECK(profile_supports_dimensions(*selected,640,360));CHECK(profile_supports_dimensions(*selected,1920,1080));
 CHECK(!profile_supports_dimensions(*selected,16384,8640));
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
 gpu.physical_id="fixture-uuid";Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
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
 gpu.physical_id="fixture-uuid";Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
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
  gpu.physical_id="fixture-uuid";std::vector<Profile> profiles{cpu};if(backend!=Backend::cpu)profiles.push_back(gpu);
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

TEST("encoder tokens name every backend that survived probing") {
 std::vector<Profile> profiles(4);
 profiles[0].backend=Backend::cpu;
 profiles[1].backend=Backend::intel_qsv;
 profiles[2].backend=Backend::amd_amf;
 profiles[3].backend=Backend::cpu;
 CHECK(encoder_tokens(profiles)=="cpu,qsv,amf");
}

TEST("encoder tokens never claim hardware when only CPU was profiled") {
 std::vector<Profile> profiles(1);
 profiles[0].backend=Backend::cpu;
 CHECK(encoder_tokens(profiles)=="cpu");
 CHECK(encoder_tokens({}).empty());
}

TEST("a hardware-only profile set cannot imply a CPU profile") {
 ow::adaptive::Profile p{};p.backend=ow::adaptive::Backend::amd_amf;
 CHECK(ow::adaptive::encoder_tokens({p})=="amf");
}

TEST("runtime hardware failure reports exactly once without changing selected profiles") {
 using namespace ow::adaptive;
 Profile cpu{};cpu.backend=Backend::cpu;cpu.codec=Codec::hevc;
 Profile gpu=cpu;gpu.backend=Backend::amd_amf;gpu.stable_id="amd";
 ow::CapabilityReport report;report.probed(true,true,true,true,{cpu,gpu});
 std::vector<std::string> lines;const auto file=std::filesystem::temp_directory_path()/"ow-report-fallback.mp4";
 for(unsigned retry=0;retry<2;++retry){
  CHECK(!run_with_cpu_fallback(file,true,[]{return false;},[&](bool hardware){
   if(hardware)throw std::runtime_error("synthetic hardware failure");
   if(auto line=report.failed(gpu))lines.push_back(*line);
   std::ofstream(file)<<"cpu";
  },[](const auto&){return true;}));
 }
 std::filesystem::remove(file);
 CHECK(lines==std::vector<std::string>{"ENCODERS UPDATE encoders=cpu failed=amf/hevc fallback=cpu"});
 CHECK(report.encoders()=="cpu");CHECK(report.profiles().size()==2);
 CHECK(!report.changed({cpu})); // already stopped claiming the failed profile
}
TEST("reporting retains another profiled codec and handles profile removal and binary invalidation") {
 using namespace ow::adaptive;
 Profile cpu{};Profile gpu{};gpu.backend=Backend::amd_amf;gpu.stable_id="amd";
 Profile hevc=gpu;hevc.codec=Codec::hevc;
 ow::CapabilityReport report;report.probed(true,false,true,false,{cpu,gpu,hevc});
 CHECK(report.failed(gpu));CHECK(report.encoders()=="cpu,amf");
 CHECK(report.changed({cpu})=="ENCODERS UPDATE encoders=cpu");CHECK(!report.changed({cpu}));
 report.probed(false,false,false,false,{});
 CHECK(report.encoders().empty());CHECK(report.record()=="PROBE RESULT app_video=0 app_image=0 video=0 image=0 encoders=none");
}
TEST("diagnostic CPU evidence is distinct from missing adaptive profiles") {
 ow::CapabilityReport report;report.probed(false,false,false,false,{});CHECK(report.encoders().empty());
 report.probed(true,false,false,false,{});CHECK(report.encoders()=="cpu");
 report.probed(false,true,false,false,{});CHECK(report.encoders()=="cpu");
}

TEST("converter invalidation retracts CPU as well as hardware evidence once") {
 ow::CapabilityReport report;report.probed(true,true,true,true,{});
 CHECK(report.disabled()=="ENCODERS UPDATE encoders=none");CHECK(!report.disabled());
 CHECK(report.record()=="PROBE RESULT app_video=0 app_image=0 video=0 image=0 encoders=none");
}
TEST("backup presentation promises a routing hint only") {
 CHECK(ow::win::priority_label(ow::win::Priority::backup)=="Backup (routing hint)");
 CHECK(ow::win::priority_label(ow::win::Priority::normal)=="Normal");
}

TEST("unobserved inventory is unknown while an observed zero stays zero") {
 CHECK(ow::observed_count(std::nullopt)=="unknown");
 CHECK(ow::observed_count(0)=="0");CHECK(ow::observed_count(4043)=="4043");
}

TEST("status evidence separates unrun checks from failed probes and requires current CPU app profiles") {
 using namespace ow::adaptive;
 ow::CapabilityReport report;CHECK(!report.processing_ready().has_value());
 report.probed(false,false,false,false,{});CHECK(report.processing_ready()==false);
 report.probed(true,true,true,true,{});CHECK(!report.app_video());CHECK(report.app_image());
 Profile h264{};h264.backend=Backend::cpu;h264.codec=Codec::h264;
 Profile hevc=h264;hevc.codec=Codec::hevc;
 report.probed(true,true,true,true,{h264,hevc});CHECK(report.app_video());
 Profile gpu=hevc;gpu.backend=Backend::amd_amf;gpu.stable_id="amd";
 CHECK(report.changed({h264,hevc,gpu}));CHECK(report.failed(gpu));CHECK(report.app_video());
 CHECK(report.changed({h264}));CHECK(!report.app_video());
 CHECK(report.record().starts_with("PROBE RESULT app_video=0 app_image=1"));
}

TEST("sample dimensions describe evidence while encoder geometry governs admission"){
 for(auto backend:{Backend::nvidia_nvenc,Backend::intel_qsv,Backend::amd_amf}){
  auto c=cpu(),g=gpu("fixture:geometry",backend);
  auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
    {tune(c,Codec::h264,20),tune(g,Codec::h264,42,512)});
  auto p=find(profiles,g.stable_id,Codec::h264);CHECK(p);
  CHECK(p->measured_width==640&&p->measured_height==360);
  CHECK(profile_supports_dimensions(*p,1920,1080));
  CHECK(geometry_evidence(*p,1920,1080)==GeometryEvidence::unproven);
  CHECK(geometry_evidence(*p,640,360)==GeometryEvidence::measured);
  CHECK(geometry_evidence(*p,p->max_width+2,1080)==GeometryEvidence::unsupported);
  CHECK(geometry_evidence(*p,1921,1080)==GeometryEvidence::unsupported);
  CHECK(geometry_evidence(*p,0,1080)==GeometryEvidence::unproven);
  CHECK(geometry_reason(GeometryEvidence::unproven)!=geometry_reason(GeometryEvidence::unsupported));
 }
}
TEST("only equal realistic measurements may withhold hardware for CPU throughput"){
 auto c=cpu(),g=gpu("fixture:comparison",Backend::intel_qsv);
 auto ct=tune(c,Codec::h264,100),gt=tune(g,Codec::h264,20,512);
 ct.measured_width=gt.measured_width=640;ct.measured_height=gt.measured_height=360;
 CHECK(!comparable_resolution(gt,ct));
 CHECK(find(run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},{ct,gt}),g.stable_id,Codec::h264));
 ct.measured_width=gt.measured_width=1920;ct.measured_height=gt.measured_height=1080;
 CHECK(comparable_resolution(gt,ct));
 CHECK(!find(run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},{ct,gt}),g.stable_id,Codec::h264));
 gt.measured_width=3840;gt.measured_height=2160;CHECK(!comparable_resolution(gt,ct));
}
TEST("large hardware encode or validation failure demotes once and retries CPU while capacity stays owned"){
 for(bool encode_failure:{true,false}){
 auto c=cpu(),g=gpu("fixture:large",Backend::amd_amf);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
   {tune(c,Codec::h264,20),tune(g,Codec::h264,42,512)});
 auto p=*find(profiles,g.stable_id,Codec::h264);
 Scheduler scheduler(limits(),profiles);auto reserved=scheduler.reserve({Codec::h264});CHECK(reserved);
 CHECK(reserved.hardware_enabled(p));CHECK(geometry_evidence(p,3840,2160)==GeometryEvidence::unproven);
 auto output=std::filesystem::temp_directory_path()/"ow-large-fallback-test.mp4";
 unsigned demotions=0,hardware_runs=0,cpu_runs=0;
 bool used_hardware=run_with_cpu_fallback(output,true,[]{return false;},[&](bool hardware){
  CHECK(scheduler.active_jobs()==1);
  if(hardware){++hardware_runs;std::ofstream(output)<<"partial";if(encode_failure)throw std::runtime_error("large encode failed");return;}
  if(scheduler.demote_profile(p))++demotions;
  CHECK(!scheduler.demote_profile(p));CHECK(!reserved.hardware_enabled(p));
  CHECK(reserved.acquire_cpu_fallback([]{return false;},0));CHECK(!std::filesystem::exists(output));
  ++cpu_runs;std::ofstream(output)<<"validated";
 },[](const auto& path){std::ifstream file(path);std::string result;file>>result;return result=="validated";});
 CHECK(!used_hardware&&hardware_runs==1&&cpu_runs==1&&demotions==1);
 reserved.release();CHECK(scheduler.active_jobs()==0);
 auto next=scheduler.reserve({Codec::h264});CHECK(next);
 CHECK(next.profiles().front().backend==Backend::cpu);
 std::filesystem::remove(output);
 }
}

TEST("saved profiles and rejected measurements name their actual tuning resolution"){
 auto c=cpu(),g=gpu("fixture:record",Backend::intel_qsv);
 auto ct=tune(c,Codec::h264,20),gt=tune(g,Codec::h264,42,512);
 ct.measured_width=gt.measured_width=tuning_width;ct.measured_height=gt.measured_height=tuning_height;
 CHECK(tuning_width>=1920&&tuning_height>=1080);
 CHECK(tuning_source_filter()==L"testsrc2=size=1920x1080:rate=24");
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},{ct,gt});
 auto record=profile_record(*find(profiles,g.stable_id,Codec::h264));
 CHECK(record.at("measured_width").as_uint64()==1920);CHECK(record.at("measured_height").as_uint64()==1080);
 CHECK(record.at("max_width").as_uint64()>1920);CHECK(record.at("geometry_source").as_string()=="conservative_backend_ceiling");
 auto measurement=tuning_record(gt);CHECK(measurement.at("measured_width").as_uint64()==1920);
 CHECK(measurement.at("measured_height").as_uint64()==1080);
 const std::string valid="Input #0, mov,mp4\nVideo: h264 (High), yuv420p, 1920x1080, 24 fps\nAudio: aac\nframe=   72 ";
 CHECK(tuning_output_valid(valid,"h264"));CHECK(!tuning_output_valid(valid,"hevc"));
 auto bad=valid;bad.replace(bad.find("1920x1080"),9,"640x360");CHECK(!tuning_output_valid(bad,"h264"));
 bad=valid;bad.replace(bad.find("72"),2,"71");CHECK(!tuning_output_valid(bad,"h264"));
 CHECK(video_geometry_valid(valid,"h264",1920,1080));
 CHECK(!video_geometry_valid(valid,"h264",3840,2160));
}

TEST("demotion is atomic codec specific and preserves already reserved capacity"){
 auto c=cpu(),g=gpu("fixture:demotion",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264),probe(g,Codec::hevc)},
   {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,42,512),tune(g,Codec::hevc,40,512)});
 auto p=*find(profiles,g.stable_id,Codec::h264);Scheduler scheduler(limits(),profiles);
 auto active=scheduler.reserve({Codec::h264});CHECK(active);
 auto first=std::async(std::launch::async,[&]{return scheduler.demote_profile(p);});
 auto second=std::async(std::launch::async,[&]{return scheduler.demote_profile(p);});
 CHECK(static_cast<unsigned>(first.get())+static_cast<unsigned>(second.get())==1);
 CHECK(scheduler.active_jobs()==1);CHECK(!active.hardware_enabled(p));
 active.release();
 auto hevc=scheduler.reserve({Codec::hevc});CHECK(hevc);CHECK(hevc.profiles().front().stable_id==g.stable_id);
}

TEST("ambiguous duplicate adapter identities never produce profiles"){
 auto c=cpu(),a=gpu("same-adapter",Backend::nvidia_nvenc),alias=a;alias.backend_index=1;
 auto profiles=run({c,a,alias},{probe(c,Codec::h264),probe(a,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,60,512)});
 CHECK(profiles.size()==1);CHECK(profiles.front().backend==Backend::cpu);
}
TEST("one physical card shares its session budget across codecs until drain"){
 auto c=cpu(),g=gpu("single-card",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264),probe(g,Codec::hevc)},
  {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,60,256),tune(g,Codec::hevc,55,1024)});
 Scheduler scheduler(limits(),profiles);
 auto first=scheduler.reserve({Codec::h264});CHECK(first);
 scheduler.set_device_available_vram(g.stable_id,4096); // VRAM is not a session limit.
 auto second=scheduler.reserve({Codec::hevc});CHECK(second);
 CHECK(second.profiles().front().backend==Backend::cpu);
 first.release();second.release();
 auto after=scheduler.reserve({Codec::hevc});CHECK(after.profiles().front().backend==Backend::nvidia_nvenc);
}
TEST("complete rendition jobs use both cards concurrently with a single CPU fallback slot"){
 auto c=cpu(),a=gpu("card-a",Backend::nvidia_nvenc),b=gpu("card-b",Backend::nvidia_nvenc);
 a.backend_index=0;b.backend_index=1;
 std::vector<ProbeEvidence> probes;std::vector<TuningEvidence> tuning;
 for(auto codec:{Codec::h264,Codec::hevc})for(auto d:{c,a,b}){
  probes.push_back(probe(d,codec));auto t=tune(d,codec,d.family==Backend::cpu?20:d.stable_id==a.stable_id?60:50,d.family==Backend::cpu?0:512);
  t.cpu_threads=1;t.ram_mb=512;t.scratch_mb=128;tuning.push_back(t);
 }
 Scheduler scheduler({2,1024,1024,256,2},run({c,a,b},probes,tuning));
 auto first=scheduler.reserve({Codec::h264,Codec::hevc}),second=scheduler.reserve({Codec::h264,Codec::hevc});
 CHECK(first&&second);CHECK(scheduler.active_jobs()==2);
 for(auto codec:{Codec::h264,Codec::hevc}){
  CHECK(find(first.profiles(),a.stable_id,codec));CHECK(find(second.profiles(),b.stable_id,codec));
 }
 CHECK(first.acquire_cpu_fallback([]{return false;},0));
 CHECK(!second.acquire_cpu_fallback([]{return false;},0));
 CHECK(!scheduler.reserve({Codec::h264})); // cancellation intent alone cannot release a reservation
 first.release();CHECK(second.acquire_cpu_fallback([]{return false;},0));second.release();
 CHECK(scheduler.active_jobs()==0);
}

TEST("different LUID aliases of a physical UUID are withheld while identical real cards stay independent"){
 auto c=cpu(),a=gpu("pci:10de:2488:153a10de:a1:luid:a",Backend::nvidia_nvenc),b=gpu("pci:10de:2488:153a10de:a1:luid:b",Backend::nvidia_nvenc);
 a.backend_index=0;b.backend_index=1;b.physical_id=a.physical_id;
 auto select=[&]{return run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,60,512),tune(b,Codec::h264,55,512)});};
 CHECK(select().size()==1); // two supposedly verified aliases are ambiguous: neither may schedule
 b.physical_id="different-real-uuid";CHECK(select().size()==3);
 b.backend_identity_verified=false;b.backend_index=0;
 CHECK(find(select(),a.stable_id,Codec::h264)); // an unmappable alias cannot poison the real card
 CHECK(!find(select(),b.stable_id,Codec::h264));
 a.physical_id.clear();CHECK(select().size()==1);
}
TEST("scheduler rejects forged alias profiles and missing NVIDIA physical identities"){
 auto c=cpu(),g=gpu("original",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,60,512)});
 auto alias=*find(profiles,g.stable_id,Codec::h264);alias.stable_id="alias";alias.backend_index=1;
 profiles.push_back(alias);Scheduler duplicated(limits(),profiles);
 auto reserved=duplicated.reserve({Codec::h264});CHECK(reserved);CHECK(reserved.profiles().front().backend==Backend::cpu);
 profiles.pop_back();for(auto& p:profiles)if(p.backend!=Backend::cpu)p.physical_id.clear();
 Scheduler missing(limits(),profiles);auto cpu_only=missing.reserve({Codec::h264});CHECK(cpu_only.profiles().front().backend==Backend::cpu);
}
TEST("NVIDIA worker session ceiling is shared by all cards independently of VRAM"){
 auto c=cpu();std::vector<Device> devices{c};std::vector<ProbeEvidence> probes{probe(c,Codec::h264)};
 auto ct=tune(c,Codec::h264,20);ct.tested_concurrency=4;std::vector<TuningEvidence> tuning{ct};
 for(unsigned i=0;i<3;++i){auto d=gpu(("card-"+std::to_string(i)).c_str(),Backend::nvidia_nvenc);d.backend_index=i;
  devices.push_back(d);probes.push_back(probe(d,Codec::h264));tuning.push_back(tune(d,Codec::h264,60-i,512));}
 auto profiles=run(devices,probes,tuning);Scheduler scheduler({8,8192,8192,4096,4},profiles);
 auto a=scheduler.reserve({Codec::h264}),b=scheduler.reserve({Codec::h264}),third=scheduler.reserve({Codec::h264});
 CHECK(a&&b&&third);CHECK(a.profiles().front().backend==Backend::nvidia_nvenc);CHECK(b.profiles().front().backend==Backend::nvidia_nvenc);
 CHECK(third.profiles().front().backend==Backend::cpu);
}

TEST("unrelated release and reservation moves cannot steal a running CPU fallback slot"){
 auto c=cpu(),a=gpu("a",Backend::nvidia_nvenc),b=gpu("b",Backend::nvidia_nvenc);b.backend_index=1;
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,60,512),tune(b,Codec::h264,55,512)});
 Scheduler scheduler(limits(),profiles);auto owner=scheduler.reserve({Codec::h264}),other=scheduler.reserve({Codec::h264});
 CHECK(owner.acquire_cpu_fallback([]{return false;},0));
 CHECK(!scheduler.reserve({Codec::h264})); // a new primary CPU job cannot steal the active fallback
 other.release();auto replacement=scheduler.reserve({Codec::h264});CHECK(replacement);
 CHECK(!replacement.acquire_cpu_fallback([]{return false;},0));
 Scheduler::Reservation moved(std::move(owner));CHECK(moved.acquire_cpu_fallback([]{return false;},0));
 Scheduler::Reservation assigned;assigned=std::move(moved);CHECK(!moved);CHECK(assigned.acquire_cpu_fallback([]{return false;},0));
 assigned.release();CHECK(replacement.acquire_cpu_fallback([]{return false;},0));
}
TEST("profile device totals count encodable cards once across codecs"){
 auto c=cpu(),g=gpu("physical",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(c,Codec::hevc),probe(g,Codec::h264),probe(g,Codec::hevc)},
  {tune(c,Codec::h264,20),tune(c,Codec::hevc,12),tune(g,Codec::h264,60,512),tune(g,Codec::hevc,55,1024)});
 CHECK(schedulable_device_count(profiles)==1);CHECK(profile_vram_capacity(profiles)==1024);
 auto record=profile_record(*find(profiles,g.stable_id,Codec::h264));
 CHECK(record.at("physical_id").as_string()==g.physical_id);
 CHECK(record.at("device_encode_sessions").to_number<unsigned>()==1);
 CHECK(record.at("worker_nvenc_sessions").to_number<unsigned>()==2);
 CHECK(schedulable_device_count({})==0);CHECK(profile_vram_capacity({})==0);
}

TEST("physical UUID drift invalidates a saved profile even when the DXGI identity stayed the same"){
 auto c=cpu(),g=gpu("unchanged-luid",Backend::nvidia_nvenc);
 auto profiles=run({c,g},{probe(c,Codec::h264),probe(g,Codec::h264)},
  {tune(c,Codec::h264,20),tune(g,Codec::h264,60,512)});
 auto p=*find(profiles,g.stable_id,Codec::h264);CHECK(profile_current(p,g,hash));
 g.physical_id="different-physical-card";CHECK(!profile_current(p,g,hash));
}
TEST("simultaneous reservations cannot create a third hardware session or reuse a busy card"){
 auto c=cpu(),a=gpu("card-a",Backend::nvidia_nvenc),b=gpu("card-b",Backend::nvidia_nvenc);b.backend_index=1;
 auto profiles=run({c,a,b},{probe(c,Codec::h264),probe(a,Codec::h264),probe(b,Codec::h264)},
  {tune(c,Codec::h264,20),tune(a,Codec::h264,60,512),tune(b,Codec::h264,55,512)});
 for(auto& p:profiles){p.cpu_threads=1;p.ram_mb=256;p.scratch_mb=128;if(p.backend==Backend::cpu)p.max_concurrency=4;}
 Scheduler scheduler({8,4096,8192,4096,8},profiles);
 std::vector<std::future<Scheduler::Reservation>> pending;
 for(unsigned i=0;i<8;++i)pending.push_back(std::async(std::launch::async,[&]{return scheduler.reserve({Codec::h264});}));
 std::vector<Scheduler::Reservation> held;std::set<std::string> cards;unsigned hardware=0;
 for(auto& future:pending){auto reservation=future.get();if(!reservation)continue;
  const auto& primary=reservation.profiles().front();if(primary.backend!=Backend::cpu){++hardware;CHECK(cards.insert(primary.physical_id).second);}
  held.push_back(std::move(reservation));}
 CHECK(hardware==2);CHECK(held.size()==6);CHECK(scheduler.active_jobs()==6);
 held.clear();CHECK(scheduler.active_jobs()==0);
}
TEST("opposite codec speed winners do not split each rendition job across both cards"){
 auto c=cpu(),a=gpu("card-a",Backend::nvidia_nvenc),b=gpu("card-b",Backend::nvidia_nvenc);b.backend_index=1;
 std::vector<ProbeEvidence> probes;std::vector<TuningEvidence> tuning;
 for(auto codec:{Codec::h264,Codec::hevc})for(auto d:{c,a,b}){
  probes.push_back(probe(d,codec));auto t=tune(d,codec,d.family==Backend::cpu?20:(codec==Codec::h264)==(d.stable_id==a.stable_id)?60:50,d.family==Backend::cpu?0:512);
  t.cpu_threads=1;t.ram_mb=256;t.scratch_mb=128;tuning.push_back(t);
 }
 Scheduler scheduler({4,2048,1024,512,4},run({c,a,b},probes,tuning));
 auto first=scheduler.reserve({Codec::h264,Codec::hevc}),second=scheduler.reserve({Codec::h264,Codec::hevc});
 CHECK(first&&second);
 for(auto codec:{Codec::h264,Codec::hevc}){CHECK(find(first.profiles(),a.stable_id,codec));CHECK(find(second.profiles(),b.stable_id,codec));}
}
