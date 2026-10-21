#include "offline_media.hpp"
#include "owner_state.hpp"
#include <iostream>
namespace ow::win {void headless_bootstrap_acceptance(const std::filesystem::path&,const std::string&);}
int wmain(int argc,wchar_t** argv){using namespace ow;using namespace ow::win;try{if(argc!=4)throw std::runtime_error("Usage: isolated-unattended-fixture NEW_ROOT APPROVED_FFMPEG SHA256");require_normal_owner();Config c;c.root=argv[1];c.ffmpeg=argv[2];c.ffmpeg_sha256=utf8(argv[3]);headless_bootstrap_acceptance(c.ffmpeg,c.ffmpeg_sha256);std::cout<<json::serialize(OfflineMediaHarness::control_acceptance(std::move(c),true))<<"\n";return 0;}catch(const std::exception& e){try{if(argc==4)atomic_write(std::filesystem::path(std::wstring(argv[1])+L".fixture-failure.txt"),e.what());}catch(...){}std::cerr<<"UNATTENDED_FIXTURE_REJECTED: "<<e.what()<<"\n";return 1;}}
