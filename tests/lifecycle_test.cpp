#include "test.hpp"
#include "ow/lifecycle.hpp"
using namespace ow;
namespace {
struct Host:LifecycleHost {
 Tick clock{};int starts{},restarts{};bool exists{},ready{},exiting{};std::uint32_t pid{42};Tick gone_at{},ready_at{};
 std::optional<LifecycleStatus> observe() override {if(exists&&gone_at&&clock>=gone_at)exists=false;if(exists&&clock>=ready_at)ready=true;if(!exists)return {};return LifecycleStatus{pid,ready,exiting};}
 void request_restart(const LifecycleStatus&) override {++restarts;exiting=true;gone_at=clock+50;}
 void start_enrolled() override {++starts;exists=true;pid=43;ready=false;ready_at=clock+40;gone_at=0;exiting=false;}
 bool alive(const LifecycleStatus& s) override {return exists&&s.pid==pid&&!(gone_at&&clock>=gone_at);}
 Tick now() override{return clock;}void wait() override{clock+=10;}
};
}
TEST("start is idempotent and waits genuine readiness not task submission") {
 Host h;auto s=run_lifecycle(h,false,200);CHECK(h.starts==1);CHECK(s.pid==43);CHECK(s.ready);CHECK(h.clock>=40);
 auto again=run_lifecycle(h,false,200);CHECK(h.starts==1);CHECK(again.pid==43);
}
TEST("restart waits old process termination before one task activation and new readiness") {
 Host h;h.exists=true;h.ready=true;auto s=run_lifecycle(h,true,200);CHECK(h.restarts==1);CHECK(h.starts==1);CHECK(s.pid==43);CHECK(s.ready);CHECK(h.clock>=90);
}
TEST("restart timeout does not cancel work force stop or start another engine") {
 struct Busy:Host {void request_restart(const LifecycleStatus&)override{++restarts;exiting=true;gone_at=10000;}} h;
 h.exists=true;h.ready=true;THROWS(run_lifecycle(h,true,100));CHECK(h.restarts==1);CHECK(h.starts==0);CHECK(h.exists);
}
TEST("retry resumes an already requested drain instead of toggling operator intent") {
 Host h;h.exists=true;h.ready=true;h.exiting=true;h.gone_at=50;auto s=run_lifecycle(h,true,200);CHECK(h.restarts==0);CHECK(h.starts==1);CHECK(s.pid==43);
}
TEST("restart exit requires reservations children and durable matching ACK to settle") {
 CHECK(!restart_quiescent(1,1,0));CHECK(!restart_quiescent(0,1,0));CHECK(!restart_quiescent(0,0,1));CHECK(restart_quiescent(0,0,0));
}
TEST("unattended activation requires exact limited owner password-backed fixed boot task") {
 LaunchTask wanted{"S-1-5-21-1",TaskLogon::password,true,true,true,true,0,{{"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe","-NoLogo -NoProfile -NonInteractive -File C:\\owner\\start-headless.ps1","C:\\owner"}}};
 validate_launch_task(wanted,wanted);
 for(auto mode:{TaskLogon::s4u,TaskLogon::interactive,TaskLogon::service}){auto bad=wanted;bad.logon=mode;THROWS(validate_launch_task(bad,wanted));}
 auto bad=wanted;bad.limited=false;THROWS(validate_launch_task(bad,wanted));bad=wanted;bad.sid="S-1-5-19";THROWS(validate_launch_task(bad,wanted));
 bad=wanted;bad.actions[0].arguments+=" -Command malicious";THROWS(validate_launch_task(bad,wanted));bad=wanted;bad.actions.push_back(bad.actions[0]);THROWS(validate_launch_task(bad,wanted));
 bad=wanted;bad.boot_trigger=false;THROWS(validate_launch_task(bad,wanted));bad=wanted;bad.execution_limit_seconds=600;THROWS(validate_launch_task(bad,wanted));
}
