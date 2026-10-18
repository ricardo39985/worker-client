#pragma once
#include "ow/operator_status.hpp"
#include "platform.hpp"
#include "ow/protocol.hpp"
#include "ow/stage_timing.hpp"
#include <optional>
namespace ow::win {
std::optional<std::uint64_t> thread_cpu_us() noexcept;

class Child {
 Handle job_,process_;
 Resources resources_{};
 std::uint64_t launch_us_{};
 std::optional<ChildTiming> timing_;
 std::optional<ChildTiming> sample_timing() const noexcept;
public:
 Child(const std::filesystem::path& exe,const std::vector<std::wstring>& args,
       const std::filesystem::path& workspace,Resources resources,Priority priority);
 ~Child();Child(const Child&)=delete;Child& operator=(const Child&)=delete;
 std::optional<DWORD> exit_code();
 DWORD process_id() const;
 std::optional<ChildTiming> timing() const noexcept;
 std::uint64_t peak_commit_mb() const;
 void stop();void priority(Priority);
};
// Declared after Child and inside its stage: account even on cancellation or
// nonzero exit, and sample only after descendants have drained.
class ChildTimingCapture {
 Child& child_;StageTimer* stage_;
public:
 ChildTimingCapture(Child& child,StageTimer* stage):child_(child),stage_(stage){}
 ~ChildTimingCapture(){if(stage_){child_.stop();stage_->child(child_.timing());}}
};
}
