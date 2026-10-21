// Disposable real controller: no GUI, converter, credentials or coordinator.
#include "worker.hpp"
#include "control_pipe.hpp"
#include <iostream>
int wmain(int argc,wchar_t** argv){
 using namespace ow;using namespace ow::win;
 if(argc!=2)return 2;std::filesystem::path root=argv[1];
 if(!root.is_absolute()||std::filesystem::exists(root))return 2;
 try{
  std::filesystem::create_directories(root);auto doc=control_defaults();doc["operator"].as_object()["paused"]=true;atomic_write(root/"worker.json",json::serialize(doc));
  Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock)fail("Fixture lock");
  Worker worker(load_config(root));ControlPipe pipe(root,[&](const json::object& r){return worker.control(r);});std::cout<<"FIXTURE: no coordinator; not proof of production connection\n";
  const auto deadline=monotonic_ms()+30000;while(!worker.ready_to_exit()&&monotonic_ms()<deadline)Sleep(50);return worker.ready_to_exit()?0:3;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
