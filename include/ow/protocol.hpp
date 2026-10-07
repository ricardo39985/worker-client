#pragma once
#include "ow/admission.hpp"
#include "ow/json.hpp"
#include <filesystem>
#include <vector>
namespace ow {
struct ArtifactTarget { std::string role,url,mime; };
struct JobSpec {
 Offer offer;
 std::string input_url,input_sha256,output_url;
 std::uint64_t input_bytes{},output_max_bytes{},timeout_ms{};
 std::vector<ArtifactTarget> artifacts;
 bool copy_audio{};
};
Tick lease_deadline(Tick server_utc,Tick expires_utc,Tick local_utc,Tick monotonic_now);
bool allowed_url(const std::string& url,const std::vector<std::string>& hosts);
JobSpec decode_offer(const json::object& message,Tick now,const std::vector<std::string>& hosts);
std::vector<std::wstring> ffmpeg_arguments(const JobSpec&,const std::filesystem::path& workspace);
std::wstring quote_windows_argument(const std::wstring& argument);
std::string output_name(const JobSpec&);
}

