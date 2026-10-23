#pragma once
#include <cstdint>
#include <mutex>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace ow {
using Tick = std::int64_t; // monotonic milliseconds; never persisted across boots
struct Resources {
    std::uint64_t ram_mb{}, vram_mb{}, scratch_mb{}, cpu_threads{};
    bool operator==(const Resources&) const = default;
};
enum class Preference { preferred, allowed, disabled };
enum class Phase { reserved, running, cancelling };
enum class Start { started, already_running, declined };
struct Offer {
    std::string job_id, attempt_id, capability, fingerprint;
    Resources resources;
    Tick reserve_until{};
    bool operator==(const Offer&) const = default;
};
struct Active {
    Offer offer;
    Phase phase{Phase::reserved};
    std::string lease_token;
    Tick deadline{};
    std::uint64_t renewal_sequence{};
    std::uint64_t extra_ram_mb{};
    bool compute_released{};
    std::uint64_t released_scratch_mb{};
    std::map<std::string,Resources> stages;
};
struct Decision { bool accepted{}; std::string reason; };
struct Expired { std::string attempt_id; bool stop_process{}; };
class Admission {
public:
    Admission(Resources budget, std::size_t max_jobs);
    void capability(std::string name, bool verified, Preference preference);
    // Controller-owned model memory remains charged for the process lifetime.
    bool reserve_resident_memory(std::uint64_t mb);
    void pause(bool value);
    void defer_new_starts(std::string reason);
    Decision start_gate() const; // fresh local admission gate, not a promise for any particular offer
    // Atomically excludes offers while all job reservations are drained.
    bool begin_probe();
    void end_probe();
    Decision offer(const Offer&, Tick now, Resources available);
    Start begin(const std::string& attempt, std::string token, Tick deadline, Tick now);
    bool renew(const std::string& attempt, const std::string& token,
               std::uint64_t sequence, Tick deadline, Tick now);
    bool cancel(const std::string& attempt);
    bool finish(const std::string& attempt);
    // Grow only a live running task; never alter its original offer identity.
    bool reserve_ram(const std::string& attempt,std::uint64_t total_mb,Resources available,Tick now);
    // Caller has drained input/inspection children. Keep scratch/attempt identity,
    // but replace job-wide compute escrow with per-stage atomic claims.
    bool release_compute(const std::string& attempt);
    Decision stage(const std::string& attempt,const std::string& stage,Resources cost,Tick now,Resources available,unsigned concurrent_limit=64);
    void waiting(const std::string& attempt,const std::string& stage,const std::string& reason);
    unsigned stage_count() const;
    std::string queue_reason(const std::string& attempt,const std::string& stage) const;
    // Called after the stage's complete contained process tree has drained.
    bool finish_stage(const std::string& attempt,const std::string& stage);
    bool release_scratch(const std::string& attempt,std::uint64_t removed_mb);
    std::vector<std::string> capacity_lines() const;
    std::vector<Expired> expire(Tick now);
    std::vector<Active> active() const;
    Resources used() const;
private:
    struct Capability { bool verified; Preference preference; };
    mutable std::mutex mutex_;
    Resources budget_, used_{};
    std::size_t max_jobs_;
    bool paused_{},probing_{};std::string deferred_reason_;
    std::unordered_map<std::string, Capability> capabilities_;
    std::unordered_map<std::string, Active> active_;
    std::map<std::pair<std::string,std::string>,std::string> waiting_;
    void release(const Resources&);
};
bool safe_id(const std::string&);
}
