#pragma once
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
namespace ow {
inline std::uint64_t timing_wall_us(){
 return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
  std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct ChildTiming {
 // CPU includes every contained descendant, sampled after drain. Launch covers
 // CreateProcess/containment/resume, NOT FFmpeg's internal codec startup.
 std::uint64_t cpu_us{},launch_us{};
 std::optional<std::uint64_t> root_lifetime_us;
};
struct StageSample {
 std::string stage,artifact;
 std::uint64_t wall_us{};
 std::optional<std::uint64_t> controller_cpu_us;
 std::optional<ChildTiming> child;
 bool child_expected{},completed{};
};
inline std::string timing_value(std::optional<std::uint64_t> n){return n?std::to_string(*n):"unavailable";}
inline std::string stage_record(const StageSample& s){
 return "TIMING stage="+s.stage+" artifact="+(s.artifact.empty()?"none":s.artifact)+
  " wall_us="+std::to_string(s.wall_us)+" controller_cpu_us="+timing_value(s.controller_cpu_us)+
  " child_cpu_us="+(s.child?std::to_string(s.child->cpu_us):s.child_expected?"unavailable":"0")+
  " launch_us="+(s.child?std::to_string(s.child->launch_us):s.child_expected?"unavailable":"0")+
  " root_lifetime_us="+(s.child?timing_value(s.child->root_lifetime_us):s.child_expected?"unavailable":"0")+
  " completed="+(s.completed?"true":"false");
}
class StageTimer {
public:
 using WallClock=std::function<std::uint64_t()>;
 using CpuClock=std::function<std::optional<std::uint64_t>()>;
 using Sink=std::function<void(const StageSample&)>;
private:
 StageSample sample_;Sink sink_;WallClock wall_;CpuClock cpu_;
 std::uint64_t start_;std::optional<std::uint64_t> cpu_start_;int exceptions_;
public:
 StageTimer(std::string stage,std::string artifact,Sink sink,WallClock wall=timing_wall_us,
            CpuClock cpu=[]()->std::optional<std::uint64_t>{return {};},bool child_expected=false)
  :sink_(std::move(sink)),wall_(std::move(wall)),cpu_(std::move(cpu)),start_(wall_()),
   cpu_start_(cpu_()),exceptions_(std::uncaught_exceptions()){
  sample_.stage=std::move(stage);sample_.artifact=std::move(artifact);sample_.child_expected=child_expected;
 }
 StageTimer(const StageTimer&)=delete;StageTimer& operator=(const StageTimer&)=delete;
 void child(std::optional<ChildTiming> value) noexcept {sample_.child_expected=true;sample_.child=std::move(value);}
 ~StageTimer() noexcept {
  try{
   sample_.wall_us=wall_()-start_;const auto end=cpu_();
   if(cpu_start_&&end&&*end>=*cpu_start_)sample_.controller_cpu_us=*end-*cpu_start_;
   sample_.completed=std::uncaught_exceptions()==exceptions_;sink_(sample_);
  }catch(...){/* Observability must never replace the job's result. */}
 }
};
class StageTotals {
 mutable std::mutex mutex_;
 std::uint64_t wall_{},cpu_{},count_{};bool cpu_known_{true};
 std::uint64_t child_cpu_{},background_cpu_{};bool job_cpu_known_{true};
public:
 void add(const StageSample& s,bool background=false){
  std::lock_guard lock(mutex_);++count_;wall_+=s.wall_us;
  if(!s.controller_cpu_us||(s.child_expected&&!s.child))cpu_known_=false;
  if(s.controller_cpu_us)cpu_+=*s.controller_cpu_us;
  if(s.child)cpu_+=s.child->cpu_us;
  if(s.child_expected&&!s.child)job_cpu_known_=false;
  if(s.child)child_cpu_+=s.child->cpu_us;
  if(background){if(s.controller_cpu_us)background_cpu_+=*s.controller_cpu_us;else job_cpu_known_=false;}
 }
 std::string record(std::uint64_t job_wall_us,std::optional<std::uint64_t> main_thread_cpu_us={}) const {
  std::lock_guard lock(mutex_);
  return "TIMING total job_wall_us="+std::to_string(job_wall_us)+" stage_wall_sum_us="+std::to_string(wall_)+
   " cpu_us="+(cpu_known_?std::to_string(cpu_):"unavailable")+" cpu_scope=timed_stages_and_child_trees stages="+
   std::to_string(count_)+" stages_may_overlap=true job_cpu_us="+
   (main_thread_cpu_us&&job_cpu_known_?std::to_string(*main_thread_cpu_us+child_cpu_+background_cpu_):"unavailable")+
   " job_cpu_scope=main_thread_background_stages_and_child_trees";
 }
};
}
