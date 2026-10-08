#pragma once
#include "worker.hpp"
namespace ow::win {
struct EmbeddingProbe {bool text{},media{},audio{},video{};std::string reason;};
EmbeddingProbe probe_embedding(const Config&,const std::function<bool()>& cancelled={},bool include_media=true);
void execute_embedding(const Config&,const JobSpec&,const std::filesystem::path&,Priority,const std::function<bool()>&,const std::function<void(const char*)>&);
}
