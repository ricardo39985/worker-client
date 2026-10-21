#pragma once
#include "platform.hpp"
namespace ow::win {
std::filesystem::path bootstrap_anchor_directory();
std::filesystem::path bootstrap_launcher_path();
// Elevated same-owner installation/approval METADATA ONLY, never executes a
// package or stores a credential. Caller already validated selected version.
void install_bootstrap_anchor(const std::filesystem::path& selected,const std::string& enrollment_id);
json::object read_bootstrap_policy();
}
