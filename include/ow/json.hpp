#pragma once
#include <boost/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstdint>
namespace ow {
namespace json=boost::json;
inline json::value parse(std::string_view s,std::size_t limit=256*1024){
 if(s.size()>limit)throw std::runtime_error("JSON exceeds permitted size");
 json::parse_options options;options.max_depth=32;
 return json::parse(json::string_view(s.data(),s.size()),{},options);
}
inline std::string text(const json::object& o,const char* key,std::size_t limit=8192){
 auto* v=o.if_contains(key);if(!v||!v->is_string()||v->as_string().size()>limit)throw std::runtime_error(std::string("missing/invalid string: ")+key);
 const auto& s=v->as_string();if(s.find('\0')!=json::string::npos)throw std::runtime_error("NUL is not permitted");
 return {s.data(),s.size()};
}
inline std::uint64_t number(const json::object& o,const char* key,std::uint64_t max){
 auto* v=o.if_contains(key);if(!v)throw std::runtime_error(std::string("missing integer: ")+key);
 std::uint64_t n;if(v->is_uint64())n=v->as_uint64();else if(v->is_int64()&&v->as_int64()>=0)n=static_cast<std::uint64_t>(v->as_int64());else throw std::runtime_error(std::string("invalid integer: ")+key);
 if(n>max)throw std::runtime_error(std::string("integer out of bounds: ")+key);
 return n;
}
}
