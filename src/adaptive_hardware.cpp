#include "ow/adaptive_hardware.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <thread>
namespace ow::adaptive {
namespace {
const ProbeEvidence* find_probe(const std::vector<ProbeEvidence>& all,const Device& d,
                                Codec codec,const std::string& hash){
 auto i=std::find_if(all.begin(),all.end(),[&](const auto& p){
  return p.stable_id==d.stable_id&&p.driver_version==d.driver_version&&p.backend==d.family&&
         p.codec==codec&&p.ffmpeg_sha256==hash&&p.encoded&&p.decoded&&p.output_valid;
 });
 return i==all.end()?nullptr:&*i;
}
const TuningEvidence* find_tuning(const std::vector<TuningEvidence>& all,const Device& d,Codec codec){
 auto i=std::find_if(all.begin(),all.end(),[&](const auto& t){
  return t.stable_id==d.stable_id&&t.backend==d.family&&t.codec==codec&&t.completed&&
         t.output_valid&&t.cancelled_cleanly&&std::isfinite(t.frames_per_second)&&t.frames_per_second>0&&
         t.cpu_threads>0&&t.ram_mb>0&&t.scratch_mb>0&&t.tested_concurrency>0&&
         (d.family==Backend::cpu?t.vram_mb==0:(t.vram_mb>0&&t.max_width>0&&t.max_height>0));
 });
 return i==all.end()?nullptr:&*i;
}
std::uint64_t concurrency(std::uint64_t cap,std::uint64_t each){return each?cap/each:0;}
}
TuningComparison compare_tuning_runs(const std::vector<std::uint64_t>& serial,
 const std::vector<std::uint64_t>& parallel,std::uint32_t sr,std::uint32_t pr,
 std::uint64_t sf,std::uint64_t pf,std::uint64_t sc,std::uint64_t pc,
 std::uint64_t sm,std::uint64_t pm,std::uint64_t ss,std::uint64_t ps){
 TuningComparison result;
 if(!sr||sr!=pr||serial.size()!=sr||parallel.size()!=pr||!sf||sf!=pf||!sc||sc!=pc||!sm||sm!=pm||!ss||ss!=ps)return result;
 std::uint64_t st=0,pt=0;for(auto n:serial){if(!n||st>UINT64_MAX-n)return result;st+=n;}
 for(auto n:parallel){if(!n||pt>UINT64_MAX-n)return result;pt+=n;}
 result.serial_fps=static_cast<double>(sf)*1000.0/static_cast<double>(st);
 result.parallel_fps=static_cast<double>(pf)*1000.0/static_cast<double>(pt);result.comparable=true;return result;
}
std::vector<Profile> choose_profiles(const std::vector<Device>& devices,
                                     const std::vector<ProbeEvidence>& probes,
                                     const std::vector<TuningEvidence>& tuning,
                                     const Limits& limits,const std::string& hash) {
 std::vector<Profile> out;
 if(hash.empty()||!limits.max_jobs||!limits.cpu_threads||!limits.ram_mb||!limits.scratch_mb)return out;
 for(auto codec:{Codec::h264,Codec::hevc}){
  const Device* cpu_device=nullptr;const TuningEvidence* cpu_tuning=nullptr;
  for(const auto& d:devices)if(d.family==Backend::cpu&&d.stable_id=="cpu"&&d.driver_healthy&&
    find_probe(probes,d,codec,hash)){
   if(auto t=find_tuning(tuning,d,codec)){cpu_device=&d;cpu_tuning=t;break;}
  }
  if(!cpu_device||!cpu_tuning)continue;
  auto add=[&](const Device& d,const TuningEvidence& t){
   auto concurrency_cap=std::min<std::uint64_t>(t.tested_concurrency,limits.max_jobs);
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.cpu_threads,t.cpu_threads));
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.ram_mb,t.ram_mb));
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.scratch_mb,t.scratch_mb));
   if(d.family!=Backend::cpu){
    concurrency_cap=std::min(concurrency_cap,concurrency(limits.vram_mb,t.vram_mb));
    concurrency_cap=std::min(concurrency_cap,concurrency(d.dedicated_vram_mb,t.vram_mb));
   }
   if(!concurrency_cap||t.cpu_threads>limits.cpu_threads||t.ram_mb>limits.ram_mb||
      t.scratch_mb>limits.scratch_mb||
      (d.family!=Backend::cpu&&(!d.driver_healthy||d.dedicated_vram_mb<t.vram_mb||t.vram_mb>limits.vram_mb)))return;
   out.push_back({d.stable_id,d.driver_version,hash,d.family,codec,t.cpu_threads,t.ram_mb,t.vram_mb,
                  t.scratch_mb,static_cast<std::uint32_t>(concurrency_cap),t.frames_per_second,d.adapter_index,d.backend_index,d.backend_identity_verified,t.max_width,t.max_height});
  };
  add(*cpu_device,*cpu_tuning);
  for(const auto& d:devices){
   if(d.family==Backend::cpu||!d.driver_healthy||!d.backend_identity_verified||d.stable_id.empty()||d.driver_version.empty()||
      !d.dedicated_vram_mb||!find_probe(probes,d,codec,hash))continue;
   if(std::count_if(devices.begin(),devices.end(),[&](const auto& other){
       return other.family==d.family&&other.backend_index==d.backend_index;
      })!=1)continue;
   auto t=find_tuning(tuning,d,codec);if(!t||t->vram_mb>d.dedicated_vram_mb)continue;
   // Hardware is selected only after a repeatable measured improvement over
   // the verified CPU baseline; CRF/quality equivalence is never inferred.
   if(t->frames_per_second<cpu_tuning->frames_per_second*1.05)continue;
   add(d,*t);
  }
 }
 return out;
}
std::optional<GpuMemoryTotals> gpu_memory_totals(const std::vector<GpuMemoryRecord>& records,
                                                std::uint32_t process_id,const std::string& stable_id){
 if(stable_id.empty())return {};
 GpuMemoryTotals totals;std::map<std::pair<std::uint32_t,std::uint32_t>,bool> seen;
 bool adapter_seen=false,child_seen=false;
 for(const auto& record:records){
  if(record.stable_id!=stable_id)continue;
  if(!record.valid)return {};
  adapter_seen=true;
  if(!seen.emplace(std::pair{record.process_id,record.node},true).second)return {};
  if(record.bytes>UINT64_MAX-totals.adapter_bytes)return {};
  totals.adapter_bytes+=record.bytes;
  if(record.process_id==process_id){
   child_seen=true;if(record.bytes>UINT64_MAX-totals.child_bytes)return {};
   totals.child_bytes+=record.bytes;
  }
 }
 if(!adapter_seen||!totals.adapter_bytes)return {};
 if(process_id!=0&&(!child_seen||!totals.child_bytes))return {};
 return totals;
}
std::optional<std::uint32_t> resolve_unique_backend_ordinal(const std::vector<BackendIdentity>& mappings,
                                                           const std::string& stable_id){
 if(stable_id.empty())return {};
 std::optional<std::uint32_t> result;
 for(const auto& mapping:mappings)if(mapping.stable_id==stable_id){
  if(!mapping.node_mask||result)return {};
  result=mapping.ordinal;
 }
 if(!result)return {};
 for(const auto& mapping:mappings)if(mapping.stable_id!=stable_id&&mapping.ordinal==*result)return {};
 return result;
}
bool profile_current(const Profile& p,const Device& d,const std::string& hash){
 return !p.stable_id.empty()&&p.stable_id==d.stable_id&&p.backend==d.family&&
        p.driver_version==d.driver_version&&p.ffmpeg_sha256==hash&&d.driver_healthy&&
        p.adapter_index==d.adapter_index&&p.backend_index==d.backend_index&&
        (d.family==Backend::cpu||d.backend_identity_verified)&&
        (d.family==Backend::cpu||d.dedicated_vram_mb>=p.vram_mb);
}
bool profile_supports_dimensions(const Profile& p,std::uint32_t width,std::uint32_t height){
 return p.backend==Backend::cpu||(width&&height&&p.max_width&&p.max_height&&width<=p.max_width&&height<=p.max_height);
}
struct Scheduler::State {
 mutable std::mutex mutex;
 Limits limits;
 std::vector<Profile> profiles;
 std::uint64_t jobs{},cpu{},ram{},scratch{},vram{};
 std::map<std::string,std::uint64_t> device_vram,device_caps;
 std::map<std::string,std::uint32_t> device_jobs;
 std::map<std::string,std::uint32_t> profile_jobs;
 std::map<std::string,std::uint32_t> primary_jobs;
 std::map<std::string,bool> fallback_busy;
};
namespace {
std::string profile_key(const Profile& p){return p.stable_id+":"+std::to_string(static_cast<int>(p.codec));}
struct ReservationCost {std::uint64_t cpu{},ram{},scratch{},vram{};std::map<std::string,std::uint64_t> devices;};
ReservationCost aggregate_cost(const std::vector<Profile>& profiles){
 std::map<Codec,ReservationCost> codecs;
 for(const auto& p:profiles){auto& c=codecs[p.codec];c.cpu=std::max(c.cpu,p.cpu_threads);c.ram=std::max(c.ram,p.ram_mb);c.scratch=std::max(c.scratch,p.scratch_mb);
  if(p.backend!=Backend::cpu)c.devices[p.stable_id]=std::max(c.devices[p.stable_id],p.vram_mb);
 }
 ReservationCost total;
 for(const auto& [_,c]:codecs){total.cpu=std::max(total.cpu,c.cpu);total.ram=std::max(total.ram,c.ram);total.scratch=std::max(total.scratch,c.scratch);
  for(const auto& [id,mb]:c.devices)total.devices[id]=std::max(total.devices[id],mb);}
 for(const auto& [_,mb]:total.devices)total.vram+=mb;
 return total;
}
}
Scheduler::Scheduler(Limits limits,std::vector<Profile> profiles):state_(std::make_shared<State>()){
 state_->limits=limits;state_->profiles=std::move(profiles);
 for(const auto& p:state_->profiles)if(p.backend!=Backend::cpu)
  state_->device_caps[p.stable_id]=std::max(state_->device_caps[p.stable_id],p.vram_mb*p.max_concurrency);
}
Scheduler::Reservation::Reservation(std::shared_ptr<State> state,std::vector<Profile> profiles)
 :state_(std::move(state)),profiles_(std::move(profiles)),active_(true){}
