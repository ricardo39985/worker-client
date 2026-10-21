#include "headless.hpp"
#include "pairing_adapter.hpp"
#include "control_pipe.hpp"
namespace ow::win {
void validate_headless_identity(const std::filesystem::path& root){
 const auto sid=process_owner_sid(GetCurrentProcess());
 if(elevated()||sid=="S-1-5-18"||sid=="S-1-5-19"||sid=="S-1-5-20")throw std::runtime_error("A non-elevated normal owner token is required");
 for(const auto& p:{root,root/"worker.json",root/"credential.dpapi",root/"pairing-key.dpapi",root/"journal.sqlite3"}){
  const auto a=GetFileAttributesW(p.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Existing owner state/profile required; no reparse or implicit pairing");
 }
 auto cfg=load_config(root);ProtectedPairingSecrets secrets(root);auto saved=secrets.read(SecretSlot::credential);if(!saved)throw std::runtime_error("Existing owner credential is required");
 try{auto record=parse(*saved).as_object();if(text(record,"coordinator_url",1024)!=cfg.coordinator)throw std::runtime_error("Existing identity scope differs");}
 catch(...){SecureZeroMemory(saved->data(),saved->size());throw;}
 SecureZeroMemory(saved->data(),saved->size());NativePairingKey key(root);(void)key.public_key();
}
int run_headless(const std::filesystem::path& root) noexcept {
 try{
  validate_headless_identity(root); // before journal mutation, probes, UI or implicit key creation
  Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock)return 3;
  auto cfg=load_config(root);cfg.headless=true;Worker worker(std::move(cfg));
  ControlPipe pipe(root,[&](const json::object& request){return worker.control(request);});
  publish_runtime_ready(root);while(!worker.ready_to_exit())Sleep(50);clear_runtime_ready(root);return 0;
 }catch(...){clear_runtime_ready(root);return 4;} // never a MessageBox or secret-bearing error on a noninteractive desktop
}
}
