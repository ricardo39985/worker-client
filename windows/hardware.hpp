#pragma once
#include "ow/adaptive_hardware.hpp"
#include <vector>
#include <cstdint>
#include <optional>
namespace ow::win {
std::vector<adaptive::Device> enumerate_video_devices(bool resolve_encoder_identity=true);
struct GpuMemorySample { std::uint64_t child_bytes{},adapter_bytes{}; };
// Returns no sample when the OS counter provider is unavailable or ambiguous.
std::optional<GpuMemorySample> sample_gpu_memory(std::uint32_t process_id,const adaptive::Device& device);
}