Scheduler::Reservation::Reservation(Reservation&& other) noexcept
 :state_(std::move(other.state_)),profiles_(std::move(other.profiles_)),active_(other.active_){other.active_=false;}
Scheduler::Reservation& Scheduler::Reservation::operator=(Reservation&& other) noexcept{
 if(this!=&other){release();state_=std::move(other.state_);profiles_=std::move(other.profiles_);active_=other.active_;other.active_=false;}return *this;
}
Scheduler::Reservation::~Reservation(){release();}
void Scheduler::Reservation::release() noexcept{if(active_){Scheduler::release(state_,profiles_);active_=false;}}
bool Scheduler::Reservation::acquire_cpu_fallback(const std::function<bool()>& cancelled,std::uint32_t wait_ms){
 if(!active_||!state_)return false;
 auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(wait_ms);
 for(;;){if(cancelled&&cancelled())return false;bool ready=true;{
  std::lock_guard lock(state_->mutex);
  for(const auto& p:profiles_)if(p.backend==Backend::cpu){
   const bool is_fallback=std::any_of(profiles_.begin(),profiles_.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;});
   if(!is_fallback)continue;
   const auto key=profile_key(p);const bool owned=std::find(cpu_fallback_keys_.begin(),cpu_fallback_keys_.end(),key)!=cpu_fallback_keys_.end();
   if(state_->primary_jobs[key]>=p.max_concurrency||(!owned&&state_->fallback_busy[key]))ready=false;
  }
  if(ready){for(const auto& p:profiles_)if(p.backend==Backend::cpu&&std::any_of(profiles_.begin(),profiles_.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;})){const auto key=profile_key(p);if(std::find(cpu_fallback_keys_.begin(),cpu_fallback_keys_.end(),key)==cpu_fallback_keys_.end()){state_->fallback_busy[key]=true;cpu_fallback_keys_.push_back(key);}}return true;}
 }if(std::chrono::steady_clock::now()>=deadline)return false;std::this_thread::sleep_for(std::chrono::milliseconds(20));}
}
void Scheduler::release(const std::shared_ptr<State>& state,const std::vector<Profile>& profiles) noexcept{
 if(!state)return;
 std::lock_guard lock(state->mutex);
 const auto cost=aggregate_cost(profiles);std::map<std::string,bool> released_devices;
 for(const auto& p:profiles){
  const bool has_gpu=std::any_of(profiles.begin(),profiles.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;});
  if(p.backend!=Backend::cpu&&!released_devices[p.stable_id]){--state->device_jobs[p.stable_id];released_devices[p.stable_id]=true;}
  if(p.backend!=Backend::cpu||!has_gpu)--state->primary_jobs[profile_key(p)];
  if(p.backend==Backend::cpu)state->fallback_busy[profile_key(p)]=false;
 }
 state->cpu-=cost.cpu;state->ram-=cost.ram;state->scratch-=cost.scratch;state->vram-=cost.vram;
 for(const auto& [id,mb]:cost.devices)state->device_vram[id]-=mb;
 --state->jobs;
}
Scheduler::Reservation Scheduler::reserve(const std::vector<Codec>& codecs){
 return reserve(codecs,state_->limits);
}
Scheduler::Reservation Scheduler::reserve(const std::vector<Codec>& codecs,const Limits& task_limits){
 auto s=state_;std::lock_guard lock(s->mutex);
 if(!s->limits.max_jobs||s->jobs>=s->limits.max_jobs)return {};
 // Task limits describe the maximum cost of this one job. Machine limits
 // describe aggregate reservations across all active jobs, so never compare
 // accumulated usage to the task-level ceiling.
 const Limits task_caps{std::min(s->limits.cpu_threads,task_limits.cpu_threads),
                        std::min(s->limits.ram_mb,task_limits.ram_mb),
                        std::min(s->limits.vram_mb,task_limits.vram_mb),
                        std::min(s->limits.scratch_mb,task_limits.scratch_mb),1};
 std::vector<Profile> chosen;
 for(auto codec:codecs){
  std::vector<const Profile*> candidates;
  for(const auto& p:s->profiles){
   if(p.codec!=codec||!p.max_concurrency||p.stable_id.empty())continue;
   if(p.backend!=Backend::cpu&&!p.backend_identity_verified)continue;
   if(p.backend==Backend::cpu){if(s->primary_jobs[profile_key(p)]>=p.max_concurrency)continue;}
   else if(s->primary_jobs[profile_key(p)]>=p.max_concurrency)continue;
   candidates.push_back(&p);
  }
  std::sort(candidates.begin(),candidates.end(),[](const auto* a,const auto* b){
   if(a->measured_frames_per_second!=b->measured_frames_per_second)return a->measured_frames_per_second>b->measured_frames_per_second;
   return a->stable_id<b->stable_id;
  });
  if(std::any_of(chosen.begin(),chosen.end(),[&](const auto& p){return p.codec==codec;}))continue;
  bool admitted=false;
  for(const auto* candidate:candidates){
   std::vector<Profile> for_job{*candidate};
   if(candidate->backend!=Backend::cpu){
    const Profile* fallback=nullptr;
    for(const auto& p:s->profiles)if(p.codec==codec&&p.backend==Backend::cpu&&p.max_concurrency&&
       (!fallback||p.measured_frames_per_second>fallback->measured_frames_per_second))fallback=&p;
    if(!fallback)continue;
    for_job.push_back(*fallback);
   }
   auto combined=chosen;combined.insert(combined.end(),for_job.begin(),for_job.end());const auto projected=aggregate_cost(combined);
   if(projected.cpu>task_caps.cpu_threads||projected.ram>task_caps.ram_mb||projected.scratch>task_caps.scratch_mb||projected.vram>task_caps.vram_mb)continue;
   if(projected.cpu+s->cpu>s->limits.cpu_threads||projected.ram+s->ram>s->limits.ram_mb||projected.scratch+s->scratch>s->limits.scratch_mb||projected.vram+s->vram>s->limits.vram_mb)continue;
   bool device_fit=true;for(const auto& [id,mb]:projected.devices)if(mb+s->device_vram[id]>s->device_caps[id])device_fit=false;
   if(!device_fit)continue;
   chosen.insert(chosen.end(),for_job.begin(),for_job.end());admitted=true;break;
  }
  if(!admitted)return {};
 }
 const auto cost=aggregate_cost(chosen);
 if(cost.cpu>task_caps.cpu_threads||cost.ram>task_caps.ram_mb||
    cost.scratch>task_caps.scratch_mb||cost.vram>task_caps.vram_mb)return {};
 if(cost.cpu+s->cpu>s->limits.cpu_threads||cost.ram+s->ram>s->limits.ram_mb||
    cost.scratch+s->scratch>s->limits.scratch_mb||cost.vram+s->vram>s->limits.vram_mb)return {};
 for(const auto& [id,mb]:cost.devices)if(mb+s->device_vram[id]>s->device_caps[id])return {};
 s->cpu+=cost.cpu;s->ram+=cost.ram;s->scratch+=cost.scratch;s->vram+=cost.vram;++s->jobs;
 for(const auto& [id,mb]:cost.devices){s->device_vram[id]+=mb;++s->device_jobs[id];}
 for(const auto& p:chosen)if(p.backend!=Backend::cpu||std::count_if(chosen.begin(),chosen.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;})==0)++s->primary_jobs[profile_key(p)];
 return Reservation(s,std::move(chosen));
}
void Scheduler::set_device_available_vram(const std::string& id,std::uint64_t mb){
 auto s=state_;std::lock_guard lock(s->mutex);auto found=s->device_caps.find(id);
 if(found!=s->device_caps.end())found->second=mb;
}
std::uint64_t Scheduler::active_jobs() const{auto s=state_;std::lock_guard lock(s->mutex);return s->jobs;}
void Scheduler::invalidate_hardware(){auto s=state_;std::lock_guard lock(s->mutex);s->profiles.erase(std::remove_if(s->profiles.begin(),s->profiles.end(),[](const auto& p){return p.backend!=Backend::cpu;}),s->profiles.end());}
void Scheduler::invalidate_all(){auto s=state_;std::lock_guard lock(s->mutex);s->profiles.clear();}
}
