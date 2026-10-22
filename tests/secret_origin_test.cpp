#include "test.hpp"
#include "ow/secret_origin.hpp"
#include <algorithm>
#include <array>
using namespace ow;
namespace {template<class T>bool wiped(const T& data){return std::all_of(data.begin(),data.end(),[](auto c){return c==0;});}}
TEST("secret origin extraction returns only scope and wipes input plus caller-owned parser storage"){
 std::string secret=R"({"coordinator_url":"https://fixture.invalid","worker_id":"fixture-worker","token":"SYNTHETIC-NOT-A-REAL-CREDENTIAL"})";std::array<unsigned char,32768> workspace;workspace.fill(0xA5);
 CHECK(consume_secret_origin(secret,workspace)=="https://fixture.invalid");CHECK(wiped(secret));CHECK(wiped(workspace));
}
TEST("malformed secret records wipe plaintext and parser storage on exceptional exit"){
 std::string secret=R"({"token":"SYNTHETIC-NOT-REAL",)";std::array<unsigned char,32768> workspace;workspace.fill(0xA5);
 THROWS(consume_secret_origin(secret,workspace));CHECK(wiped(secret));CHECK(wiped(workspace));
}
TEST("headless credential scope requires actual valid bounded token and worker identity"){
 std::size_t index=0;for(const auto* record:{R"({"coordinator_url":"https://fixture.invalid","worker_id":"fixture-worker"})",R"({"coordinator_url":"https://fixture.invalid","worker_id":"fixture-worker","token":""})",R"({"coordinator_url":"https://fixture.invalid","worker_id":"bad worker","token":"synthetic"})",R"({"coordinator_url":"https://fixture.invalid","token":"synthetic"})"}){
  std::string secret=record;std::array<unsigned char,32768> workspace;workspace.fill(0xA5);try{(void)consume_secret_origin(secret,workspace);throw std::runtime_error("invalid record accepted at index "+std::to_string(index));}catch(const std::runtime_error& e){if(std::string(e.what()).starts_with("invalid record accepted"))throw;}CHECK(wiped(secret));CHECK(wiped(workspace));++index;
 }
}
TEST("secret parsing cannot fall back to untracked secret-bearing heap allocations"){
 std::string secret=R"({"coordinator_url":"https://fixture.invalid","worker_id":"fixture-worker","token":"SYNTHETIC-NOT-REAL"})";std::array<unsigned char,1> workspace{0xA5};
 THROWS(consume_secret_origin(secret,workspace));CHECK(wiped(secret));CHECK(wiped(workspace));
}
