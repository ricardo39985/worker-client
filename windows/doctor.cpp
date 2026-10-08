#include "worker.hpp"
#include "embedding.hpp"
#include "pairing_adapter.hpp"
#include <iostream>
using namespace ow;using namespace ow::win;
int wmain(int argc,wchar_t** argv){
 json::object report{{"agent_version","0.1.3"},{"runtime_healthy",false},{"processing_ready",false}};
 try{
  bool embedding=false,probe=false,network=false,bind_journal=false;auto root=data_directory();for(int i=1;i<argc;++i){std::wstring a=argv[i];if(a==L"--json")continue;else if(a==L"--probe-embedding")embedding=true;else if(a==L"--probe-ffmpeg")probe=true;else if(a==L"--check-coordinator")network=true;else if(a==L"--bind-journal")bind_journal=true;else if(a==L"--data-root"&&i+1<argc)root=argv[++i];else throw std::runtime_error("Unknown doctor argument");}
  std::filesystem::create_directories(root);auto free=available_resources(root,0);report["available_ram_mb"]=free.ram_mb;report["free_disk_mb"]=free.scratch_mb;report["logical_cpus"]=free.cpu_threads;report["elevated"]=elevated();
  if(!free.cpu_threads||free.ram_mb<256||free.scratch_mb<128)throw std::runtime_error("Insufficient detected resources");
  auto sample=random_hex(32);if(unprotect(protect(sample))!=sample)throw std::runtime_error("Credential protection round-trip failed");report["dpapi"]=true;
  auto journalPath=root/("doctor-journal-"+random_hex(8)+".db");
  {Store journal(journalPath);if(!journal.reserve("probe","probe","test"))throw std::runtime_error("SQLite probe failed");journal.result("probe","ok");}
  {Store reopened(journalPath);if(reopened.pending().size()!=1)throw std::runtime_error("SQLite durable outbox probe failed");}
  std::filesystem::remove(journalPath);report["journal"]=true;
  auto temp=root/("doctor-"+random_hex(8)+".tmp");atomic_write(temp,"organizer-worker-doctor");auto hash=sha256_file(temp);std::filesystem::remove(temp);if(hash!="c8a0ad4d2ca29226378ae04b3d70d774c050ac4a73e458155d83b02430cc4733")throw std::runtime_error("SHA-256 probe failed");report["atomic_storage"]=true;
  report["runtime_healthy"]=true;
  if(std::filesystem::exists(root/"worker.json")){
   auto c=load_config(root);report["configuration_valid"]=true;
   if(bind_journal){
    std::optional<std::string> proven;ProtectedPairingSecrets identity(root);
    if(auto credential=identity.read(SecretSlot::credential))proven=text(parse(*credential).as_object(),"coordinator_url",1024);
    Store journal(root/"journal.sqlite3");bind_journal_origin(journal,c.coordinator,proven);report["journal_endpoint_bound"]=true;
   }
   if(probe){auto p=probe_ffmpeg(c);report["capabilities"]={{"conversion.video.h264",p.video},{"conversion.image.jpeg",p.image},{"inference",false},{"gpu",false},{"reason",p.reason}};report["processing_ready"]=p.video||p.image;}
   if(embedding&&(free.ram_mb<768||c.budget.ram_mb<768)){report["embedding"]={{"deferred",true},{"reason","Text CPU probe needs 768 MB in both the shared budget and current free memory"}};}
   else if(embedding){bool media=free.ram_mb>=2048&&c.budget.ram_mb>=2048;auto p=probe_embedding(c,{},media);report["embedding"]={{"text",p.text},{"image",p.media},{"audio",p.audio},{"video",p.video},{"media_deferred",!media},{"reason",p.reason}};if(!p.text||(media&&!c.ffmpeg.empty()&&!p.media))throw std::runtime_error(p.reason);}
   if(network){if(c.coordinator.empty())throw std::runtime_error("No coordinator configured");Http h;auto r=h.request("GET",c.coordinator+"/v1/worker/protocol");report["coordinator_http_status"]=r.status;if(r.status!=200)throw std::runtime_error("Lightsail worker endpoint is not ready");auto body=parse(r.body);if(number(body.as_object(),"protocol",1)!=1)throw std::runtime_error("Incompatible coordinator");report["coordinator_ready"]=true;}
  }else report["configuration_valid"]=false;
  std::cout<<json::serialize(report)<<'\n';return 0;
 }catch(const std::exception& e){report["error"]=e.what();std::cout<<json::serialize(report)<<'\n';return 1;}
}
