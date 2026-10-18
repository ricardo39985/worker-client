#include "test.hpp"
#include "ow/stage_timing.hpp"
#include <future>
using namespace ow;
TEST("timing separates wall controller CPU child CPU launch and process lifetime") {
 std::uint64_t wall=100,cpu=10;std::vector<StageSample> records;
 {
  StageTimer stage("transcode","feed.mp4",[&](const auto& s){records.push_back(s);},
   [&]{return wall;},[&]{return std::optional<std::uint64_t>(cpu);});
  stage.child(ChildTiming{300,20,700});wall=1100;cpu=50;
 }
 CHECK(records.size()==1);const auto& r=records.front();
 CHECK(r.wall_us==1000);CHECK(r.controller_cpu_us==40);CHECK(r.child->cpu_us==300);
 CHECK(r.child->launch_us==20);CHECK(r.child->root_lifetime_us==700);CHECK(r.completed);
 CHECK(stage_record(r).find("child_cpu_us=300")!=std::string::npos);
 CHECK(stage_record(r).find("root_lifetime_us=700")!=std::string::npos);
}
TEST("failed timed work retains its exception and reports incomplete work") {
 std::vector<StageSample> records;std::string error;
 try{StageTimer stage("validation","optimized.mp4",[&](const auto& s){records.push_back(s);});
  throw std::runtime_error("original failure");
 }catch(const std::exception& e){error=e.what();}
 CHECK(error=="original failure");CHECK(records.size()==1);CHECK(!records.front().completed);
 CHECK(stage_record(records.front()).find("controller_cpu_us=unavailable")!=std::string::npos);
}
TEST("missing child accounting never becomes a measured zero") {
 StageSample record;record.stage="validation";record.child_expected=true;
 auto line=stage_record(record);CHECK(line.find("child_cpu_us=unavailable")!=std::string::npos);
 record.child_expected=false;CHECK(stage_record(record).find("child_cpu_us=0")!=std::string::npos);
}
TEST("timing observer failure cannot fail or replace conversion outcome") {
 {StageTimer stage("poster","poster.webp",[](const auto&){throw std::runtime_error("observer failed");});}
 std::string error;
 try{StageTimer stage("transcode","feed.mp4",[](const auto&){throw std::runtime_error("observer failed");});
  throw std::runtime_error("conversion failed");
 }catch(const std::exception& e){error=e.what();}
 CHECK(error=="conversion failed");
}
TEST("concurrent stage sums are explicitly distinct from job wall time") {
 StageTotals totals;
 auto add=[&](const char* name){StageSample r;r.stage=name;r.wall_us=1000;r.controller_cpu_us=10;
  r.child=ChildTiming{100,20,800};r.child_expected=true;r.completed=true;totals.add(r);};
 auto transfer=std::async(std::launch::async,[&]{add("upload");});add("transcode");transfer.get();
 auto line=totals.record(1500);
 CHECK(line.find("job_wall_us=1500")!=std::string::npos);
 CHECK(line.find("stage_wall_sum_us=2000")!=std::string::npos);
 CHECK(line.find("cpu_us=220")!=std::string::npos);
 CHECK(line.find("stages_may_overlap=true")!=std::string::npos);
}
TEST("one unavailable CPU measurement makes aggregate CPU unavailable") {
 StageTotals totals;StageSample a;a.controller_cpu_us=5;totals.add(a);
 StageSample b;b.child_expected=true;totals.add(b);
 CHECK(totals.record(100).find("cpu_us=unavailable")!=std::string::npos);
}
TEST("job CPU includes main thread once plus background CPU and all child trees") {
 StageTotals totals;StageSample encode;encode.controller_cpu_us=40;encode.child=ChildTiming{300,20,700};
 encode.child_expected=true;totals.add(encode);
 StageSample upload;upload.controller_cpu_us=20;totals.add(upload,true);
 CHECK(totals.record(1000,100).find("job_cpu_us=420")!=std::string::npos);
 CHECK(totals.record(1000).find("job_cpu_us=unavailable")!=std::string::npos);
}
