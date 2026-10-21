#pragma once
#include "platform.hpp"
namespace ow::win {
// File-system policy only; never changes ACLs or loads/decrypts another profile.
void verify_owner_state_path(const std::filesystem::path&,bool private_identity=false);
void require_normal_owner();
}
