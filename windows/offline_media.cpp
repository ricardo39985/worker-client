#include "offline_media.hpp"
#include "ow/device_identity.hpp"
#include <iostream>
namespace ow::win {
json::object OfflineMediaHarness::run(Config config,const Probe& proof,const std::filesystem::path& source,
 const std::map<adaptive::Codec,std::string>& pins,unsigned jobs,unsigned concurrency,std::uint64_t minimum_ms,unsigned warmups){
 if(!proof.media_video||!jobs||!concurrency||concurrency>config.max_jobs||jobs>512)throw std::runtime_error("Invalid offline media proof/batch");
 if(!std::filesystem::create_directory(config.root))throw std::runtime_error("Offline worker root must be new");
 config.coordinator="https://offline.invalid";config.storage_hosts.clear();
 auto worker=std::unique_ptr<Worker>(new Worker(config,true));
 worker->offline_source_=source;worker->offline_output_=config.root/"delivered";worker->offline_pins_=pins;
 worker->priority_=Priority::low;worker->safe_log("ISOLATED SETUP priority");
 worker->hardware_profiles_=proof.profiles;
 worker->hardware_profiles_.erase(std::remove_if(worker->hardware_profiles_.begin(),worker->hardware_profiles_.end(),[&](const auto& p){
  return p.backend!=adaptive::Backend::cpu&&(!pins.contains(p.codec)||pins.at(p.codec)!=p.stable_id);
 }),worker->hardware_profiles_.end());
 worker->safe_log("ISOLATED SETUP profiles");
 for(const auto& [codec,id]:pins)if(std::none_of(worker->hardware_profiles_.begin(),worker->hardware_profiles_.end(),[&](const auto& p){return p.codec==codec&&p.stable_id==id;}))throw std::runtime_error("Pinned backend lacks current encode/decode proof");
 const auto vram=config.vram_override_set?config.budget.vram_mb:adaptive::profile_vram_capacity(worker->hardware_profiles_);
 adaptive::Limits caps{config.budget.cpu_threads,config.budget.ram_mb,vram,config.budget.scratch_mb,static_cast<unsigned>(config.max_jobs)};
#ifndef OW_FLEX_BASELINE
 caps.nvenc_sessions=adaptive::proven_nvenc_sessions(worker->hardware_profiles_);
#endif
 worker->adaptive_scheduler_=std::make_unique<adaptive::Scheduler>(caps,worker->hardware_profiles_);
 worker->safe_log("ISOLATED SETUP scheduler");
 worker->admission_.capability("media.video.renditions.v1",true,Preference::allowed);
 worker->safe_log("ISOLATED SETUP before watchdog");
 worker->watchdog_=std::jthread([&worker]{worker->watchdog();});worker->safe_log("ISOLATED SETUP watchdog started");
 const auto digest=sha256_file(source);const auto size=std::filesystem::file_size(source);
 auto batch=[&](unsigned minimum,unsigned concurrent,std::uint64_t duration,const std::string& prefix){
  struct Flight {std::shared_ptr<Worker::TaskData> data;Tick started,arrived;std::uint64_t renew{};};
  std::map<std::string,Flight> live;json::array records;unsigned submitted=0;const auto started=monotonic_ms(),started_utc=utc_ms();Tick next_renew=0,queued_at=started;
  for(;;){
   const auto now=monotonic_ms();
   for(auto i=live.begin();i!=live.end();){
    if(!i->second.data->done){++i;continue;}
    bool found=false;
    for(const auto& pending:worker->store_.pending())if(pending.attempt_id==i->first){
     auto result=parse(pending.body).as_object();
     if(text(result,"status",32)!="succeeded")throw std::runtime_error("Offline actual worker failed: "+text(result,"reason",4096));
     result["wall_ms"]=now-i->second.started;result["offered_to_result_ms"]=now-i->second.arrived;records.emplace_back(std::move(result));
     if(!worker->store_.acknowledge(pending.sequence,pending.attempt_id))throw std::runtime_error("Offline durable result ACK mismatch");found=true;break;
    }
    if(!found)throw std::runtime_error("Completed offline job lacks durable result");
    i=live.erase(i);
   }
   const bool enough=submitted>=minimum&&static_cast<std::uint64_t>(now-started)>=duration;
   if(enough&&live.empty())break;
   if(now-started>900000)throw std::runtime_error("Offline batch exceeded fifteen minutes");
   if(!enough&&live.size()<concurrent){
    if(submitted>=512)throw std::runtime_error("Offline batch count safety ceiling reached before window filled");
    const auto id=prefix+std::to_string(submitted);
    JobSpec spec;spec.offer={"job-"+id,id,"media.video.renditions.v1",digest+id,Resources{512,0,512,1},now+60000};
    spec.input_bytes=size;spec.input_sha256=digest;spec.input_url="https://offline.invalid/source";spec.copy_audio=true;
    spec.output_max_bytes=100*1024*1024;spec.timeout_ms=600000;
    spec.artifacts={{"feed","https://offline.invalid/feed","video/mp4"},{"feed_optimized","https://offline.invalid/hevc","video/mp4"},{"thumbnail","https://offline.invalid/poster","image/webp"}};
    if(worker->admission_.offer(spec.offer,now,available_resources(config.root,config.reserve_ram_mb)).accepted){
     if(worker->admission_.begin(id,"offline-lease",now+300000,now)!=Start::started)throw std::runtime_error("Offline BEGIN failed");
     auto data=std::make_shared<Worker::TaskData>();data->spec=spec;data->token="offline-lease";data->hard_deadline=now+600000;
     bool ready=true;
#ifdef OW_FLEX_BASELINE
     auto reservation=worker->reserve_hardware_profiles({adaptive::Codec::h264,adaptive::Codec::hevc},data->spec.offer.resources,id);
     ready=static_cast<bool>(reservation);
     if(ready)for(const auto& [codec,pin]:pins){auto primary=std::find_if(reservation.profiles().begin(),reservation.profiles().end(),[&](const auto& p){return p.codec==codec;});if(primary==reservation.profiles().end()||primary->stable_id!=pin)ready=false;}
     if(ready)data->profile_reservation.emplace(std::move(reservation));
#endif
     if(!ready){worker->admission_.finish(id);}
     else{
      if(!worker->store_.reserve(id,spec.offer.job_id,spec.offer.fingerprint)||!worker->store_.start(id))throw std::runtime_error("Offline journal start failed");
      auto running=std::make_unique<Worker::Running>();running->data=data;
      {std::lock_guard lock(worker->report_mutex_);worker->report_.start(id,"offline video",utc_ms(),now);}
      {std::lock_guard lock(worker->mutex_);running->thread=std::jthread([&worker,data]{worker->execute(data);});worker->running_.emplace(id,std::move(running));}
      live.emplace(id,Flight{data,now,queued_at,0});++submitted;queued_at=now;
     }
    }
   }
   if(now>=next_renew){for(auto& [id,f]:live)worker->admission_.renew(id,"offline-lease",++f.renew,now+300000,now);next_renew=now+1000;}
   Sleep(10);
  }
  return json::object{{"wall_ms",monotonic_ms()-started},{"started_utc_ms",started_utc},{"ended_utc_ms",utc_ms()},{"jobs",std::move(records)}};
 };
 if(warmups)batch(warmups,concurrency,60000,"warmup-");
 auto result=batch(jobs,concurrency,minimum_ms,"measured-");
 result["source_sha256"]=digest;result["source_bytes"]=size;result["offline_storage_only"]=true;
 result["child_priority"]="low";result["coordinator_acceptance"]=false;
 return result;
}
}
