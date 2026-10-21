#pragma once
#include "ow/json.hpp"
#include <functional>
#include <mutex>
#include <vector>
namespace ow {
// Local control protocol, deliberately separate from coordinator protocol v1.
inline constexpr unsigned control_schema=1;
inline constexpr std::size_t control_config_limit=256*1024;
// Replies include current + effective values. Bound them above two full files.
inline constexpr std::size_t control_frame_limit=1024*1024;
json::object control_defaults();
json::object validate_control_config(json::object);
json::object control_request(const std::vector<std::string>& args,const std::string& input={});
json::object validate_control_request(const json::object&);
struct ControlFailure : std::runtime_error {int exit_code;ControlFailure(int code,const char* message):std::runtime_error(message),exit_code(code){}};
std::string control_help();
class ControlSettings {
 mutable std::mutex mutex_;
 json::object current_;
 std::function<void(const json::object&)> persist_;
public:
 ControlSettings(json::object document,std::function<void(const json::object&)> persist);
 json::object current() const;
 json::object patch(const json::object& values);
 json::object reset(const std::vector<std::string>& names);
 json::array describe(const json::object& effective,bool live) const;
};
}
