#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include "ow/progress.hpp"

namespace ow {
// Probe work owns phase updates; a separate observer polls this bounded record.
// It never times, cancels or chooses probe work. The clock is injectable for contracts.
class ProbeStatus {
 std::function<std::int64_t()> clock_;
 std::mutex mutex_;
 std::int64_t started_{},last_{};
 std::string phase_;
 bool active_{},written_{};
public:
 explicit ProbeStatus(std::function<std::int64_t()> clock=[] {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
 }):clock_(std::move(clock)),started_(clock_()){}
 void phase(std::string name,unsigned current=0,unsigned total=0){
  std::lock_guard lock(mutex_);if(!active_&&!written_)started_=clock_();active_=true;phase_=std::move(name);
  if(total&&current&&current<=total)phase_+=" "+std::to_string(current)+"/"+std::to_string(total)+", "+std::to_string(total-current)+" left";
 }
 std::optional<std::string> line(){
  std::lock_guard lock(mutex_);const auto now=clock_();
  if(!active_||(written_&&now-last_<5000))return {};
  last_=now;written_=true;
  return "PROBE PROGRESS | "+phase_+" | elapsed "+format_duration_compact(std::max<std::int64_t>(0,now-started_));
 }
 void finish(){std::lock_guard lock(mutex_);active_=false;}
};
}
