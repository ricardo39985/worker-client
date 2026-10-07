#pragma once
#include "platform.hpp"
#include "ow/protocol.hpp"
#include <optional>
namespace ow::win {
enum class Priority { maximum, normal, low, backup };
class Child {
 Handle job_,process_;
 Resources resources_{};
public:
 Child(const std::filesystem::path& exe,const std::vector<std::wstring>& args,
       const std::filesystem::path& workspace,Resources resources,Priority priority);
 ~Child();Child(const Child&)=delete;Child& operator=(const Child&)=delete;
 std::optional<DWORD> exit_code();
 void stop();void priority(Priority);
};
}
