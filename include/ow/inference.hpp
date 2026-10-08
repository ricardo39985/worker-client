#pragma once
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace ow {
inline constexpr const char* embedding_capability="inference.embeddinggemma2.v1";
inline constexpr const char* embedding_profile="embeddinggemma2-q8-b11475-768-v1";
inline std::string_view inference_lane(std::string_view capability){return (capability==embedding_capability||capability=="local.embedding.probe")?"inference":"conversion";}
inline std::vector<double> normalize_embedding(std::vector<double> values){
 if(values.size()!=768)throw std::runtime_error("Embedding dimension mismatch");
 double norm=0;for(double v:values){if(!std::isfinite(v)||std::abs(v)>1e6)throw std::runtime_error("Invalid embedding value");norm+=v*v;}
 if(!std::isfinite(norm)||norm<=1e-20)throw std::runtime_error("Empty embedding");
 norm=std::sqrt(norm);for(double& v:values)v/=norm;return values;
}
}
