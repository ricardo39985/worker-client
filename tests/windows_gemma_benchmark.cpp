#include "gemma.hpp"
#include <iostream>
using namespace ow;using namespace ow::win;
int wmain(int argc,wchar_t** argv){
 std::filesystem::path workspace;
 try{
  if(argc!=3||elevated())throw std::runtime_error("Normal-owner benchmark requires configured root and reviewed adapter path");
  auto runtime=load_gemma(argv[1]);if(!runtime)throw std::runtime_error("Provisioned runtime required");
  runtime->script=std::filesystem::absolute(argv[2]);runtime->script_hash=sha256_file(runtime->script);
  workspace=std::filesystem::path(argv[1])/"probes"/("resident-benchmark-"+random_hex(8));std::filesystem::create_directories(workspace);
  const Resources limits{GemmaQuery::memory_mb,0,256,8};json::array cold,warm;
  for(const auto* text:{"tomato","eggs","pasta"}){atomic_write(workspace/"source.bin",text);const auto start=monotonic_ms();run_gemma(*runtime,{},workspace,"text",[]{return false;},limits,Priority::normal,false,true);cold.emplace_back(monotonic_ms()-start);}
  Tick startup=0;
  {
   const auto start=monotonic_ms();GemmaQuery model(*runtime,workspace,limits,Priority::normal,[]{return false;});startup=monotonic_ms()-start;
   for(const auto* text:{"tomato","eggs","pasta","motorcycle","blue bicycle"}){atomic_write(workspace/"source.bin",text);const auto began=monotonic_ms();model.encode(workspace,[]{return false;},Priority::normal);warm.emplace_back(monotonic_ms()-began);}
  }
  std::error_code error;std::filesystem::remove_all(workspace,error);
  std::cout<<json::serialize(json::object{{"ok",true},{"cold_ms",cold},{"resident_startup_ms",startup},{"resident_query_ms",warm},{"memory_cap_mb",GemmaQuery::memory_mb},{"threads",8}})<<'\n';return 0;
 }catch(const std::exception& e){std::error_code error;if(!workspace.empty())std::filesystem::remove_all(workspace,error);std::cerr<<e.what()<<'\n';return 1;}
}
