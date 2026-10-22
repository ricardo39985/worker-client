#pragma once
#include "worker.hpp"
namespace ow::win {
void validate_headless_identity(const std::filesystem::path& root);
int run_headless(const std::filesystem::path& root) noexcept;
}
