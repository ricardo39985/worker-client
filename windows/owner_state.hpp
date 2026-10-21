#pragma once
#include "platform.hpp"
namespace ow::win {
// File-system policy only; never changes ACLs or loads/decrypts another profile.
void verify_owner_state_path(const std::filesystem::path&,bool private_identity=false);
void require_normal_owner();
bool owner_bootstrap_enrolled(const std::filesystem::path&);
void complete_owner_pairing(const std::filesystem::path&);
std::filesystem::path verified_owner_image(const std::filesystem::path& root,bool hash=true);
}
