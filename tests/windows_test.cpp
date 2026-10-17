#include "test.hpp"
#include "platform.hpp"
#include "process.hpp"
#include "pairing_adapter.hpp"
#include "ow/rendition_pipeline.hpp"
#include <algorithm>
#include <future>
#include <atomic>
#include <fstream>
#include <thread>
using namespace ow;using namespace ow::win;
namespace {
struct Temp {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("ow-native-test-"+random_hex(12));
 Temp(){std::filesystem::create_directories(path);}
 ~Temp(){std::error_code error;std::filesystem::remove_all(path,error);}
};
DWORD wait_pid(const std::filesystem::path& path){
 auto until=monotonic_ms()+15000;
 while(monotonic_ms()<until){std::ifstream file(path);DWORD pid=0;if(file>>pid&&pid)return pid;Sleep(25);}
 throw std::runtime_error("Test child did not publish PID before deadline");
}
}
TEST("Windows UTF8 conversion preserves non-ASCII filenames"){
 std::string name="worker-\xc3\xa9-\xf0\x9f\x90\xa6";
 CHECK(utf8(wide(name))==name);THROWS(wide(std::string("\xff",1)));
}
TEST("DPAPI round-trips binary credentials and rejects corruption"){
 std::string secret("worker\0credential",17);auto encrypted=protect(secret);
 CHECK(encrypted!=secret);CHECK(unprotect(encrypted)==secret);
 encrypted[encrypted.size()/2]^=0x40;THROWS(unprotect(encrypted));
}
TEST("pairing public identity persists without storing a plaintext private key"){
 Temp t;auto key=t.path/"identity.dpapi";auto first=public_pairing_key(key);
 CHECK(!first.empty());CHECK(public_pairing_key(key)==first);
 auto encrypted=read_file(key);CHECK(encrypted!=unprotect(encrypted));
 CHECK(!sign_pairing_challenge(key,"test-domain\nchallenge").empty());
}
TEST("stopping a contained job terminates both root and descendant"){
 Temp t;Child child(executable_directory()/"ow_test_child.exe",{},t.path,{128,0,16,1},Priority::low);
 auto root=wait_pid(t.path/"root.pid"),leaf=wait_pid(t.path/"leaf.pid");
 Handle rootHandle(OpenProcess(SYNCHRONIZE,FALSE,root)),leafHandle(OpenProcess(SYNCHRONIZE,FALSE,leaf));
 CHECK(rootHandle&&leafHandle);child.stop();
 CHECK(WaitForSingleObject(rootHandle.get(),1000)==WAIT_OBJECT_0);
 CHECK(WaitForSingleObject(leafHandle.get(),1000)==WAIT_OBJECT_0);
}

TEST("protected enrollment and credentials remain readable after adapter reopen"){
 Temp t;
 {ProtectedPairingSecrets s(t.path);s.write(SecretSlot::pending_pairing,"synthetic pending");s.write(SecretSlot::credential,"synthetic credential");}
 ProtectedPairingSecrets reopened(t.path);
 CHECK(reopened.read(SecretSlot::pending_pairing)=="synthetic pending");
 CHECK(reopened.read(SecretSlot::credential)=="synthetic credential");
 reopened.erase(SecretSlot::pending_pairing);
 CHECK(!reopened.read(SecretSlot::pending_pairing));
 CHECK(reopened.read(SecretSlot::credential)=="synthetic credential");
}

TEST("runtime readiness identifies this process and is removed on shutdown"){
 Temp t;publish_runtime_ready(t.path);
 auto value=parse(read_file(t.path/"runtime.json")).as_object();
 CHECK(number(value,"schema",1)==1);
 CHECK(number(value,"process_id",UINT32_MAX)==GetCurrentProcessId());
 CHECK(number(value,"process_started_filetime",INT64_MAX)>0);
 CHECK(text(value,"state")=="running");
 clear_runtime_ready(t.path);
 CHECK(!std::filesystem::exists(t.path/"runtime.json"));
}

TEST("durable log rotation keeps concurrent lines whole and free of terminal escapes"){
 Temp t;auto path=t.path/"worker.log";
 {
  Log log(path);std::vector<std::thread> writers;
  for(unsigned worker=0;worker<6;++worker)writers.emplace_back([&,worker]{
   auto payload="T"+std::to_string(worker)+":"+std::string(3000,static_cast<char>('A'+worker))+"\x1b[31m";
   for(unsigned line=0;line<300;++line)log.write(payload);
  });
  for(auto& writer:writers)writer.join();
 }
 std::size_t lines{};
 for(const auto& file:std::vector<std::filesystem::path>{path,path.wstring()+L".1"}){
  CHECK(std::filesystem::exists(file));std::ifstream input(file,std::ios::binary);std::string line;
  while(std::getline(input,line)){
   ++lines;CHECK(!line.empty()&&line.back()=='\r');line.pop_back();CHECK(line.find('\x1b')==std::string::npos);
   CHECK(line.size()>=25+3+3000);auto marker=line.substr(25,3);CHECK(marker[0]=='T'&&marker[1]>='0'&&marker[1]<='5'&&marker[2]==':');
   auto expected=static_cast<char>('A'+(marker[1]-'0'));CHECK(std::all_of(line.begin()+28,line.begin()+28+3000,[&](char value){return value==expected;}));
  }
 }
 CHECK(lines==1800);
}

TEST("pipeline upload failure drains a real contained process tree before unwinding") {
 Temp t;std::ofstream(t.path/"source.marker")<<"synthetic input";Handle root,leaf;std::promise<void> child_ready;
 auto ready=child_ready.get_future().share();std::atomic_bool transfer_joined{};
 std::string reason;
 try{run_rendition_pipeline(std::vector<int>{0,1},[&](int i,std::stop_token stop){
  if(!i)return;
  Child child(executable_directory()/"ow_test_child.exe",{},t.path,{128,0,16,1},Priority::normal);
  root.reset(OpenProcess(SYNCHRONIZE,FALSE,wait_pid(t.path/"root.pid")));
  leaf.reset(OpenProcess(SYNCHRONIZE,FALSE,wait_pid(t.path/"leaf.pid")));
  child_ready.set_value();auto until=monotonic_ms()+5000;
  while(!stop.stop_requested()&&monotonic_ms()<until)Sleep(10);
  if(!stop.stop_requested())throw std::runtime_error("pipeline did not cancel native child");
  throw std::runtime_error("secondary encoder cancellation");
 },[&](int i,std::stop_token){
  if(i==0){if(ready.wait_for(std::chrono::seconds(20))!=std::future_status::ready)throw std::runtime_error("native child not ready");
   CHECK(std::filesystem::exists(t.path/"source.marker"));transfer_joined=true;
   throw std::runtime_error("synthetic upload rejected");}
  return i;
 });}catch(const std::exception& e){reason=e.what();}
 CHECK(reason=="synthetic upload rejected");CHECK(transfer_joined);CHECK(root&&leaf);
 CHECK(WaitForSingleObject(root.get(),1000)==WAIT_OBJECT_0);
 CHECK(WaitForSingleObject(leaf.get(),1000)==WAIT_OBJECT_0);
 CHECK(std::filesystem::exists(t.path/"source.marker"));
}
