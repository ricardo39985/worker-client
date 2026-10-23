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
// One text-only contained process tree, owned by the controller, never by a lease.
class GemmaQuery {
 GemmaRuntime runtime_;std::filesystem::path workspace_;Resources limits_;Priority priority_;
 std::unique_ptr<Child> child_;std::mutex mutex_;
 void start(const std::function<bool()>&);
 void stop() noexcept;
public:
 static constexpr std::uint64_t memory_mb=2048;
 GemmaQuery(const GemmaRuntime&,const std::filesystem::path&,Resources,Priority,const std::function<bool()>&);
 ~GemmaQuery();
 void encode(const std::filesystem::path& attempt,const std::function<bool()>& cancelled,Priority);
};
struct GemmaExecution {std::uint64_t peak_commit_mb{};Tick elapsed_ms{};};
GemmaExecution run_gemma(const GemmaRuntime&,const std::filesystem::path& ffmpeg,const std::filesystem::path& workspace,
 const std::string& modality,const std::function<bool()>& cancelled,Resources resources,Priority priority,bool probe=false,bool query=false);
}
