#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <map>
#include <system_error>
#include <vector>

namespace ow::adaptive {
enum class Backend { cpu, nvidia_nvenc, intel_qsv, amd_amf };
enum class Codec { h264, hevc };
struct Device {
    std::string stable_id;
    Backend family{Backend::cpu};
    std::string driver_version;
    std::uint64_t dedicated_vram_mb{};
    bool driver_healthy{};
    std::uint32_t adapter_index{};
    std::string name;
    std::uint32_t backend_index{};
    bool backend_identity_verified{};
};
struct ProbeEvidence {
    std::string stable_id, driver_version, ffmpeg_sha256;
    Backend backend{Backend::cpu};
    Codec codec{Codec::h264};
    bool encoded{}, decoded{}, output_valid{};
};
struct TuningEvidence {
    std::string stable_id;
    Backend backend{Backend::cpu};
    Codec codec{Codec::h264};
    double frames_per_second{};
    std::uint64_t cpu_threads{}, ram_mb{}, vram_mb{}, scratch_mb{};
    bool completed{}, output_valid{}, cancelled_cleanly{};
    std::uint32_t tested_concurrency{1};
    std::uint32_t max_width{},max_height{};
};
struct Limits {
    std::uint64_t cpu_threads{}, ram_mb{}, vram_mb{}, scratch_mb{};
    std::uint32_t max_jobs{};
};
struct GpuMemoryRecord { std::uint32_t process_id{},node{};std::uint64_t bytes{};std::string stable_id;bool valid{true}; };
struct GpuMemoryTotals { std::uint64_t child_bytes{},adapter_bytes{}; };
std::optional<GpuMemoryTotals> gpu_memory_totals(const std::vector<GpuMemoryRecord>& records,
                                                 std::uint32_t process_id,const std::string& stable_id);
// Missing telemetry budgets half declared VRAM, capped at 1024 MiB and the
// operator ceiling. This bounds reservations, not unobserved external GPU usage.
struct VramBudget { std::uint64_t mb{};bool estimated{}; };
VramBudget vram_budget(std::uint64_t declared_mb,std::optional<std::uint64_t> adapter_bytes,std::uint64_t global_mb);
bool vram_envelope_ok(std::uint64_t declared_mb,std::uint64_t reserved_mb,std::optional<GpuMemoryTotals> sample);
struct BackendIdentity { std::string stable_id;std::uint32_t ordinal{},node_mask{}; };
std::optional<std::uint32_t> resolve_unique_backend_ordinal(const std::vector<BackendIdentity>& mappings,
                                                            const std::string& stable_id);
struct Profile {
    std::string stable_id, driver_version, ffmpeg_sha256;
    Backend backend{Backend::cpu};
    Codec codec{Codec::h264};
    std::uint64_t cpu_threads{}, ram_mb{}, vram_mb{}, scratch_mb{};
    std::uint32_t max_concurrency{};
    double measured_frames_per_second{};
    std::uint32_t adapter_index{};
    std::uint32_t backend_index{};
    bool backend_identity_verified{};
    std::uint32_t max_width{},max_height{};
};
struct TuningComparison { double serial_fps{},parallel_fps{};bool comparable{}; };
TuningComparison compare_tuning_runs(const std::vector<std::uint64_t>& serial_stage_ms,
                                     const std::vector<std::uint64_t>& parallel_stage_ms,
                                     std::uint32_t serial_repeats,std::uint32_t parallel_repeats,
                                     std::uint64_t serial_total_frames,std::uint64_t parallel_total_frames,
                                     std::uint64_t serial_child_cpu,std::uint64_t parallel_child_cpu,
                                     std::uint64_t serial_child_ram,std::uint64_t parallel_child_ram,
                                     std::uint64_t serial_child_scratch,std::uint64_t parallel_child_scratch);

// Only profiles with a matching successful encode, decode, output validation,
// and bounded tuning run are returned. Limits are hard ceilings.
std::vector<Profile> choose_profiles(const std::vector<Device>& devices,
                                     const std::vector<ProbeEvidence>& probes,
                                     const std::vector<TuningEvidence>& tuning,
                                     const Limits& limits,
                                     const std::string& ffmpeg_sha256);
bool profile_current(const Profile& profile, const Device& device,
                     const std::string& ffmpeg_sha256);
bool profile_supports_dimensions(const Profile& profile,std::uint32_t width,std::uint32_t height);

// Owns adaptive per-job budgets until the selected pipeline has drained.
// Reservations account CPU, RAM, disk, global VRAM, per-device VRAM and jobs
// as one atomic decision. The lease is move-only and releases on destruction.
class Scheduler {
    struct State;
    std::shared_ptr<State> state_;
public:
    class Reservation {
        std::shared_ptr<State> state_;
        std::vector<Profile> profiles_;
        bool active_{};
        std::vector<std::string> cpu_fallback_keys_;
        Reservation(std::shared_ptr<State>, std::vector<Profile>);
        friend class Scheduler;
    public:
        Reservation() = default;
        Reservation(const Reservation&) = delete;
        Reservation& operator=(const Reservation&) = delete;
        Reservation(Reservation&&) noexcept;
        Reservation& operator=(Reservation&&) noexcept;
        ~Reservation();
        const std::vector<Profile>& profiles() const { return profiles_; }
        explicit operator bool() const { return active_; }
        void release() noexcept;
        bool acquire_cpu_fallback(const std::function<bool()>& cancelled,
                                  std::uint32_t max_wait_ms=30000);
    };
    Scheduler(Limits limits, std::vector<Profile> profiles);
    Reservation reserve(const std::vector<Codec>& codecs);
    Reservation reserve(const std::vector<Codec>& codecs, const Limits& task_limits);
    void set_device_available_vram(const std::string& stable_id,std::uint64_t mb);
    void invalidate_hardware();
    void invalidate_all();
    std::uint64_t active_jobs() const;
private:
    static void release(const std::shared_ptr<State>&, const std::vector<Profile>&) noexcept;
};
template<class Cancelled,class Encode,class Validate>
bool run_with_cpu_fallback(const std::filesystem::path& output,bool try_hardware,
                           Cancelled cancelled,Encode encode,Validate validate){
 auto discard=[&]{std::error_code error;std::filesystem::remove(output,error);};
 discard();
 if(cancelled())throw std::runtime_error("Adaptive encode cancelled before helper start");
 if(try_hardware){
  try{
   encode(true);
   if(std::filesystem::is_regular_file(output)&&std::filesystem::file_size(output)>0&&validate(output)){
    if(cancelled())throw std::runtime_error("Adaptive encode cancelled during output validation");
    return true;
   }
  }catch(...){discard();if(cancelled())throw;}
  discard();
  if(cancelled())throw std::runtime_error("Adaptive encode cancelled before CPU retry");
 }
 try{
  if(cancelled())throw std::runtime_error("Adaptive encode cancelled before CPU retry");
  encode(false);
  if(!std::filesystem::is_regular_file(output)||!std::filesystem::file_size(output)||!validate(output))
   throw std::runtime_error("CPU encode output validation failed");
  if(cancelled())throw std::runtime_error("Adaptive encode cancelled during CPU output validation");
  return false;
 }catch(...){discard();throw;}
}
}
