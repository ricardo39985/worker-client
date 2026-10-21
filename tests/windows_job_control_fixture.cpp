#include "offline_media.hpp"
#include <iostream>
int wmain(int argc,wchar_t** argv){
 using namespace ow;using namespace ow::win;
 try{
  if(argc!=4)throw std::runtime_error("Usage: ow_job_control_fixture NEW_ROOT APPROVED_FFMPEG SHA256");
  if(elevated())throw std::runtime_error("Acceptance controller must run under LIMITED owner token");
  DWORD session{};require(ProcessIdToSessionId(GetCurrentProcessId(),&session),"Read fixture session");if(!session)throw std::runtime_error("Acceptance controller requires owner interactive session");
  Config config;config.root=argv[1];config.ffmpeg=argv[2];config.ffmpeg_sha256=utf8(argv[3]);auto result=OfflineMediaHarness::control_acceptance(std::move(config));std::cout<<json::serialize(result)<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FIXTURE_REJECTED: "<<e.what()<<'\n';return 1;}
}
