#pragma once
#include <span>
#include <string>
namespace ow {
// Consumes a secret-bearing JSON record. Only its public coordinator origin may
// escape; plaintext and ALL caller-owned parser storage are wiped on every exit.
// No fallback allocation when workspace is exhausted.
std::string consume_secret_origin(std::string& plaintext,std::span<unsigned char> workspace);
}
