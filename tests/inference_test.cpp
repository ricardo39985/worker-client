#include "ow/inference.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
int main(){
 try {
  auto v=ow::normalize_embedding(std::vector<double>(768,2.0));
  double norm=0;for(auto x:v)norm+=x*x;
  if(std::abs(norm-1)>1e-6)throw std::runtime_error("Embedding must be unit length");
  for(auto bad: {std::vector<double>(767,1),std::vector<double>(768,0),std::vector<double>(768,std::numeric_limits<double>::quiet_NaN())}){
   bool rejected=false;try{ow::normalize_embedding(bad);}catch(const std::exception&){rejected=true;}
   if(!rejected)throw std::runtime_error("Invalid model output accepted");
  }
  if(ow::inference_lane("inference.embeddinggemma2.v1")!="inference" || ow::inference_lane("media.video.renditions.v1")!="conversion")throw std::runtime_error("Independent lane mapping failed");
  std::cout<<"Inference output contracts passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
