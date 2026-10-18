#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace ow {
inline bool insufficient_child_budget(std::string_view reason){return reason.starts_with("insufficient child memory budget");}
inline std::optional<std::string> child_memory_failure(std::string detail,std::uint32_t width,std::uint32_t height,std::uint64_t budget_mb){
 if(!budget_mb||!width||!height)return {};
 std::transform(detail.begin(),detail.end(),detail.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 bool allocation=false;
 for(std::string_view marker:{"out of memory","cannot allocate memory","cuda_error_out_of_memory","allocation failed","failed to allocate","std::bad_alloc","insufficient memory"})
  allocation=allocation||detail.find(marker)!=std::string::npos;
 if(!allocation)return {}; // Heap corruption alone is not allocation evidence.
 return "insufficient child memory budget: "+std::to_string(width)+"x"+std::to_string(height)+" budget="+std::to_string(budget_mb)+" MiB (allocation failure under cap)";
}
}
