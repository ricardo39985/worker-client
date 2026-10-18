#include "ow/adaptive_hardware.hpp"
#include "ow/device_identity.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
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
         (d.family==Backend::cpu?t.vram_mb==0:(t.vram_mb>0&&t.measured_width>0&&t.measured_height>0));
 });
 return i==all.end()?nullptr:&*i;
}
std::uint64_t concurrency(std::uint64_t cap,std::uint64_t each){return each?cap/each:0;}
}
namespace {
std::uint64_t footprint_mb(std::uint32_t width,std::uint32_t height,Codec codec,InputMemory input){
 // Envelope fitted above the successful 460x390..1080p samples in the A/B
 // ledger. HEVC and device surfaces have separate pool slopes. Above 1080p
 // this is a conservative extrapolation, never evidence of encode support.
 if(!width||!height||width>16384||height>16384)throw std::invalid_argument("GPU budget requires supported dimensions");
 const std::uint64_t bytes_per_pixel=input==InputMemory::host?(codec==Codec::h264?128:120):(codec==Codec::h264?112:108);
 return 256+(std::uint64_t{width}*height*bytes_per_pixel+1048575)/1048576;
}
}
std::uint64_t gpu_child_memory_mb(std::uint32_t width,std::uint32_t height,Codec codec,InputMemory input,std::uint64_t overhead){
 if(overhead>1048576)throw std::invalid_argument("GPU card overhead exceeds supported RAM budget");
 const auto footprint=footprint_mb(width,height,codec,input)+overhead;
 const auto guarded=footprint+std::max<std::uint64_t>(128,(footprint+3)/4);
 return ((guarded+127)/128)*128;
}
std::uint64_t gpu_card_overhead_mb(std::uint32_t width,std::uint32_t height,Codec codec,InputMemory input,std::uint64_t peak){
 const auto base=footprint_mb(width,height,codec,input);
 return std::max<std::uint64_t>(64,peak>base?peak-base:0);
}
VramBudget vram_budget(std::uint64_t declared,std::optional<std::uint64_t> bytes,std::uint64_t global){
 // With no usage sample, leave at least half the declared memory unbudgeted.
 if(!bytes)return {std::min({global,declared/2,std::uint64_t{1024}}),true};
 const auto used=*bytes/(1024*1024)+(*bytes%(1024*1024)!=0);
 return {std::min(global,declared>used?declared-used:0),false};
}
bool vram_envelope_ok(std::uint64_t declared,std::uint64_t reserved,std::optional<GpuMemoryTotals> sample){
 if(!declared||!reserved||reserved>declared)return false;
 if(!sample)return reserved<=vram_budget(declared,{},reserved).mb;
 const auto rounded_mb=[](std::uint64_t bytes){return bytes/(1024*1024)+(bytes%(1024*1024)!=0);};
 return rounded_mb(sample->child_bytes)<=reserved&&rounded_mb(sample->adapter_bytes)<=declared;
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
 auto verified=devices;verify_device_identities(verified);
 if(hash.empty()||!limits.max_jobs||!limits.cpu_threads||!limits.ram_mb||!limits.scratch_mb)return out;
 for(auto codec:{Codec::h264,Codec::hevc}){
  const Device* cpu_device=nullptr;const TuningEvidence* cpu_tuning=nullptr;
  for(const auto& d:devices)if(d.family==Backend::cpu&&d.stable_id=="cpu"&&d.driver_healthy&&
    find_probe(probes,d,codec,hash)){
   if(auto t=find_tuning(tuning,d,codec)){cpu_device=&d;cpu_tuning=t;break;}
  }
  if(!cpu_device||!cpu_tuning)continue;
  auto add=[&](const Device& d,const TuningEvidence& t){
   const auto ram=d.family==Backend::cpu?t.ram_mb:std::max(t.ram_mb,gpu_child_memory_mb(t.measured_width,t.measured_height,codec,InputMemory::host,t.child_memory_overhead_mb));
   auto concurrency_cap=std::min<std::uint64_t>(t.tested_concurrency,limits.max_jobs);
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.cpu_threads,t.cpu_threads));
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.ram_mb,ram));
   concurrency_cap=std::min(concurrency_cap,concurrency(limits.scratch_mb,t.scratch_mb));
   if(d.family!=Backend::cpu){
    concurrency_cap=std::min<std::uint64_t>(concurrency_cap,device_encode_sessions);
    concurrency_cap=std::min(concurrency_cap,concurrency(limits.vram_mb,t.vram_mb));
    concurrency_cap=std::min(concurrency_cap,concurrency(d.dedicated_vram_mb,t.vram_mb));
   }
   if(!concurrency_cap||t.cpu_threads>limits.cpu_threads||ram>limits.ram_mb||
      t.scratch_mb>limits.scratch_mb||
      (d.family!=Backend::cpu&&(!d.driver_healthy||d.dedicated_vram_mb<t.vram_mb||t.vram_mb>limits.vram_mb)))return;
   const auto geometry=encoder_geometry(d.family);
   out.push_back({d.stable_id,d.driver_version,hash,d.family,codec,t.cpu_threads,ram,t.vram_mb,
                  t.scratch_mb,static_cast<std::uint32_t>(concurrency_cap),t.frames_per_second,d.adapter_index,d.backend_index,d.backend_identity_verified,geometry.max_width,geometry.max_height,t.measured_width,t.measured_height,d.physical_id,d.name,t.child_memory_overhead_mb});
  };
  add(*cpu_device,*cpu_tuning);
  for(const auto& d:verified){
   if(d.family==Backend::cpu||!d.driver_healthy||!d.backend_identity_verified||d.stable_id.empty()||d.driver_version.empty()||
      !d.dedicated_vram_mb||!find_probe(probes,d,codec,hash))continue;
   auto t=find_tuning(tuning,d,codec);if(!t||t->vram_mb>d.dedicated_vram_mb)continue;
   // Only comparable 1080p-or-larger evidence can withhold a functional
   // profile for speed. A tiny or mismatched sample cannot decide real-video
   // performance; runtime still uses guarded validation and resource caps.
   // CRF/quality equivalence is never inferred.
   if(comparable_resolution(*t,*cpu_tuning)&&t->frames_per_second<cpu_tuning->frames_per_second*1.05)continue;
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
        (d.family==Backend::cpu||(d.backend_identity_verified&&canonical_instance_id(p.physical_id)==canonical_instance_id(d.physical_id)&&
          pci_instance_id(d.physical_id)))&&
        (d.family==Backend::cpu||d.dedicated_vram_mb>=p.vram_mb);
}
bool profile_supports_dimensions(const Profile& p,std::uint32_t width,std::uint32_t height){
 return p.backend==Backend::cpu||(width&&height&&!(width%2)&&!(height%2)&&p.max_width&&p.max_height&&width<=p.max_width&&height<=p.max_height);
}
EncoderGeometry encoder_geometry(Backend backend){
 // The approved FFmpeg CLI does not export a uniform per-device dimension-cap
 // query. Do not turn a successful sample into a hardware capability limit.
 // These are conservative admission ceilings, not proof of every size within.
 // NVENC: bound both axes to the older AVC 4096 envelope, even for HEVC.
 // QSV: bound both axes to 4096 for older Intel generations (no 8K inference).
 // AMF: cap at UHD, avoiding newer VCN-only 8K capability assumptions.
 // Driver/codec-specific restrictions inside these ceilings are caught by the
 // guarded full-size encode AND decode, with one demotion and CPU retry.
 // Encoder APIs: FFmpeg nvenc.c NV_ENC_CAPS_WIDTH_MAX/HEIGHT_MAX;
 // GPUOpen AMF_Video_Encode_API.md AMFCaps (hardware-specific).
 switch(backend){
  case Backend::nvidia_nvenc:return {4096,4096};
  case Backend::intel_qsv:return {4096,4096};
  case Backend::amd_amf:return {3840,2160};
  case Backend::cpu:return {}; // CPU is not constrained by a GPU envelope.
 }
 return {};
}
bool comparable_resolution(const TuningEvidence& hardware,const TuningEvidence& cpu){
 return hardware.measured_width>=1920&&hardware.measured_height>=1080&&
  hardware.measured_width==cpu.measured_width&&hardware.measured_height==cpu.measured_height;
}
GeometryEvidence geometry_evidence(const Profile& p,std::uint32_t width,std::uint32_t height){
 if(!width||!height)return GeometryEvidence::unproven;
 if(!profile_supports_dimensions(p,width,height))return GeometryEvidence::unsupported;
 if(width>p.measured_width||height>p.measured_height)return GeometryEvidence::unproven;
 return GeometryEvidence::measured;
}
std::string geometry_reason(GeometryEvidence evidence){
 switch(evidence){
  case GeometryEvidence::unsupported:return "unsupported geometry";
  case GeometryEvidence::unproven:return "GPU size not yet proven";
  case GeometryEvidence::measured:return "GPU size sampled";
 }
 return {};
}
struct Scheduler::State {
 mutable std::mutex mutex;
 Limits limits;
 std::vector<Profile> profiles;
 std::uint64_t jobs{},cpu{},ram{},scratch{},vram{},nvenc_jobs{};
 std::map<std::string,std::uint64_t> device_vram,device_caps;
 std::map<std::string,std::uint32_t> device_jobs;
 std::map<std::string,std::uint32_t> primary_jobs;
 std::map<std::string,bool> fallback_busy;
};
namespace {
std::string card_key(const Profile& p){return canonical_instance_id(p.physical_id);}
std::string profile_key(const Profile& p){return (p.backend==Backend::cpu?p.stable_id:card_key(p))+":"+std::to_string(static_cast<int>(p.codec));}
struct ReservationCost {std::uint64_t cpu{},ram{},scratch{},vram{};std::map<std::string,std::uint64_t> devices;std::set<std::string> nvenc_devices;};
ReservationCost aggregate_cost(const std::vector<Profile>& profiles){
 std::map<Codec,ReservationCost> codecs;
 for(const auto& p:profiles){auto& c=codecs[p.codec];c.cpu=std::max(c.cpu,p.cpu_threads);c.ram=std::max(c.ram,p.ram_mb);c.scratch=std::max(c.scratch,p.scratch_mb);
  if(p.backend!=Backend::cpu)c.devices[card_key(p)]=std::max(c.devices[card_key(p)],p.vram_mb);
  if(p.backend==Backend::nvidia_nvenc)c.nvenc_devices.insert(card_key(p));
 }
 ReservationCost total;
 for(const auto& [_,c]:codecs){total.nvenc_devices.insert(c.nvenc_devices.begin(),c.nvenc_devices.end());total.cpu=std::max(total.cpu,c.cpu);total.ram=std::max(total.ram,c.ram);total.scratch=std::max(total.scratch,c.scratch);
  for(const auto& [id,mb]:c.devices)total.devices[id]=std::max(total.devices[id],mb);}
 for(const auto& [_,mb]:total.devices)total.vram+=mb;
 return total;
}
}
Scheduler::Scheduler(Limits limits,std::vector<Profile> profiles):state_(std::make_shared<State>()){
 state_->limits=limits;
 std::vector<Device> devices;
 for(const auto& p:profiles)if(p.backend!=Backend::cpu){
  auto found=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.stable_id==p.stable_id;});
  if(found==devices.end())devices.push_back({p.stable_id,p.backend,p.driver_version,0,true,p.adapter_index,p.device_name,p.backend_index,p.backend_identity_verified,p.physical_id});
  else if(found->family!=p.backend||found->backend_index!=p.backend_index||found->physical_id!=p.physical_id||!p.backend_identity_verified)
   found->backend_identity_verified=false;
 }
 verify_device_identities(devices);
 for(auto p:profiles){
  if(p.backend!=Backend::cpu){
   auto d=std::find_if(devices.begin(),devices.end(),[&](const auto& current){return current.stable_id==p.stable_id&&current.backend_identity_verified;});
   if(d==devices.end()||!p.vram_mb)continue;
   // Only one simultaneous encode was measured on each card. H.264 and HEVC
   // are sequential within a job and share this physical session reservation.
   p.max_concurrency=std::min(p.max_concurrency,device_encode_sessions);
   state_->device_caps[card_key(p)]=std::max(state_->device_caps[card_key(p)],p.vram_mb);
  }
  state_->profiles.push_back(std::move(p));
 }
}
Scheduler::Reservation::Reservation(std::shared_ptr<State> state,std::vector<Profile> profiles)
 :state_(std::move(state)),profiles_(std::move(profiles)),active_(true){}
