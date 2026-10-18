// Inspection/command construction only: never encodes or writes media.
#include "ow/media_renditions.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  std::ifstream file(argv[1]);
  auto timing=ow::video_timing(std::string(std::istreambuf_iterator<char>(file),{}));
  if(!timing)return 2;
  std::cout<<"samples="<<timing->pts.size()<<" time_base="<<timing->numerator<<'/'<<timing->denominator
           <<" reported_rate="<<timing->reported_rate<<" constant_rate="<<timing->constant_rate()<<'\n';
  auto plan=ow::rendition_plan(true,false,"not-executed",std::vector<ow::adaptive::Profile>{},"",&*timing);
  std::cout<<"planned_renditions="<<plan.size()<<'\n';
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
