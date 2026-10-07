#include "test.hpp"
#include "platform.hpp"
#include "process.hpp"
#include "pairing_adapter.hpp"
#include <fstream>
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
