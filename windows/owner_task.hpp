#pragma once
#include "platform.hpp"
namespace ow::win {
json::object enroll_owner_task(const std::filesystem::path& root);
json::object owner_lifecycle(const std::filesystem::path& root,bool restart,Tick timeout_ms);
json::object inspect_owner_task(const std::filesystem::path& root);
}
