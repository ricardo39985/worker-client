#include "pairing_adapter.hpp"
namespace ow::win {
std::filesystem::path ProtectedPairingSecrets::path(SecretSlot slot) const {
 return root_/(slot==SecretSlot::credential?"credential.dpapi":"pending-pairing.dpapi");
}
std::optional<std::string> ProtectedPairingSecrets::read(SecretSlot slot) {
 auto file=path(slot);if(!std::filesystem::exists(file))return {};
 return unprotect(read_file(file));
}
void ProtectedPairingSecrets::write(SecretSlot slot,const std::string& value) {
 atomic_write(path(slot),protect(value));
}
void ProtectedPairingSecrets::erase(SecretSlot slot) {std::filesystem::remove(path(slot));}
}
