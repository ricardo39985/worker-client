#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace ow {
inline std::string observed_count(std::optional<std::uint64_t> value){return value?std::to_string(*value):"unknown";}
}
namespace ow::win {
enum class Priority { maximum, normal, low, backup };
inline std::string priority_label(Priority priority){
 switch(priority){case Priority::maximum:return "Maximum";case Priority::normal:return "Normal";
  case Priority::low:return "Low";case Priority::backup:return "Backup (routing hint)";}
 return "Unknown";
}
}
