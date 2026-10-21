#include "ow/lifecycle.hpp"
#include <stdexcept>
namespace ow {
bool unattended_ready(bool connected,std::optional<bool> processing,bool headless,bool identity,bool expected_owner,bool non_elevated){return connected&&processing.value_or(false)&&headless&&identity&&expected_owner&&non_elevated;}
bool restart_quiescent(std::size_t running,std::size_t admitted,std::uint64_t outbox){return running==0&&admitted==0&&outbox==0;}
LifecycleStatus run_lifecycle(LifecycleHost& h,bool restart,Tick timeout){
 if(timeout<1||timeout>1800000)throw std::runtime_error("Lifecycle wait must be bounded to 1..1800000 milliseconds");
 const auto began=h.now();const auto expired=[&]{return h.now()-began>=timeout;};
 auto current=h.observe();
 if(current&&(restart||current->exiting)){
  const auto old=*current;if(!old.exiting)h.request_restart(old);
  while(h.alive(old)){if(expired())throw std::runtime_error("Drain pending; no process was cancelled or replaced");h.wait();}
  current.reset();
 }
 bool activated=false;
 for(;;){
  if(current&&current->ready&&!current->exiting)return *current;
  if(expired())throw std::runtime_error("Readiness pending; task submission is not readiness");
  if(!current&&!activated){h.start_enrolled();activated=true;}
  h.wait();current=h.observe();
 }
}
void validate_launch_task(const LaunchTask& a,const LaunchTask& e){
 if(e.sid.empty()||e.sid=="S-1-5-18"||e.sid=="S-1-5-19"||e.sid=="S-1-5-20"||e.logon!=TaskLogon::password||!e.limited||!e.boot_trigger||!e.demand_start||!e.ignore_new||e.execution_limit_seconds||e.actions.size()!=1||e.actions[0].executable.empty()||e.actions[0].arguments.empty()||e.actions[0].directory.empty())throw std::runtime_error("Unsafe expected owner launch policy");
 if(a.sid!=e.sid||a.logon!=TaskLogon::password||!a.limited||!a.boot_trigger||!a.demand_start||!a.ignore_new||a.execution_limit_seconds||a.actions!=e.actions)throw std::runtime_error("Unattended task is not the exact enrolled LIMITED owner password task");
}
}
