#include "ow/adaptive_evidence.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="source"){
  std::wcout<<ow::adaptive::tuning_source_filter();return 0;
 }
 if(argc!=3)return 2;
 std::ifstream file(argv[2],std::ios::binary);if(!file)return 2;
 std::string log{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
 return ow::adaptive::tuning_output_valid(log,argv[1])?0:1;
}
