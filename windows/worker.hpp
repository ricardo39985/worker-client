#pragma once
#include "http.hpp"
#include "process.hpp"
#include "ow/store.hpp"
#include "ow/progress.hpp"
#include "ow/adaptive_hardware.hpp"
#include "ow/probe_status.hpp"
#include "ow/probe_retry.hpp"
#include "ow/capability_report.hpp"
#include "ow/worker_status.hpp"
#include "capacity.hpp"
#include "ow/control.hpp"
#include "gemma.hpp"
#include "ow/result_delivery.hpp"
#include <array>
#include <thread>
#include <atomic>
#include <unordered_map>
namespace ow::win {
struct Config {
 std::string coordinator,ffmpeg_sha256;
 std::filesystem::path root,ffmpeg;
 std::vector<std::string> storage_hosts;
 Resources budget{2048,0,8192,2};
 bool vram_override_set{};
 std::uint64_t reserve_ram_mb{0};std::size_t max_jobs{4};
 std::function<bool()> capacity_foreground; // local measurement port; never parsed from config/network
 std::function<void(const std::string&,std::uint32_t)> observe_child; // local test/measurement observer, never config/network
};
Config load_config(const std::filesystem::path& root);
json::object load_control_document(const std::filesystem::path& root,Store& store,bool persist_migration=true);
json::object offline_control(const std::filesystem::path& root,const json::object& request);
struct Probe {bool video{},image{};std::string reason;bool media_video{},media_image{};std::vector<adaptive::Profile> profiles;std::vector<std::string> fallback_notes;std::vector<ProbeFailure> gpu_failures;bool complete{};};
using ProbePhase=std::function<void(std::string,unsigned,unsigned)>;
using ProbeTiming=std::function<void(const std::string&)>;
// retained may refer only to current successful in-process evidence, never disk profiles.
Probe probe_ffmpeg(const Config&,const std::function<bool()>& cancelled={},const ProbePhase& phase={},const Probe* retained=nullptr,const ProbeTiming& timing={});
class OfflineMediaHarness;
class Worker {
 friend class OfflineMediaHarness;
 Worker(Config config,bool isolated);
 struct TaskData {JobSpec spec;std::string token;Tick hard_deadline{};bool compute_handoff{};std::atomic_bool cancel{},done{};ProgressCounter progress,transfer_progress;std::array<ProgressCounter,3> rendition_progress;};
 struct Running {std::shared_ptr<TaskData> data;std::jthread thread;};
 Config config_;Store store_;Log log_;Admission admission_;CapacityControl capacity_;
 const Tick process_started_{monotonic_ms()};
 const std::int64_t process_started_utc_{utc_ms()};std::atomic<std::int64_t> last_contact_utc_{};
 std::recursive_mutex control_mutex_;
 std::unique_ptr<ControlSettings> settings_;json::object effective_config_;
 void update_settings(const json::object&);
 void apply_operator_settings(bool apply_pause=true);
 RuntimePressure pressure_; // guarded by mutex_
 std::map<std::string,std::unique_ptr<CapacityControl>> gpu_capacity_;
 Tick cached_capacity_until_{}; // device live-budget refresh, not capability proof
 bool isolated_{};
 std::filesystem::path offline_source_,offline_output_;
 std::map<adaptive::Codec,std::string> offline_pins_;
 unsigned preparation_parallelism_{3};
 ProgressLogPolicy progress_log_;
 std::mutex report_mutex_;WorkerStatus report_{utc_ms(),monotonic_ms()};StatusReadings readings_;bool report_io_failed_{};
 void publish_report(const std::filesystem::path&,const std::string&) noexcept;
 void report_status() noexcept;
 void report_state(std::string name,std::string reason);
 std::jthread network_,watchdog_,hash_watchdog_,recovery_;
 mutable std::mutex mutex_;std::unordered_map<std::string,std::unique_ptr<Running>> running_;
 std::atomic_bool paused_{},exit_requested_{},stop_{},report_connected_{};
 std::uint64_t hardware_generation_{};
 ProbeRecovery gpu_recovery_;
 ProbeStatus probe_status_;CapabilityReport capability_report_;
 std::string last_activity_;bool connected_once_{};
 std::string pairing_user_code_;std::int64_t pairing_expires_utc_{}; // public approval label only; guarded by mutex_
 std::atomic<Priority> priority_{Priority::normal};
 std::atomic_bool verified_video_{},verified_image_{},verified_media_video_{},verified_media_image_{};std::string status_{"STARTING"};
 std::atomic_bool ffmpeg_hash_changed_{};
 std::unique_ptr<GemmaQuery> gemma_query_;
 std::atomic_bool results_ready_{};
 std::optional<GemmaRuntime> gemma_runtime_;std::atomic_bool verified_gemma_{},gemma_dispatch_{};
 std::unordered_map<std::string,JobSpec> offered_; // network thread only
 std::vector<adaptive::Profile> hardware_profiles_;
 std::unique_ptr<adaptive::Scheduler> adaptive_scheduler_;
 std::atomic<Preference> video_{Preference::allowed},image_{Preference::allowed};
 void recover_gpus();
 void network();void watchdog();void execute(std::shared_ptr<TaskData>);
 using Send=std::function<void(const std::string&)>;
 void message(const Send&,const json::object&);
 void send_pending(const Send&);
 std::string paired_token();
 void state(std::string text);
 json::object heartbeat();
 void apply_preferences();
 adaptive::Scheduler::Reservation reserve_hardware_profiles(const std::vector<adaptive::Codec>& codecs,Resources& cost,const std::string& attempt,const std::string& stage,bool cpu_only,std::uint32_t width,std::uint32_t height,std::uint64_t minimum_ram=0);
 void wait_stage(const std::string& attempt,const std::string& stage,Resources cost,const std::function<bool()>& cancelled);
 void report_encoder_failure(const adaptive::Profile&);
 void safe_log(const std::string&) noexcept;
public:
 explicit Worker(Config config);~Worker();
 Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
 void pause(bool);bool paused() const{return paused_;}
 void priority(Priority);Priority priority() const{return priority_;}
 void preference(bool video,Preference);Preference preference(bool video) const{return video?video_.load():image_.load();}
 void exit(bool stop_jobs);bool ready_to_exit() const;
 json::object control(const json::object& request);
 std::string status() const;std::size_t active_count() const;
};
}

