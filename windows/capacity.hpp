#pragma once
#include "platform.hpp"
#include "ow/operator_status.hpp"
#include "ow/capacity_control.hpp"
#include <map>
namespace ow::win {
struct RuntimePressure {Pressure host;std::map<std::string,double> gpu_busy;};
// Cross-session foreground input uses WTS, not the SSH service's input queue.
RuntimePressure sample_runtime_pressure(const std::filesystem::path& root);
Priority desktop_priority(Priority requested);
void background_gpu_process(HANDLE process) noexcept;
}
