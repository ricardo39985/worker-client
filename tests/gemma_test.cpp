#include "test.hpp"
#include "ow/gemma.hpp"
using namespace ow;
namespace {
json::object fixture(){
 json::array vector;vector.emplace_back(1.0);for(int n=1;n<768;++n)vector.emplace_back(0.0);
 return {{"schema",2},{"profile",gemma::profile},{"source_sha256",std::string(64,'a')},{"modality","video"},{"duration_ms",25000},
  {"entries",json::array{json::object{{"modality","video"},{"start_ms",0},{"end_ms",20000},{"vector",vector}},
   json::object{{"modality","video"},{"start_ms",20000},{"end_ms",25000},{"vector",vector}}}}};
}
}
TEST("Gemma accepts complete full-duration normalized section results"){
 auto doc=fixture();gemma::validate_result(doc,std::string(64,'a'),"video");
}
TEST("Gemma rejects source changes partial coverage and nonunit vectors"){
 auto doc=fixture();THROWS(gemma::validate_result(doc,std::string(64,'b'),"video"));
 doc.at("entries").as_array().pop_back();THROWS(gemma::validate_result(doc,std::string(64,'a'),"video"));
 doc=fixture();doc.at("entries").as_array()[1].as_object()["start_ms"]=21000;THROWS(gemma::validate_result(doc,std::string(64,'a'),"video"));
 doc=fixture();doc.at("entries").as_array()[0].as_object().at("vector").as_array()[0]=0.5;THROWS(gemma::validate_result(doc,std::string(64,'a'),"video"));
}
