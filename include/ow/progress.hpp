#pragma once
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
namespace ow {
struct ProgressSnapshot {
 std::string stage;std::uint64_t completed{},total{};std::optional<unsigned> percent;
 std::int64_t elapsed_ms{},quiet_ms{};bool quiet{},finished{},succeeded{};
};
// Measured counters and monotonic time are separate. Waiting cannot advance work.
class ProgressCounter {
 mutable std::mutex mutex_;std::string stage_;std::uint64_t completed_{},total_{};
 std::int64_t started_{},activity_{},finished_at_{};bool finished_{},succeeded_{};
public:
 void begin(std::string stage,std::int64_t now,std::uint64_t total=0){
  std::lock_guard lock(mutex_);stage_=std::move(stage);started_=activity_=now;completed_=0;total_=total;finished_=succeeded_=false;
 }
 void update(std::uint64_t completed,std::int64_t now){
  std::lock_guard lock(mutex_);if(!finished_&&completed>completed_){completed_=completed;activity_=std::max(activity_,now);}
 }
 void finish(bool succeeded,std::int64_t now){std::lock_guard lock(mutex_);if(finished_)return;finished_=true;succeeded_=succeeded;finished_at_=now;}
 ProgressSnapshot snapshot(std::int64_t now) const {
  std::lock_guard lock(mutex_);if(finished_)now=finished_at_;
  ProgressSnapshot s;s.stage=stage_;s.completed=completed_;s.total=total_;s.finished=finished_;s.succeeded=succeeded_;
  s.elapsed_ms=std::max<std::int64_t>(0,now-started_);s.quiet_ms=std::max<std::int64_t>(0,now-activity_);s.quiet=!finished_&&s.quiet_ms>=30000;
  if(total_)s.percent=static_cast<unsigned>(100.0L*std::min(completed_,total_)/total_);
  return s;
 }
};
}
