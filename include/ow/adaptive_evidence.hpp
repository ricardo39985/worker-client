#pragma once
#include "ow/adaptive_hardware.hpp"
#include <boost/json.hpp>
#include <regex>
namespace ow::adaptive {
inline constexpr std::uint32_t tuning_width=1920,tuning_height=1080;
inline std::wstring tuning_source_filter(){
 return L"testsrc2=size="+std::to_wstring(tuning_width)+L"x"+std::to_wstring(tuning_height)+L":rate=24";
}
// Encode success alone is insufficient: the decoded stream must match the
// expected codec, geometry, complete frame count, container and audio.
inline bool video_geometry_valid(const std::string& log,const std::string& codec,
                                  std::uint32_t width,std::uint32_t height){
 if(!width||!height)return false;
 const std::regex stream("Video: "+codec+"[^\\r\\n]*\\b"+std::to_string(width)+"x"+
                         std::to_string(height)+"\\b");
 return std::regex_search(log,stream);
}
inline bool tuning_output_valid(const std::string& log,const std::string& codec){
 const std::regex frames(R"(frame=\s*72(?:\s|$))");
 return video_geometry_valid(log,codec,tuning_width,tuning_height)&&std::regex_search(log,frames)&&
   log.find("Input #0, mov,mp4")!=std::string::npos&&log.find("Audio: aac")!=std::string::npos;
}
inline boost::json::object profile_record(const Profile& p){
 return {{"device_id",p.stable_id},{"driver_version",p.driver_version},{"ffmpeg_sha256",p.ffmpeg_sha256},
  {"backend",static_cast<int>(p.backend)},{"codec",static_cast<int>(p.codec)},
  {"cpu_threads",p.cpu_threads},{"ram_mb",p.ram_mb},{"vram_mb",p.vram_mb},{"scratch_mb",p.scratch_mb},
  {"physical_id",p.physical_id},{"device_name",p.device_name},
  {"device_encode_sessions",p.backend==Backend::cpu?0:device_encode_sessions},{"worker_nvenc_sessions",worker_nvenc_sessions},
  {"max_concurrency",p.max_concurrency},{"measured_fps",p.measured_frames_per_second},
  {"adapter_index",p.adapter_index},{"backend_index",p.backend_index},{"backend_identity_verified",p.backend_identity_verified},
  {"max_width",p.max_width},{"max_height",p.max_height},
  {"measured_width",p.measured_width},{"measured_height",p.measured_height},
  {"geometry_source",p.backend==Backend::cpu?"cpu_unrestricted":"conservative_backend_ceiling"}};
}
inline boost::json::object tuning_record(const TuningEvidence& t){
 return {{"device_id",t.stable_id},{"backend",static_cast<int>(t.backend)},{"codec",static_cast<int>(t.codec)},
  {"measured_fps",t.frames_per_second},{"measured_width",t.measured_width},{"measured_height",t.measured_height},
  {"cpu_threads",t.cpu_threads},{"ram_mb",t.ram_mb},{"vram_mb",t.vram_mb},{"scratch_mb",t.scratch_mb},
  {"tested_concurrency",t.tested_concurrency}};
}
}
