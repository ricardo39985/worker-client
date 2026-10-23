#pragma once
#include "process.hpp"
#include "ow/gemma.hpp"
namespace ow::win {
struct GemmaRuntime {
 std::filesystem::path python,script,root,ffprobe;
 std::string python_hash,script_hash,ffprobe_hash;
 bool advertise{};
};
std::optional<GemmaRuntime> load_gemma(const std::filesystem::path& owner_root);
struct GemmaExecution {std::uint64_t peak_commit_mb{};Tick elapsed_ms{};};
GemmaExecution run_gemma(const GemmaRuntime&,const std::filesystem::path& ffmpeg,const std::filesystem::path& workspace,
 const std::string& modality,const std::function<bool()>& cancelled,Resources resources,Priority priority,bool probe=false,bool query=false);
}
