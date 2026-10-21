// Isolated old/new controller adapter for real legacy persistence/publication
// contracts. Not a shipping worker entry point; never accesses default user data.
#include "worker.hpp"
#include "pairing_adapter.hpp"
#ifndef OW_LEGACY_ACTOR
#include "control_pipe.hpp"
#endif
#include <iostream>
namespace ow::win {
class OfflineMediaHarness {
public:
 static int legacy_actor(const std::filesystem::path& root){
  if(elevated())throw std::runtime_error("Legacy acceptance actor requires LIMITED owner");DWORD session{};require(ProcessIdToSessionId(GetCurrentProcessId(),&session),"Actor session");if(!session)throw std::runtime_error("Actor requires interactive owner session");
#ifdef OW_LEGACY_ACTOR
  if(!std::filesystem::exists(root/"worker.json")){
   std::filesystem::create_directories(root);
   atomic_write(root/"worker.json",json::serialize(json::object{{"schema",1},{"coordinator_url","https://legacy-fixture.invalid"},{"storage_hosts",json::array{}},{"ffmpeg_path",""},{"ffmpeg_sha256",""},{"limits",{{"ram_mb",1024},{"reserve_ram_mb",0},{"scratch_mb",4096},{"cpu_threads",2},{"max_jobs",4}}}}));
   ProtectedPairingSecrets secrets(root);secrets.write(SecretSlot::credential,json::serialize(json::object{{"coordinator_url","https://legacy-fixture.invalid"},{"worker_id","fixture-identity"},{"token","fixture-only-not-a-real-credential"}}));
   NativePairingKey key(root);key.public_key();
  }
#endif
  Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock)fail("Actor instance lock");
  auto worker=std::unique_ptr<Worker>(new Worker(load_config(root),true));
#ifdef OW_LEGACY_ACTOR
  worker->priority(Priority::backup);worker->preference(true,Preference::disabled);worker->preference(false,Preference::preferred);worker->pause(true);
#else
  ControlPipe pipe(root,[&](const json::object& r){return worker->control(r);});
#endif
  worker->report_connected_=true;worker->safe_log("FIXTURE TRANSPORT STATE CONNECTED; not a real coordinator");
  publish_runtime_ready(root);
  auto priority=[](Priority p){return p==Priority::maximum?"maximum":p==Priority::normal?"normal":p==Priority::low?"low":"backup";};
  auto preference=[](Preference p){return p==Preference::preferred?"preferred":p==Preference::allowed?"allowed":"disabled";};
  atomic_write(root/"actor-ready.json",json::serialize(json::object{{"fixture",true},{"real_coordinator",false},{"pid",GetCurrentProcessId()},{"session",session},{"elevated",false},{"operator",{{"priority",priority(worker->priority())},{"video_preference",preference(worker->preference(true))},{"image_preference",preference(worker->preference(false))},{"paused",worker->paused()}}}}));
  const auto until=monotonic_ms()+600000;while(!worker->ready_to_exit()&&!std::filesystem::exists(root/"stop")&&monotonic_ms()<until)Sleep(50);
  if(monotonic_ms()>=until)throw std::runtime_error("Actor timeout");worker->exit(false);while(!worker->ready_to_exit())Sleep(50);clear_runtime_ready(root);return 0;
 }
};
}
int wmain(int argc,wchar_t** argv){try{if(argc!=2)return 2;return ow::win::OfflineMediaHarness::legacy_actor(argv[1]);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
