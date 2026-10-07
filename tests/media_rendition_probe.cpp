#include "ow/media_renditions.hpp"
#include <iostream>
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 for(const auto& item:ow::rendition_plan(std::string(argv[1])=="video",std::string(argv[2])=="copy",argv[3])){
  std::cout<<item.role<<'\t'<<item.mime<<'\t'<<item.filename;
  for(const auto& argument:item.arguments){std::cout<<'\t';for(auto c:argument){if(c<0||c>127)return 3;std::cout<<static_cast<char>(c);}}
  std::cout<<'\n';
 }
}
