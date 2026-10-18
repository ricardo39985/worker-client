#pragma once
#include "ow/adaptive_hardware.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace ow {
// An NVENC allocation error does not distinguish session pressure, driver
// state and actual memory pressure. Never label it permanent incompatibility.
inline std::string probe_failure_reason(const std::string& error){
 if(error.find("out of memory")!=std::string::npos||error.find("Cannot allocate memory")!=std::string::npos||
    error.find("No free encode sessions")!=std::string::npos||error.find("too many concurrent sessions")!=std::string::npos||
    error.find("ENCODER_BUSY")!=std::string::npos||error.find("temporarily unavailable")!=std::string::npos)
  return "memory/session unavailable";
 return "probe failed";
}
// Each attempt owns and drains its contained children before it throws.
// Three total attempts bound immediate work; a later idle epoch can try again.
template<class Attempt,class Cancelled,class Wait>
void retry_probe(Attempt attempt,Cancelled cancelled,Wait wait){
 for(unsigned i=0;i<3;++i){
  if(cancelled())throw std::runtime_error("Capability probe cancelled");
  try{attempt();return;}catch(const std::exception&){if(cancelled()||i==2)throw;}
  wait(1000u<<i);
 }
}
struct RetainedProbeEvidence {std::vector<adaptive::ProbeEvidence> probes;std::vector<adaptive::TuningEvidence> tuning;};
// Only the running controller's successful samples may enter here, never a
// saved diagnostic file. Failed/removed profiles cannot manufacture evidence.
inline RetainedProbeEvidence retain_current_profiles(const std::vector<adaptive::Device>& devices,
 const std::vector<adaptive::Profile>& profiles,const std::string& hash){
 RetainedProbeEvidence out;
 for(const auto& p:profiles){
  if(std::none_of(devices.begin(),devices.end(),[&](const auto& d){return adaptive::profile_current(p,d,hash);}))continue;
  out.probes.push_back({p.stable_id,p.driver_version,p.ffmpeg_sha256,p.backend,p.codec,true,true,true});
  out.tuning.push_back({p.stable_id,p.backend,p.codec,p.measured_frames_per_second,p.cpu_threads,p.ram_mb,p.vram_mb,p.scratch_mb,
   true,true,true,p.max_concurrency,p.measured_width,p.measured_height});
 }
 return out;
}
struct ProbeFailure { std::string card,codec,reason,device_id; };
// Monotonic, process-local recovery policy. A failed epoch is never final;
// backoff caps at five minutes. Scheduling still requires fresh full proof.
class ProbeRecovery {
 std::vector<ProbeFailure> failures_;
 std::int64_t next_{};
 unsigned epochs_{};
public:
 void failed(std::vector<ProbeFailure> failures,std::int64_t now){
  failures_=std::move(failures);
  if(failures_.empty()){epochs_=0;next_=0;return;}
  const auto delay=std::min<std::int64_t>(300000,30000LL<<std::min(epochs_,4u));
  epochs_=std::min(epochs_+1,5u);next_=now+delay;
 }
 const std::vector<ProbeFailure>& pending() const {return failures_;}
 bool due(std::int64_t now) const {return !failures_.empty()&&now>=next_;}
 std::vector<std::string> status(std::int64_t now) const {
  std::vector<std::string> out;
  for(const auto& f:failures_)out.push_back(f.card+" "+f.codec+" withheld: "+probe_failure_reason(f.reason)+
   "; retry "+(now>=next_?std::string("when idle"):"in "+std::to_string((next_-now+999)/1000)+"s"));
  return out;
 }
};
}
