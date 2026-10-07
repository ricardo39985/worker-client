#pragma once
#include "ow/pairing.hpp"
#include "http.hpp"
namespace ow::win {
class ProtectedPairingSecrets final : public PairingSecrets {
 std::filesystem::path root_;
 std::filesystem::path path(SecretSlot) const;
public:
 explicit ProtectedPairingSecrets(std::filesystem::path root):root_(std::move(root)){}
 std::optional<std::string> read(SecretSlot) override;
 void write(SecretSlot,const std::string&) override;
 void erase(SecretSlot) override;
};
class NativePairingKey final : public PairingKey {
 std::filesystem::path path_;
public:
 explicit NativePairingKey(std::filesystem::path root):path_(std::move(root)/"pairing-key.dpapi"){}
 std::string public_key() override {return public_pairing_key(path_);}
 std::string sign(const std::string& message) override {return sign_pairing_challenge(path_,message);}
 std::string request_id() override {return random_hex(16);}
};
class NativePairingTransport final : public PairingTransport {
 Http http_;
public:
 PairingResponse request(const std::string& method,const std::string& url,const std::string& body) override {
  auto r=http_.request(method,url,body);return {r.status,std::move(r.body)};
 }
};
}
