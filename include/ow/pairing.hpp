#pragma once
#include "json.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace ow {
// Canonical, credential-free HTTPS origin. No implicit production endpoint.
std::string coordinator_origin(std::string input);

class Store;
// Called before recovery/network transmission. Legacy busy journals need a
// matching protected credential origin; unknown provenance fails closed.
void bind_journal_origin(Store&,const std::string& origin,
                         const std::optional<std::string>& proven_origin = {});

enum class SecretSlot { credential, pending_pairing };
class PairingSecrets {
public:
 virtual ~PairingSecrets() = default;
 virtual std::optional<std::string> read(SecretSlot) = 0;
 virtual void write(SecretSlot, const std::string&) = 0;
 virtual void erase(SecretSlot) = 0;
};
struct PairingResponse { unsigned status{}; std::string body; };
class PairingTransport {
public:
 virtual ~PairingTransport() = default;
 virtual PairingResponse request(const std::string& method, const std::string& url,
                                 const std::string& body = {}) = 0;
};
class PairingKey {
public:
 virtual ~PairingKey() = default;
 virtual std::string public_key() = 0;
 virtual std::string sign(const std::string& message) = 0;
 virtual std::string request_id() = 0;
};
class PairingActionRequired : public std::runtime_error {
public:
 using std::runtime_error::runtime_error;
};
struct PairingEnvironment {
 std::function<std::int64_t()> now_ms;
 std::function<void(unsigned)> wait_ms;
 std::function<bool()> cancelled;
 // Only the public operator code and remaining time may be displayed.
 std::function<void(const std::string&, unsigned)> show_code;
};
// Returns only after the endpoint-scoped credential is durable. Network errors
// preserve pending enrollment for retry with the same key/request/proof.
std::string pairing_token(const std::string& origin, const std::string& name,
                          PairingSecrets&, PairingTransport&, PairingKey&,
                          const PairingEnvironment&);
}
