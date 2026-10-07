#pragma once
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
struct sqlite3;
namespace ow {
struct Pending {std::int64_t sequence;std::string attempt_id,body;};
class Store {
public:
 explicit Store(const std::filesystem::path& path);
 ~Store();Store(const Store&)=delete;Store& operator=(const Store&)=delete;
 bool reserve(const std::string& id,const std::string& job,const std::string& payload);
 bool start(const std::string& id);
 std::optional<std::string> state(const std::string& id);
 void result(const std::string& id,const std::string& body);
 std::vector<Pending> pending();
 std::uint64_t pending_count();
 bool acknowledge(std::int64_t sequence,const std::string& id);
 std::vector<std::string> interrupted();
 void set(const std::string& key,const std::string& value);
 std::optional<std::string> get(const std::string& key);
private:
 sqlite3* db_{};std::mutex mutex_;
 void exec(const char* sql);
};
}
