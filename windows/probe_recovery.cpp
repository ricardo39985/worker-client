#include "worker.hpp"
#include "hardware.hpp"
#include "ow/adaptive_evidence.hpp"
namespace ow::win {
void Worker::recover_gpus(){
 while(!stop_&&!exit_requested_){
  for(unsigned elapsed=0;elapsed<1000&&!stop_&&!exit_requested_;elapsed+=100)Sleep(100);
  if(stop_||exit_requested_||paused_)continue;
  std::uint64_t generation{};std::vector<ProbeFailure> pending;Probe retained;
  {
   std::lock_guard lock(mutex_);
   if(!verified_media_video_||!gpu_recovery_.due(monotonic_ms())||!adaptive_scheduler_||
      adaptive_scheduler_->active_jobs()!=0||!admission_.begin_probe())continue;
   generation=hardware_generation_;pending=gpu_recovery_.pending();
   retained.video=verified_video_;retained.image=verified_image_;retained.media_video=verified_media_video_;retained.media_image=verified_media_image_;retained.profiles=hardware_profiles_;
  }
  // begin_probe atomically excludes offers and requires all reserved, running,
  // and cancelling jobs to have drained. Network, renewals and watchdog remain
  // on their own threads. Operator pause is independent and never cleared here.
  struct ProbeGuard { Admission& admission;ProbeStatus& status;~ProbeGuard(){status.finish();admission.end_probe();} } guard{admission_,probe_status_};
  try{
   safe_log("GPU RETRY starting fresh idle probe epoch; new offers deferred");
   auto cancelled=[&]{return stop_||exit_requested_||paused_||!verified_media_video_;};
   auto isolated=config_;isolated.root=config_.root/"gpu-recovery";
   std::filesystem::create_directories(isolated.root);
   auto probe=probe_ffmpeg(isolated,cancelled,[this](std::string phase,unsigned current,unsigned total){
    probe_status_.phase("GPU retry "+phase,current,total);
   },&retained);
   const auto devices=enumerate_video_devices();
   for(const auto& profile:probe.profiles)if(profile.backend!=adaptive::Backend::cpu){
    const auto d=std::find_if(devices.begin(),devices.end(),[&](const auto& item){return adaptive::profile_current(profile,item,config_.ffmpeg_sha256);});
    if(d==devices.end())throw std::runtime_error("GPU identity changed during recovery probe");
   }
   if(cancelled()||!probe.complete||!probe.media_video||sha256_file(config_.ffmpeg)!=config_.ffmpeg_sha256)
    throw std::runtime_error("Recovery epoch incomplete or cancelled; retained current profiles");
   std::lock_guard lock(mutex_);
   if(generation!=hardware_generation_||cancelled())throw std::runtime_error("Recovery evidence invalidated before admission");
   // Inventory/mapping failures must not silently forget previously withheld
   // cards, even if the new probe did not reach their encoder command.
   for(const auto& old:pending){
    const auto codec=old.codec=="hevc"?adaptive::Codec::hevc:adaptive::Codec::h264;
    const bool proven=std::any_of(probe.profiles.begin(),probe.profiles.end(),[&](const auto& p){return p.stable_id==old.device_id&&p.codec==codec;});
    const bool failed=std::any_of(probe.gpu_failures.begin(),probe.gpu_failures.end(),[&](const auto& f){return f.device_id==old.device_id&&f.codec==old.codec;});
    if(!proven&&!failed)probe.gpu_failures.push_back(old);
   }
   const auto vram=config_.vram_override_set?config_.budget.vram_mb:adaptive::profile_vram_capacity(probe.profiles);
   auto scheduler=std::make_unique<adaptive::Scheduler>(adaptive::Limits{config_.budget.cpu_threads,config_.budget.ram_mb,vram,config_.budget.scratch_mb,static_cast<std::uint32_t>(config_.max_jobs)},probe.profiles);
   atomic_write(config_.root/"adaptive-hardware.json",read_file(isolated.root/"adaptive-hardware.json"));
   hardware_profiles_=std::move(probe.profiles);adaptive_scheduler_=std::move(scheduler);++hardware_generation_;
   capability_report_.probed(verified_video_,verified_image_,verified_media_video_,verified_media_image_,hardware_profiles_);
   gpu_recovery_.failed(std::move(probe.gpu_failures),monotonic_ms());
   safe_log(capability_report_.record());
   safe_log("PROBE DEVICES schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(hardware_profiles_)));
   safe_log("GPU RETRY completed failed-card encode/decode/tuning epoch; schedulable GPUs="+std::to_string(adaptive::schedulable_device_count(hardware_profiles_)));
   for(const auto& line:gpu_recovery_.status(monotonic_ms()))safe_log("GPU RETRY "+line);
   if(!probe.reason.empty())safe_log("PROBE DETAIL "+probe.reason);
  }catch(const std::exception& e){
   std::lock_guard lock(mutex_);gpu_recovery_.failed(std::move(pending),monotonic_ms());
   safe_log(capability_report_.record());
   safe_log(std::string("GPU RETRY deferred: ")+e.what());
  }

 }
}
}
