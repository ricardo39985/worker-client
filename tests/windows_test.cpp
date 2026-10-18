#include "test.hpp"
#include "platform.hpp"
#include "process.hpp"
#include "pairing_adapter.hpp"
#include "hardware.hpp"
#include "ow/device_identity.hpp"
#include "worker.hpp"
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
TEST("Windows adapter inventory has repeatable unique identities and real VRAM and driver fields"){
 auto first=enumerate_video_devices(),second=enumerate_video_devices();
 CHECK(first.size()==second.size());
 for(std::size_t i=0;i<first.size();++i){
  CHECK(!first[i].stable_id.empty());CHECK(first[i].stable_id==second[i].stable_id);
  CHECK(first[i].adapter_index==second[i].adapter_index);
  CHECK(first[i].physical_id==second[i].physical_id);
  if(first[i].backend_identity_verified){
   CHECK(adaptive::pci_instance_id(first[i].physical_id));
   CHECK(first[i].exclusion_reason.empty());
   for(std::size_t j=0;j<i;++j)if(first[j].backend_identity_verified){
    if(!first[i].physical_id.empty())CHECK(first[i].physical_id!=first[j].physical_id);
    if(first[i].family==first[j].family)CHECK(first[i].backend_index!=first[j].backend_index);
   }
  }
  if(first[i].physical_id.rfind("ROOT\\",0)==0){
   CHECK(!first[i].backend_identity_verified);
   CHECK(first[i].exclusion_reason.find("software adapter")!=std::string::npos);
  }
  if(!first[i].backend_identity_verified)CHECK(!first[i].exclusion_reason.empty());
  for(std::size_t j=0;j<i;++j)CHECK(first[i].stable_id!=first[j].stable_id);
  if(first[i].driver_healthy){CHECK(!first[i].driver_version.empty());CHECK(first[i].dedicated_vram_mb>0);}
 }
}
TEST("NoConversion setup inventories adapters before reporting disabled conversion"){
 Temp t;Config config;config.root=t.path;config.budget={256,777,256,1};auto result=probe_ffmpeg(config);
 CHECK(!result.video&&!result.media_video);CHECK(result.reason.find("converter not configured")!=std::string::npos);
 CHECK(result.reason.find("DXGI inventory entries=")!=std::string::npos);
 CHECK(result.reason.find("configured_global_VRAM_override_MB=not_set")!=std::string::npos);
 auto saved=parse(read_file(t.path/"adaptive-hardware.json")).as_object();
 CHECK(saved.at("profiles").as_array().empty());CHECK(saved.at("devices").is_array());
 CHECK(number(saved.at("limits").as_object(),"vram_mb_override",UINT64_MAX)==777);
}
TEST("global VRAM override is optional for old configs and honored when configured"){
 Temp t;auto path=t.path/"worker.json";
 auto write=[&](const std::string& vram){std::ofstream out(path,std::ios::trunc);
  out<<"{\"schema\":1,\"coordinator_url\":\"https://example.test\",\"storage_hosts\":[],\"ffmpeg_path\":\"\",\"ffmpeg_sha256\":\"\",\"limits\":{\"ram_mb\":1024,"<<vram<<"\"scratch_mb\":2048,\"cpu_threads\":2,\"max_jobs\":2,\"reserve_ram_mb\":0}}";};
 write("");CHECK(load_config(t.path).budget.vram_mb==0);CHECK(!load_config(t.path).vram_override_set);
 write("\"vram_mb\":0,");CHECK(load_config(t.path).vram_override_set);
 write("\"vram_mb\":777,");CHECK(load_config(t.path).budget.vram_mb==777);CHECK(load_config(t.path).vram_override_set);
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

TEST("adaptive capacity stays reserved until a cancelled native process tree drains"){
 Temp t;adaptive::Profile gpu{"synthetic-luid","driver","hash",adaptive::Backend::nvidia_nvenc,adaptive::Codec::h264,1,256,512,128,1,50,0,0,true};
 adaptive::Profile cpu{"cpu","","hash",adaptive::Backend::cpu,adaptive::Codec::h264,1,256,0,128,1,20,0,0,true};
 gpu.physical_id=R"(PCI\VEN_FFFF&DEV_0001\fixture)";adaptive::Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
 auto held=scheduler.reserve({adaptive::Codec::h264});CHECK(held);CHECK(scheduler.active_jobs()==1);
 CHECK(held.profiles().front().backend==adaptive::Backend::nvidia_nvenc);
 // The GPU job reserves fallback budgets, but does not occupy the CPU encoder.
 auto independent=scheduler.reserve({adaptive::Codec::h264});CHECK(independent);
 CHECK(independent.profiles().size()==1);
 CHECK(independent.profiles().front().backend==adaptive::Backend::cpu);
 CHECK(scheduler.active_jobs()==2);
 CHECK(!scheduler.reserve({adaptive::Codec::h264}));
 Child child(executable_directory()/"ow_test_child.exe",{},t.path,{128,0,16,1},Priority::low);
 auto root=wait_pid(t.path/"root.pid"),leaf=wait_pid(t.path/"leaf.pid");
 Handle rootHandle(OpenProcess(SYNCHRONIZE,FALSE,root)),leafHandle(OpenProcess(SYNCHRONIZE,FALSE,leaf));CHECK(rootHandle&&leafHandle);
 auto stopped=std::async(std::launch::async,[&]{child.stop();});
 do{
  CHECK(scheduler.active_jobs()==2);
  CHECK(!scheduler.reserve({adaptive::Codec::h264}));
 }while(stopped.wait_for(std::chrono::milliseconds(1))!=std::future_status::ready);
 stopped.get();
 CHECK(WaitForSingleObject(rootHandle.get(),1000)==WAIT_OBJECT_0);CHECK(WaitForSingleObject(leafHandle.get(),1000)==WAIT_OBJECT_0);
 CHECK(scheduler.active_jobs()==2);CHECK(!scheduler.reserve({adaptive::Codec::h264}));
 held.release();CHECK(scheduler.active_jobs()==1);
 auto after_drain=scheduler.reserve({adaptive::Codec::h264});CHECK(after_drain);
 CHECK(after_drain.profiles().front().backend==adaptive::Backend::nvidia_nvenc);
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
