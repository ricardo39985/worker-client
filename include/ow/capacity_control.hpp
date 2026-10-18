#pragma once
#include "ow/admission.hpp"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
namespace ow {
struct Pressure {std::optional<double> cpu_percent,disk_percent;bool foreground{};std::optional<double> ram_percent{0.0};};
// Feedback is a sampled utilization target, NOT a GPU/disk OS quota. Atomic
// resource accounting and independently proven encoder limits remain mandatory.
// Trial concurrency grows only after sustained quiet measurements + validated
// work. Pressure/foreground backoff never frees or kills an active reservation.
class CapacityControl {
 mutable std::mutex mutex_;
 unsigned ceiling_,limit_{1},qualified_{};
 Tick window_ms_,since_{},observed_{},retry_after_{};
 unsigned completed_{},observed_active_{};bool started_{},healthy_{};
 std::string reason_{"telemetry_unavailable"},resource_{"cpu"};
 bool fresh(Tick now) const{return started_&&now>=observed_&&now-observed_<=5000;}
public:
 explicit CapacityControl(unsigned ceiling,Tick window_ms=10000,std::string resource="cpu"):ceiling_(std::clamp(ceiling,1u,64u)),window_ms_(std::max<Tick>(1,window_ms)),resource_(resource){}
 void ceiling(unsigned value){std::lock_guard lock(mutex_);ceiling_=std::clamp(value,1u,64u);limit_=std::min(limit_,ceiling_);qualified_=std::min(qualified_,ceiling_);}
 void activity(unsigned count){std::lock_guard lock(mutex_);if(count>=limit_&&observed_active_<limit_)completed_=0;observed_active_=std::max(observed_active_,count);}
 void validated(){std::lock_guard lock(mutex_);++completed_;}
 void observe(const Pressure& p,Tick now){
  std::lock_guard lock(mutex_);
  auto valid=[](auto v){return v&&std::isfinite(*v)&&*v>=0&&*v<=100;};
  const bool values=valid(p.cpu_percent)&&valid(p.disk_percent)&&valid(p.ram_percent);
  if(!started_||now<observed_){since_=now;completed_=0;}
  started_=true;observed_=now;
  reason_=!values?"telemetry_unavailable":p.foreground?"foreground_activity":
          *p.cpu_percent>=90?resource_+"_pressure":*p.ram_percent>=90?"ram_pressure":*p.disk_percent>=90?"disk_pressure":"tracking_90_percent_target";
  healthy_=values&&!p.foreground&&*p.cpu_percent<90&&*p.ram_percent<90&&*p.disk_percent<90;
  if(!healthy_){
   if(!values||p.foreground){limit_=1;if(!values)qualified_=0;}
   else{limit_=std::max(1u,qualified_&&limit_>qualified_?qualified_:limit_/2);qualified_=std::min(qualified_,limit_);retry_after_=now+60000;}
   since_=now;completed_=0;observed_active_=0;return;
  }
  if(now<retry_after_)reason_="cooldown_after_pressure";
  if(now-since_>=window_ms_&&completed_>=2&&observed_active_>=limit_){qualified_=limit_;if(now>=retry_after_)limit_=std::min(ceiling_,limit_*2);since_=now;completed_=0;observed_active_=0;}
 }
 Decision admit(unsigned active,Tick now) const{
  std::lock_guard lock(mutex_);const auto limit=fresh(now)?limit_:1;
  return {active<limit,active<limit?"capacity_ready":fresh(now)?reason_:"stale_telemetry"};
 }
 Decision admit_offer(Tick now) const{
  std::lock_guard lock(mutex_);return {fresh(now)&&healthy_,fresh(now)?reason_:"stale_telemetry"};
 }
 unsigned stage_limit(Tick now) const{std::lock_guard lock(mutex_);return fresh(now)?limit_:1;}
 unsigned limit() const{std::lock_guard lock(mutex_);return limit_;}
 unsigned qualified() const{std::lock_guard lock(mutex_);return qualified_;}
 std::string reason() const{std::lock_guard lock(mutex_);return reason_;}
};
}
