#include "ow/media_renditions.hpp"
#include <iostream>
#include <fstream>
#include <iterator>
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 if(std::string(argv[1])=="validate"){
  auto read=[](const char* path){std::ifstream f(path);return std::string((std::istreambuf_iterator<char>(f)),{});};
  auto source=ow::video_timing(read(argv[2])),output=ow::video_timing(read(argv[3]));
  if(!source||!output)return 2;
  return ow::same_video_timing(*source,*output)?0:1;
 }
 std::ifstream input(std::filesystem::path(argv[3])/"timing.log");
 std::string log((std::istreambuf_iterator<char>(input)),{});auto timing=ow::video_timing(log);
 std::vector<ow::adaptive::Profile> profiles;const std::string mode=argv[1];
 if(mode=="nvenc"||mode=="nvenc-software")for(auto codec:{ow::adaptive::Codec::h264,ow::adaptive::Codec::hevc}){
  ow::adaptive::Profile p;p.backend=ow::adaptive::Backend::nvidia_nvenc;p.backend_index=1;p.codec=codec;profiles.push_back(p);
 }
 for(const auto& item:ow::rendition_plan(mode!="image",std::string(argv[2])=="copy",argv[3],profiles,mode=="nvenc"?log:"",timing?&*timing:nullptr)){
  std::cout<<item.role<<'\t'<<item.mime<<'\t'<<item.filename;
  for(const auto& argument:item.arguments){std::cout<<'\t';for(auto c:argument){if(c<0||c>127)return 3;std::cout<<static_cast<char>(c);}}
  std::cout<<'\n';
 }
}
