#include "test.hpp"
#include "ow/pairing.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace ow;
namespace {
struct Secrets : PairingSecrets {
 std::filesystem::path root;
 bool fail_credential_write{}, fail_erase{};
 Secrets() : root(std::filesystem::temp_directory_path() /
    ("ow-pairing-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
  std::filesystem::create_directories(root);
 }
 ~Secrets(){std::error_code e;std::filesystem::remove_all(root,e);}
 std::filesystem::path path(SecretSlot slot){return root/(slot==SecretSlot::credential?"credential":"pending");}
 std::optional<std::string> read(SecretSlot slot) override {
  std::ifstream f(path(slot));if(!f)return {};return std::string(std::istreambuf_iterator<char>(f),{});
 }
 void write(SecretSlot slot,const std::string& value) override {
  if(slot==SecretSlot::credential&&fail_credential_write)throw std::runtime_error("synthetic disk failure");
  std::ofstream f(path(slot));f<<value;f.flush();CHECK(f.good());
 }
 void erase(SecretSlot slot) override {if(fail_erase)throw std::runtime_error("synthetic cleanup failure");std::filesystem::remove(path(slot));}
};
struct Key : PairingKey {
 std::string public_key() override {return "synthetic-p256-public-key";}
 std::string sign(const std::string& message) override {
  CHECK(message=="organizer-worker-pair-v1\nprivate-device-code-0123456789abcdef\nprivate-challenge-0123456789abcdef");
  return "synthetic-signature-for-this-key";
 }
 std::string request_id() override {return "request-0123456789abcdef0123456789abcdef";}
};
struct Transport : PairingTransport {
 struct Request{std::string method,url;json::object body;};
 std::vector<Request> requests;
 std::function<PairingResponse(const Request&)> reply;
 PairingResponse request(const std::string& method,const std::string& url,const std::string& body) override {
  requests.push_back({method,url,body.empty()?json::object{}:parse(body).as_object()});
  if(reply)return reply(requests.back());
  if(method=="GET")return {200,R"({"protocol":1})"};
  if(url.ends_with("/pairings"))return {201,R"({"user_code":"H7K4-P2Q9","device_code":"private-device-code-0123456789abcdef","challenge":"private-challenge-0123456789abcdef","expires_in":600,"interval":2})"};
  return {200,R"({"status":"approved","worker_id":"machine-1","token":"synthetic-permanent-credential"})"};
 }
};
struct Fixture {
 Secrets secrets;Transport http;Key key;
 std::int64_t now=1000000;bool cancelled=false;std::vector<std::string> codes;unsigned waits=0;
 PairingEnvironment environment(){return {[this]{return now;},[this](unsigned ms){CHECK(ms>=1000&&ms<=30000);now+=ms;++waits;},[this]{return cancelled;},[this](const std::string& code,unsigned seconds){
  CHECK(secrets.read(SecretSlot::pending_pairing));CHECK(code=="H7K4-P2Q9");CHECK(seconds>0&&seconds<=600);codes.push_back(code);
 }};}
 std::string run(std::string origin="https://worker.example.test") {return pairing_token(origin,"Office PC",secrets,http,key,environment());}
 void stored(std::string origin="https://worker.example.test") {secrets.write(SecretSlot::credential,json::serialize(json::object{{"coordinator_url",origin},{"worker_id","machine-1"},{"token","synthetic-permanent-credential"}}));}
};
}
TEST("endpoint supplied by operator has a stable HTTPS origin"){
 CHECK(coordinator_origin(" HTTPS://WORKER.Example.Test:443/ ")=="https://worker.example.test");
 CHECK(coordinator_origin("https://other.example.test")=="https://other.example.test");
}
TEST("missing unsafe or ambiguous endpoints cannot become network destinations"){
 for(auto bad:{"","https://","http://host.test","wss://host.test/worker/connect","https://user:pass@host.test","https://host.test:8443","https://host.test/path","https://host.test/?q=x","https://host.test/#x","https://host.test./","https://a..test","https://host.test\\evil","https://host.test\r\nX: x","https://host.test//"})THROWS(coordinator_origin(bad));
}
TEST("an absent endpoint fails before enrollment or key disclosure"){
 Fixture f;THROWS(f.run(""));CHECK(f.http.requests.empty());CHECK(!f.secrets.read(SecretSlot::pending_pairing));
}
TEST("credential reopen reconnects without a new enrollment"){
 Fixture f;f.stored();CHECK(f.run()=="synthetic-permanent-credential");CHECK(f.http.requests.empty());
}
TEST("credential canonicalization allows same origin but never a different server"){
 Fixture f;f.stored("https://WORKER.example.test:443/");CHECK(f.run()=="synthetic-permanent-credential");
 auto before=f.secrets.read(SecretSlot::credential);THROWS(f.run("https://other.example.test"));CHECK(f.http.requests.empty());CHECK(before==f.secrets.read(SecretSlot::credential));
}
TEST("corrupt saved credential never triggers silent reenrollment"){
 Fixture f;f.secrets.write(SecretSlot::credential,"corrupt");THROWS(f.run());CHECK(f.http.requests.empty());CHECK(f.secrets.read(SecretSlot::credential)=="corrupt");
}
TEST("successful pairing persists identity before returning and removes pending secret"){
 Fixture f;CHECK(f.run()=="synthetic-permanent-credential");CHECK(!f.codes.empty());CHECK(!f.secrets.read(SecretSlot::pending_pairing));
 auto c=parse(*f.secrets.read(SecretSlot::credential)).as_object();CHECK(text(c,"coordinator_url")=="https://worker.example.test");CHECK(text(c,"worker_id")=="machine-1");
 CHECK(f.run()=="synthetic-permanent-credential");
 for(const auto& r:f.http.requests)CHECK(r.url.starts_with("https://worker.example.test/"));
}
TEST("incompatible coordinator receives no public key or enrollment request"){
 Fixture f;f.http.reply=[](const auto&){return PairingResponse{200,R"({"protocol":2})"};};THROWS(f.run());CHECK(f.http.requests.size()==1);CHECK(f.http.requests.front().method=="GET");
}
TEST("lost initial response reuses durable request id rather than inventing another enrollment"){
 Fixture f;f.http.reply=[](const auto& r)->PairingResponse{if(r.method=="GET")return {200,R"({"protocol":1})"};throw std::runtime_error("lost reply");};
 THROWS(f.run());auto before=parse(*f.secrets.read(SecretSlot::pending_pairing)).as_object();CHECK(text(before,"request_id")=="request-0123456789abcdef0123456789abcdef");
 f.http.reply={};CHECK(f.run()=="synthetic-permanent-credential");
 for(const auto& r:f.http.requests)if(r.url.ends_with("/pairings"))CHECK(text(r.body,"request_id")==text(before,"request_id"));
}
TEST("lost approval response resumes same enrollment with its private key proof"){
 Fixture f;Transport reference;f.http.reply=[&](const auto& r)->PairingResponse{if(r.url.ends_with("/token"))throw std::runtime_error("lost reply");return reference.request(r.method,r.url,json::serialize(r.body));};
 THROWS(f.run());CHECK(f.secrets.read(SecretSlot::pending_pairing));f.http.requests.clear();f.http.reply={};CHECK(f.run()=="synthetic-permanent-credential");
 for(const auto& r:f.http.requests){CHECK(!r.url.ends_with("/pairings"));if(r.url.ends_with("/token"))CHECK(text(r.body,"signature")=="synthetic-signature-for-this-key");}
}
TEST("pending enrollment is never sent to a different endpoint"){
 Fixture f;Transport reference;f.http.reply=[&](const auto& r)->PairingResponse{if(r.url.ends_with("/token"))throw std::runtime_error("offline");return reference.request(r.method,r.url,json::serialize(r.body));};
 THROWS(f.run());auto old=f.secrets.read(SecretSlot::pending_pairing);f.http.requests.clear();THROWS(f.run("https://other.example.test"));CHECK(f.http.requests.empty());CHECK(f.secrets.read(SecretSlot::pending_pairing)==old);
}
TEST("failed credential persistence preserves resumable approval and returns no success"){
 Fixture f;f.secrets.fail_credential_write=true;THROWS(f.run());CHECK(!f.secrets.read(SecretSlot::credential));CHECK(f.secrets.read(SecretSlot::pending_pairing));f.secrets.fail_credential_write=false;f.http.requests.clear();CHECK(f.run()=="synthetic-permanent-credential");for(const auto& r:f.http.requests)CHECK(!r.url.ends_with("/pairings"));
}
TEST("pending cleanup failure cannot discard an already durable credential"){
 Fixture f;f.secrets.fail_erase=true;CHECK(f.run()=="synthetic-permanent-credential");f.http.requests.clear();CHECK(f.run()=="synthetic-permanent-credential");CHECK(f.http.requests.empty());
}
TEST("late approval after cancellation does not publish credentials"){
 Fixture f;Transport reference;f.http.reply=[&](const auto& r){auto response=reference.request(r.method,r.url,json::serialize(r.body));if(r.url.ends_with("/token"))f.cancelled=true;return response;};
 THROWS(f.run());CHECK(!f.secrets.read(SecretSlot::credential));CHECK(f.secrets.read(SecretSlot::pending_pairing));
}
TEST("denial is explicit and cannot erase an existing pending record"){
 Fixture f;Transport reference;f.http.reply=[&](const auto& r){if(r.url.ends_with("/token"))return PairingResponse{200,R"({"status":"denied"})"};return reference.request(r.method,r.url,json::serialize(r.body));};
 bool denied=false;try{f.run();}catch(const PairingActionRequired&){denied=true;}CHECK(denied);CHECK(!f.secrets.read(SecretSlot::credential));CHECK(f.secrets.read(SecretSlot::pending_pairing));
}
TEST("server revocation does not silently start another pairing"){
 Fixture f;f.http.reply=[](const auto&){return PairingResponse{403,""};};bool rejected=false;try{f.run();}catch(const PairingActionRequired&){rejected=true;}CHECK(rejected);CHECK(f.http.requests.size()==1);
}
TEST("rate limited polling waits and remains bounded by pairing expiry"){
 Fixture f;Transport reference;f.http.reply=[&](const auto& r){if(r.url.ends_with("/token"))return PairingResponse{429,""};return reference.request(r.method,r.url,json::serialize(r.body));};
 THROWS(f.run());CHECK(f.waits>0&&f.waits<310);CHECK(!f.secrets.read(SecretSlot::credential));
}
TEST("invalid permanent tokens and worker ids never reach credential storage"){
 for(auto response:{R"({"status":"approved","worker_id":"machine-1","token":""})",R"({"status":"approved","worker_id":"../other","token":"secret"})",R"({"status":"approved","worker_id":"machine-1","token":"bad\r\nheader"})"}){
  Fixture f;Transport reference;f.http.reply=[&](const auto& r){if(r.url.ends_with("/token"))return PairingResponse{200,response};return reference.request(r.method,r.url,json::serialize(r.body));};THROWS(f.run());CHECK(!f.secrets.read(SecretSlot::credential));
 }
}
