#pragma once
#include "ow/admission.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace ow {
struct LifecycleStatus {std::uint32_t pid{};bool ready{},exiting{};};
struct LifecycleHost {
 virtual ~LifecycleHost()=default;
 virtual std::optional<LifecycleStatus> observe()=0;
 virtual void request_restart(const LifecycleStatus&)=0;
 virtual void start_enrolled()=0;
 virtual bool alive(const LifecycleStatus&)=0;
 virtual Tick now()=0;
 virtual void wait()=0;
};
LifecycleStatus run_lifecycle(LifecycleHost&,bool restart,Tick timeout);
bool restart_quiescent(std::size_t running,std::size_t admitted,std::uint64_t outbox);
enum class TaskLogon {password,s4u,interactive,service};
struct LaunchAction {std::string executable,arguments,directory;bool operator==(const LaunchAction&)const=default;};
struct LaunchTask {std::string sid;TaskLogon logon{};bool limited{},boot_trigger{},demand_start{},ignore_new{};unsigned execution_limit_seconds{};std::vector<LaunchAction> actions;};
void validate_launch_task(const LaunchTask& actual,const LaunchTask& expected);
}
