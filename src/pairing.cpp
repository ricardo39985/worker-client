#include "ow/pairing.hpp"
#include "ow/protocol.hpp"
#include "ow/store.hpp"
#include <algorithm>
#include <cctype>
#include <limits>

namespace ow {
void bind_journal_origin(Store& store,const std::string& endpoint,const std::optional<std::string>& proven) {
 auto origin=coordinator_origin(endpoint);
 if(auto bound=store.get("coordinator_origin")) {
  if(coordinator_origin(*bound)!=origin)throw PairingActionRequired("Job journal belongs to another coordinator. Archive its state before changing endpoints.");
  return;
 }
 if(store.pending_count()||!store.interrupted().empty()) {
  if(!proven||coordinator_origin(*proven)!=origin)
   throw PairingActionRequired("Legacy job journal has unknown endpoint provenance. It was preserved and will not be sent.");
 }
 store.set("coordinator_origin",origin);
}
namespace {
bool ascii_space(unsigned char c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }
void printable(const std::string& value, std::size_t minimum, const char* error) {
 if(value.size()<minimum||std::any_of(value.begin(),value.end(),[](unsigned char c){return c<=32||c>=127;}))
  throw PairingActionRequired(error);
}
json::object secret_record(PairingSecrets& secrets,SecretSlot slot) {
 try {auto data=secrets.read(slot);return data?parse(*data).as_object():json::object{};}
 catch(...) {throw PairingActionRequired("Saved worker identity is unreadable; it was preserved. Repair or explicitly re-pair.");}
}
void scope(const json::object& record,const std::string& origin) {
 if(coordinator_origin(text(record,"coordinator_url",1024))!=origin)
  throw PairingActionRequired("Saved identity belongs to another endpoint. Explicit re-pair is required; nothing was sent.");
}
void cancelled(const PairingEnvironment& e) {
 if(e.cancelled())throw PairingActionRequired("Pairing cancelled; pending identity retained for restart.");
}
void status(const PairingResponse& r,unsigned expected) {
 if(r.status==401||r.status==403)throw PairingActionRequired("Coordinator rejected this identity. Administrator approval is required.");
 if(r.status!=expected)throw std::runtime_error("Worker coordinator returned HTTP "+std::to_string(r.status));
}
std::string credential_token(const json::object& value) {
 auto token=text(value,"token",4096);
 if(!safe_id(text(value,"worker_id",128)))throw PairingActionRequired("Invalid worker identity in approval.");
 printable(token,1,"Invalid worker credential in approval.");return token;
}
std::int64_t deadline(const json::object& pending) {
 return static_cast<std::int64_t>(number(pending,"expires_at_ms",INT64_MAX));
}
std::int64_t after(std::int64_t now,std::uint64_t seconds) {
 if(now<0||now>INT64_MAX-static_cast<std::int64_t>(seconds*1000))
  throw PairingActionRequired("Worker clock is outside the supported range.");
 return now+static_cast<std::int64_t>(seconds*1000);
}
}
std::string coordinator_origin(std::string input) {
 while(!input.empty()&&ascii_space(static_cast<unsigned char>(input.front())))input.erase(input.begin());
 while(!input.empty()&&ascii_space(static_cast<unsigned char>(input.back())))input.pop_back();
 auto reject=[](){throw PairingActionRequired("Enter a credential-free HTTPS worker origin on port 443, without a path, query or fragment.");};
 if(input.empty()||input.size()>1024)reject();
 for(auto& c:input){auto u=static_cast<unsigned char>(c);if(u<=32||u>=127)reject();c=static_cast<char>(std::tolower(u));}
 if(!input.starts_with("https://"))reject();
 auto authority=input.substr(8);if(authority.ends_with('/'))authority.pop_back();
 if(authority.find_first_of("/\\?#@")!=std::string::npos)reject();
 if(auto colon=authority.find(':');colon!=std::string::npos){if(authority.substr(colon)!=":443")reject();authority.resize(colon);}
 if(authority.empty()||authority.size()>253||authority.back()=='.')reject();
 std::size_t begin=0;
 while(begin<authority.size()){
  auto end=authority.find('.',begin);if(end==std::string::npos)end=authority.size();
  auto label=authority.substr(begin,end-begin);
  if(label.empty()||label.size()>63||label.front()=='-'||label.back()=='-')reject();
  for(unsigned char c:label)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))reject();
  begin=end+1;
 }
 return "https://"+authority;
}
std::string pairing_token(const std::string& endpoint,const std::string& name,
                          PairingSecrets& secrets,PairingTransport& http,PairingKey& key,
                          const PairingEnvironment& e) {
 const auto origin=coordinator_origin(endpoint);cancelled(e);
 auto saved=secrets.read(SecretSlot::credential);
 if(saved){
  auto record=secret_record(secrets,SecretSlot::credential);scope(record,origin);
  return credential_token(record);
 }
 auto pending=secret_record(secrets,SecretSlot::pending_pairing);
 if(!pending.empty()){
  scope(pending,origin);
  if(number(pending,"schema",1)!=1||text(pending,"public_key",512)!=key.public_key())
   throw PairingActionRequired("Pending pairing key changed. Existing state was preserved; explicit re-pair is required.");
  if(pending.if_contains("terminal")||deadline(pending)<=e.now_ms())
   throw PairingActionRequired("Pairing expired or was denied. Explicitly request a new pairing.");
 }
 auto protocol=http.request("GET",origin+"/v1/worker/protocol");status(protocol,200);cancelled(e);
 if(number(parse(protocol.body).as_object(),"protocol",1)!=1)
  throw PairingActionRequired("Incompatible worker coordinator protocol.");
 if(pending.empty()){
  auto public_key=key.public_key(),request_id=key.request_id();
  if(!safe_id(request_id)||request_id.size()<16||public_key.empty()||public_key.size()>512)
   throw PairingActionRequired("Local pairing identity is invalid.");
  pending={{"schema",1},{"coordinator_url",origin},{"request_id",request_id},
           {"public_key",public_key},{"expires_at_ms",after(e.now_ms(),600)}};
  // Persist before enrollment so a lost POST response is safely retried.
  secrets.write(SecretSlot::pending_pairing,json::serialize(pending));
 }
 if(!pending.if_contains("device_code")){
  if(name.empty()||name.size()>128)throw PairingActionRequired("Worker name must contain 1 to 128 UTF-8 bytes.");
  json::object request{{"protocol",1},{"name",name},{"platform","windows"},
   {"public_key_format","bcrypt-ecdsa-p256-public-blob-base64"},
   {"public_key",text(pending,"public_key",512)},{"request_id",text(pending,"request_id",128)}};
  auto response=http.request("POST",origin+"/v1/worker/pairings",json::serialize(request));status(response,201);cancelled(e);
  auto value=parse(response.body).as_object();
  auto device=text(value,"device_code",256),challenge=text(value,"challenge",256),code=text(value,"user_code",32);
  printable(device,32,"Invalid private pairing code.");printable(challenge,32,"Invalid pairing challenge.");
  if(code.empty()||std::any_of(code.begin(),code.end(),[](unsigned char c){return !((c>='A'&&c<='Z')||(c>='2'&&c<='9')||c=='-');}))
   throw PairingActionRequired("Invalid display code in pairing response.");
  auto seconds=number(value,"expires_in",600);if(!seconds)throw PairingActionRequired("Pairing expired.");
  auto interval=std::max<std::uint64_t>(2,number(value,"interval",30));
  pending["device_code"]=device;pending["challenge"]=challenge;pending["user_code"]=code;
  pending["interval"]=interval;pending["expires_at_ms"]=std::min(deadline(pending),after(e.now_ms(),seconds));
  // Resume material must be durable before displaying the approval code.
  secrets.write(SecretSlot::pending_pairing,json::serialize(pending));
 }
 const auto expires=deadline(pending);
 if(e.now_ms()>=expires)throw PairingActionRequired("Pairing expired.");
 auto device=text(pending,"device_code",256),challenge=text(pending,"challenge",256);
 printable(device,32,"Corrupt pending pairing code.");printable(challenge,32,"Corrupt pending challenge.");
 auto signature=key.sign("organizer-worker-pair-v1\n"+device+"\n"+challenge);
 auto interval=std::max<std::uint64_t>(2,number(pending,"interval",30));
 e.show_code(text(pending,"user_code",32),static_cast<unsigned>((expires-e.now_ms()+999)/1000));
 for(;;){
  cancelled(e);auto remaining=expires-e.now_ms();
  if(remaining<=0)throw PairingActionRequired("Pairing expired. Explicitly request a new code.");
  e.wait_ms(static_cast<unsigned>(std::min<std::int64_t>(remaining,static_cast<std::int64_t>(interval*1000))));
  cancelled(e);if(e.now_ms()>=expires)throw PairingActionRequired("Pairing expired. Explicitly request a new code.");
  auto r=http.request("POST",origin+"/v1/worker/pairings/token",json::serialize(json::object{{"device_code",device},{"signature",signature}}));
  cancelled(e);
  if(r.status==429){interval=std::min<std::uint64_t>(30,interval+5);continue;}
  status(r,200);auto reply=parse(r.body).as_object();auto state=text(reply,"status",32);
  if(state=="pending")continue;
  if(state=="denied"||state=="expired"){
   pending["terminal"]=state;secrets.write(SecretSlot::pending_pairing,json::serialize(pending));
   throw PairingActionRequired("Pairing "+state+". Administrator action is required.");
  }
  if(state!="approved")throw PairingActionRequired("Unknown pairing approval response.");
  auto token=credential_token(reply);cancelled(e);
  secrets.write(SecretSlot::credential,json::serialize(json::object{{"coordinator_url",origin},
   {"worker_id",text(reply,"worker_id",128)},{"token",token}}));
  // Failure here is harmless: reopening prioritizes the durable credential.
  try{secrets.erase(SecretSlot::pending_pairing);}catch(...){ }
  return token;
 }
}
}
