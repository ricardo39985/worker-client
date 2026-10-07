#pragma once
#include "http.hpp"
#include "process.hpp"
#include "ow/store.hpp"
#include "ow/progress.hpp"
#include <thread>
#include <atomic>
#include <unordered_map>
namespace ow::win {
struct Config {
 std::string coordinator,ffmpeg_sha256;
 std::filesystem::path root,ffmpeg;
 std::vector<std::string> storage_hosts;
 Resources budget{2048,0,8192,2};
 std::uint64_t reserve_ram_mb{0};std::size_t max_jobs{4};
};
Config load_config(const std::filesystem::path& root);
struct Probe {bool video{},image{};std::string reason;bool media_video{},media_image{};};
Probe probe_ffmpeg(const Config&,const std::function<bool()>& cancelled={});
class Worker {
 struct TaskData {JobSpec spec;std::string token;Tick hard_deadline{};std::atomic_bool cancel{},done{};ProgressCounter progress;};
 struct Running {std::shared_ptr<TaskData> data;std::jthread thread;};
 Config config_;Store store_;Log log_;Admission admission_;
 std::jthread network_,watchdog_;
 mutable std::mutex mutex_;std::unordered_map<std::string,std::unique_ptr<Running>> running_;
 std::atomic_bool paused_{},exit_requested_{},stop_{};
 std::atomic<Priority> priority_{Priority::normal};
 std::atomic_bool verified_video_{},verified_image_{},verified_media_video_{},verified_media_image_{};std::string status_{"STARTING"};
 std::unordered_map<std::string,JobSpec> offered_; // network thread only
 std::atomic<Preference> video_{Preference::allowed},image_{Preference::allowed};
 void network();void watchdog();void execute(std::shared_ptr<TaskData>);
 void message(WebSocket&,const json::object&);
 std::string paired_token();
 void state(std::string text);
 json::object heartbeat();
 void apply_preferences();
 void safe_log(const std::string&) noexcept;
public:
 explicit Worker(Config config);~Worker();
 Worker(const Worker&)=delete;Worker& operator=(const Worker&)=delete;
 void pause(bool);bool paused() const{return paused_;}
 void priority(Priority);Priority priority() const{return priority_;}
 void preference(bool video,Preference);Preference preference(bool video) const{return video?video_.load():image_.load();}
 void exit(bool stop_jobs);bool ready_to_exit() const;
 std::string status() const;std::size_t active_count() const;
};
}

