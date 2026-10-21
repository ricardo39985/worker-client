#include "headless.hpp"
#include "pairing_adapter.hpp"
#include "control_pipe.hpp"
#include "owner_state.hpp"
namespace ow::win {
void validate_headless_identity(const std::filesystem::path& root){
 require_normal_owner();
 verify_owner_state_path(root);
 for(const auto& p:{root,root/"worker.json",root/"credential.dpapi",root/"pairing-key.dpapi",root/"journal.sqlite3"}){
  verify_owner_state_path(p,p.filename()=="credential.dpapi"||p.filename()=="pairing-key.dpapi");
 }
 auto cfg=load_config(root);ProtectedPairingSecrets secrets(root);auto saved=secrets.read(SecretSlot::credential);if(!saved)throw std::runtime_error("Existing owner credential is required");
 try{auto record=parse(*saved).as_object();if(text(record,"coordinator_url",1024)!=cfg.coordinator)throw std::runtime_error("Existing identity scope differs");}
 catch(...){SecureZeroMemory(saved->data(),saved->size());throw;}
 SecureZeroMemory(saved->data(),saved->size());(void)existing_public_pairing_key(root/"pairing-key.dpapi");
}
int run_headless(const std::filesystem::path& root) noexcept {
 try{
  require_normal_owner();verify_owner_state_path(root);verify_owner_state_path(root/"instance.lock");
  Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock)return 3;
  validate_headless_identity(root); // exclusive state before journal mutation, probes, UI or implicit key creation
  auto exe=verified_owner_image(root);if(_wcsicmp(exe.c_str(),executable_path().c_str())!=0)throw std::runtime_error("Actual running headless image differs from selected owner version");
  auto cfg=load_config(root);cfg.headless=true;Worker worker(std::move(cfg));
  ControlPipe pipe(root,[&](const json::object& request){return worker.control(request);});
  publish_runtime_ready(root);while(!worker.ready_to_exit())Sleep(50);clear_runtime_ready(root);return 0;
 }catch(...){clear_runtime_ready(root);return 4;} // never a MessageBox or secret-bearing error on a noninteractive desktop
}
}
