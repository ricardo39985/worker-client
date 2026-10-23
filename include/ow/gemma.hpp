#pragma once
#include "ow/json.hpp"
#include <cmath>
#include <map>
namespace ow::gemma {
inline constexpr const char* capability="inference.embeddinggemma2.v2";
inline constexpr const char* query_capability="inference.embeddinggemma2.query.v1";
// Independently proven lanes must not inherit an unrelated input's failure.
inline bool capability_available(const std::string& cap,bool enabled,bool media_verified,bool query_verified){
 return enabled&&((cap==capability&&media_verified)||(cap==query_capability&&query_verified));
}
inline bool inference(const std::string& cap){return cap==capability||cap==query_capability;}
inline constexpr const char* profile="embeddinggemma2-q8-b11475-768-segments-v2";
inline constexpr std::size_t result_limit=8*1024*1024;
inline bool modality(const std::string& value){return value=="text"||value=="image"||value=="video"||value=="audio";}
inline void validate_vector(const json::value& value){
 if(!value.is_array()||value.as_array().size()!=768)throw std::runtime_error("Gemma vector dimension differs");
 double norm=0;
 for(const auto& number:value.as_array()){
  if(!number.is_number())throw std::runtime_error("Invalid Gemma vector value");
  const auto n=number.to_number<double>();if(!std::isfinite(n)||std::abs(n)>1.000001)throw std::runtime_error("Invalid Gemma vector value");norm+=n*n;
 }
 if(std::abs(norm-1)>0.0001)throw std::runtime_error("Gemma vector is not normalized");
}
inline void validate_result(const json::object& doc,const std::string& source,const std::string& expected_modality){
 if(doc.size()!=6||number(doc,"schema",2)!=2||text(doc,"profile")!=profile||text(doc,"source_sha256")!=source||text(doc,"modality")!=expected_modality)
  throw std::runtime_error("Gemma result identity differs");
 const auto duration=number(doc,"duration_ms",3600000);const bool timed=expected_modality=="video"||expected_modality=="audio";
 if(timed!=(duration>0))throw std::runtime_error("Gemma duration differs");
 const auto& entries=doc.at("entries").as_array();if(entries.empty()||entries.size()>360)throw std::runtime_error("Gemma section count differs");
 std::map<std::string,std::uint64_t> ends;
 for(const auto& entry:entries){
  const auto& row=entry.as_object();const auto kind=text(row,"modality");
  if(row.size()!=4||(kind!=expected_modality&&!(expected_modality=="video"&&kind=="audio")))throw std::runtime_error("Gemma section modality differs");
  const auto start=number(row,"start_ms",duration),end=number(row,"end_ms",duration);
  if(start!=ends[kind]||end<start||(timed&&(end==start||end-start>20000))||(!timed&&ends.contains(kind)&&entries.size()!=1))throw std::runtime_error("Gemma section coverage differs");
  ends[kind]=end;validate_vector(row.at("vector"));
 }
 if(!ends.contains(expected_modality))throw std::runtime_error("Gemma primary modality missing");
 for(const auto& [_,end]:ends)if(end!=duration)throw std::runtime_error("Gemma section coverage is incomplete");
}
inline void validate_query(const json::object& doc){
 if(doc.size()!=3||number(doc,"schema",2)!=2||text(doc,"profile")!=profile)throw std::runtime_error("Gemma query result differs");
 validate_vector(doc.at("vector"));
}
}