Scheduler::Reservation::Reservation(Reservation&& other) noexcept
 :state_(std::move(other.state_)),profiles_(std::move(other.profiles_)),active_(other.active_),cpu_fallback_keys_(std::move(other.cpu_fallback_keys_)){other.active_=false;}
Scheduler::Reservation& Scheduler::Reservation::operator=(Reservation&& other) noexcept{
 if(this!=&other){release();state_=std::move(other.state_);profiles_=std::move(other.profiles_);active_=other.active_;cpu_fallback_keys_=std::move(other.cpu_fallback_keys_);other.active_=false;}return *this;
}
Scheduler::Reservation::~Reservation(){release();}
void Scheduler::Reservation::release() noexcept{if(active_){Scheduler::release(state_,profiles_,cpu_fallback_keys_);active_=false;}}
void Scheduler::Reservation::finish_codec(Codec codec){
 if(!active_||!state_)return;
 auto remaining=profiles_;
 remaining.erase(std::remove_if(remaining.begin(),remaining.end(),[&](const auto& p){return p.codec==codec;}),remaining.end());
 if(remaining.size()==profiles_.size())return;
 if(remaining.empty()){release();profiles_.clear();cpu_fallback_keys_.clear();return;}
 const auto before=aggregate_cost(profiles_),after=aggregate_cost(remaining);
 std::lock_guard lock(state_->mutex);
 for(const auto& p:profiles_)if(p.codec==codec){
  const auto key=profile_key(p);
  const auto fallback=std::find(cpu_fallback_keys_.begin(),cpu_fallback_keys_.end(),key);
  if(fallback!=cpu_fallback_keys_.end()){
   state_->fallback_busy[key]=false;cpu_fallback_keys_.erase(fallback);
  }
  const bool has_gpu=std::any_of(profiles_.begin(),profiles_.end(),[&](const auto& q){return q.codec==codec&&q.backend!=Backend::cpu;});
  if(p.backend!=Backend::cpu||!has_gpu)--state_->primary_jobs[key];
 }
 for(const auto& [id,mb]:before.devices){
  const auto retained=after.devices.find(id);
  state_->device_vram[id]-=mb-(retained==after.devices.end()?0:retained->second);
  if(retained==after.devices.end())--state_->device_jobs[id];
 }
 state_->cpu-=before.cpu-after.cpu;state_->ram-=before.ram-after.ram;
 state_->scratch-=before.scratch-after.scratch;state_->vram-=before.vram-after.vram;
 state_->nvenc_jobs-=before.nvenc_devices.size()-after.nvenc_devices.size();
 profiles_=std::move(remaining);
}
bool Scheduler::Reservation::hardware_enabled(const Profile& profile) const{
 if(!active_||!state_||profile.backend==Backend::cpu)return false;
 std::lock_guard lock(state_->mutex);
 return std::any_of(state_->profiles.begin(),state_->profiles.end(),[&](const auto& p){
  return p.stable_id==profile.stable_id&&p.backend==profile.backend&&p.codec==profile.codec;
 });
}
std::uint64_t Scheduler::Reservation::memory_for_dimensions(std::uint32_t width,std::uint32_t height) const{
 std::uint64_t ram=0;
 for(const auto& p:profiles_)ram=std::max(ram,p.backend==Backend::cpu?p.ram_mb:
  std::max(p.ram_mb,gpu_child_memory_mb(width,height,p.codec,InputMemory::host,p.child_memory_overhead_mb)));
 return ram;
}
bool Scheduler::Reservation::resize_memory(std::uint32_t width,std::uint32_t height,const Limits& task){
 if(!active_||!state_)return false;
 const auto ram=memory_for_dimensions(width,height);
 std::lock_guard lock(state_->mutex);
 const auto old=aggregate_cost(profiles_).ram;
 if(ram>task.ram_mb||state_->ram>state_->limits.ram_mb||ram-old>state_->limits.ram_mb-state_->ram)return false;
 state_->ram+=ram-old;
 for(auto& p:profiles_)if(p.backend!=Backend::cpu)p.ram_mb=std::max(p.ram_mb,
  gpu_child_memory_mb(width,height,p.codec,InputMemory::host,p.child_memory_overhead_mb));
 return true;
}
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
void Scheduler::release(const std::shared_ptr<State>& state,const std::vector<Profile>& profiles,
                        const std::vector<std::string>& cpu_fallback_keys) noexcept{
 if(!state)return;
 std::lock_guard lock(state->mutex);
 for(const auto& key:cpu_fallback_keys)state->fallback_busy[key]=false;
 const auto cost=aggregate_cost(profiles);std::map<std::string,bool> released_devices;
 for(const auto& p:profiles){
  const bool has_gpu=std::any_of(profiles.begin(),profiles.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;});
  if(p.backend!=Backend::cpu&&!released_devices[card_key(p)]){--state->device_jobs[card_key(p)];released_devices[card_key(p)]=true;}
  if(p.backend!=Backend::cpu||!has_gpu)--state->primary_jobs[profile_key(p)];
 }
 state->nvenc_jobs-=cost.nvenc_devices.size();
 state->cpu-=cost.cpu;state->ram-=cost.ram;state->scratch-=cost.scratch;state->vram-=cost.vram;
 for(const auto& [id,mb]:cost.devices)state->device_vram[id]-=mb;
 --state->jobs;
}
Scheduler::Reservation Scheduler::reserve(const std::vector<Codec>& codecs){
 return reserve(codecs,state_->limits);
}
Scheduler::Reservation Scheduler::reserve(const std::vector<Codec>& codecs,const Limits& task_limits,std::vector<std::string>* exclusions){
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
 auto excluded=[&](const Profile& p,const char* reason){
  if(!exclusions||p.backend==Backend::cpu)return;
  const auto message="physical_id="+p.physical_id+" "+reason;
  if(std::find(exclusions->begin(),exclusions->end(),message)==exclusions->end())exclusions->push_back(message);
 };
 for(auto codec:codecs){
  std::vector<const Profile*> candidates;
  for(const auto& p:s->profiles){
   if(p.codec!=codec||!p.max_concurrency||p.stable_id.empty())continue;
   if(p.backend!=Backend::cpu&&!p.backend_identity_verified)continue;
   if(p.backend==Backend::cpu){if(s->primary_jobs[profile_key(p)]>=p.max_concurrency||s->fallback_busy[profile_key(p)])continue;}
   else if(s->primary_jobs[profile_key(p)]>=p.max_concurrency){excluded(p,"encoder session limit");continue;}
   candidates.push_back(&p);
  }
  std::sort(candidates.begin(),candidates.end(),[&](const auto* a,const auto* b){
   auto already_selected=[&](const auto* p){return p->backend!=Backend::cpu&&std::any_of(chosen.begin(),chosen.end(),[&](const auto& selected){return selected.backend!=Backend::cpu&&card_key(selected)==card_key(*p);});};
   if(already_selected(a)!=already_selected(b))return already_selected(a);
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
   if(projected.vram>task_caps.vram_mb||projected.vram+s->vram>s->limits.vram_mb){excluded(*candidate,"insufficient VRAM (task or machine budget)");continue;}
   if(projected.cpu>task_caps.cpu_threads||projected.ram>task_caps.ram_mb||projected.scratch>task_caps.scratch_mb||
      projected.cpu+s->cpu>s->limits.cpu_threads||projected.ram+s->ram>s->limits.ram_mb||projected.scratch+s->scratch>s->limits.scratch_mb){excluded(*candidate,"CPU/RAM/scratch budget exhausted");continue;}
   bool device_fit=true;
   for(const auto& [id,mb]:projected.devices){
    if(s->device_jobs[id]>=device_encode_sessions){excluded(*candidate,"per-card encoder session limit");device_fit=false;}
    if(mb+s->device_vram[id]>s->device_caps[id]){excluded(*candidate,"insufficient VRAM");device_fit=false;}
   }
   // Historical consumer drivers have a system-wide limit, not one limit
   // per card. Stay at two worker sessions even on newer drivers with more.
   if(!device_fit)continue;
   if(s->nvenc_jobs+projected.nvenc_devices.size()>worker_nvenc_sessions){excluded(*candidate,"worker encoder session limit");continue;}
   chosen.insert(chosen.end(),for_job.begin(),for_job.end());admitted=true;break;
  }
  if(!admitted)return {};
 }
 const auto cost=aggregate_cost(chosen);
 if(cost.cpu>task_caps.cpu_threads||cost.ram>task_caps.ram_mb||
    cost.scratch>task_caps.scratch_mb||cost.vram>task_caps.vram_mb)return {};
 if(cost.cpu+s->cpu>s->limits.cpu_threads||cost.ram+s->ram>s->limits.ram_mb||
    cost.scratch+s->scratch>s->limits.scratch_mb||cost.vram+s->vram>s->limits.vram_mb)return {};
 for(const auto& [id,mb]:cost.devices)if(mb+s->device_vram[id]>s->device_caps[id]||s->device_jobs[id]>=device_encode_sessions)return {};
 if(s->nvenc_jobs+cost.nvenc_devices.size()>worker_nvenc_sessions)return {};
 s->nvenc_jobs+=cost.nvenc_devices.size();
 s->cpu+=cost.cpu;s->ram+=cost.ram;s->scratch+=cost.scratch;s->vram+=cost.vram;++s->jobs;
 for(const auto& [id,mb]:cost.devices){s->device_vram[id]+=mb;++s->device_jobs[id];}
 for(const auto& p:chosen)if(p.backend!=Backend::cpu||std::count_if(chosen.begin(),chosen.end(),[&](const auto& q){return q.codec==p.codec&&q.backend!=Backend::cpu;})==0)++s->primary_jobs[profile_key(p)];
 return Reservation(s,std::move(chosen));
}
void Scheduler::set_device_available_vram(const std::string& id,std::uint64_t mb){
 auto s=state_;std::lock_guard lock(s->mutex);
 auto profile=std::find_if(s->profiles.begin(),s->profiles.end(),[&](const auto& p){return p.backend!=Backend::cpu&&p.stable_id==id;});
 if(profile==s->profiles.end())return;
 auto found=s->device_caps.find(card_key(*profile));
 if(found!=s->device_caps.end())found->second=mb;
}
std::uint64_t Scheduler::active_jobs() const{auto s=state_;std::lock_guard lock(s->mutex);return s->jobs;}
bool Scheduler::demote_profile(const Profile& profile){
 if(profile.backend==Backend::cpu)return false;
 auto s=state_;std::lock_guard lock(s->mutex);const auto before=s->profiles.size();
 s->profiles.erase(std::remove_if(s->profiles.begin(),s->profiles.end(),[&](const auto& p){
  return p.stable_id==profile.stable_id&&p.backend==profile.backend&&p.codec==profile.codec;
 }),s->profiles.end());
 // Active reservations retain their original cost and slots until drain.
 return before!=s->profiles.size();
}
void Scheduler::invalidate_hardware(){auto s=state_;std::lock_guard lock(s->mutex);s->profiles.erase(std::remove_if(s->profiles.begin(),s->profiles.end(),[](const auto& p){return p.backend!=Backend::cpu;}),s->profiles.end());}
void Scheduler::invalidate_all(){auto s=state_;std::lock_guard lock(s->mutex);s->profiles.clear();}
}
