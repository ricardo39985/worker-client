#pragma once
#include "platform.hpp"
#include <thread>
namespace ow::win {
// Owner-only; no target SID, arbitrary file path, commands or credentials on the wire.
// Pre-connection absence/busy only. Authentication/protocol/post-write failures
// are NOT absence and must never activate an alternate controller.
struct ControlUnavailable:std::runtime_error {using std::runtime_error::runtime_error;};
std::wstring control_pipe_name(const std::filesystem::path& root);
std::string process_owner_sid(HANDLE process);
class ControlPipe {
 std::atomic_bool stop_{};Handle pipe_;std::jthread thread_;
public:
 ControlPipe(const std::filesystem::path& root,std::function<json::object(const json::object&)> handler);
 ~ControlPipe();
};
json::object control_call(const std::filesystem::path& root,const json::object& request);
}
