#pragma once
#include "ow/adaptive_hardware.hpp"
#include "ow/device_decode.hpp"
#include "ow/video_timing.hpp"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
namespace ow {
struct Rendition {
 std::string role,mime,filename;
 std::vector<std::wstring> arguments;
 unsigned timeout_seconds;
 bool hardware{};
};
// This is an installed typed app profile, never command text from the network.
inline std::vector<Rendition> rendition_plan(bool video,bool copy_audio,const std::filesystem::path& workspace) {
 std::vector<Rendition> plan;
 auto base=[&] {
  return std::vector<std::wstring>{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-protocol_whitelist",L"file",L"-format_whitelist",L"mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe",L"-threads",L"1",L"-filter_threads",L"1"};
 };
 if(video)for(bool hevc:{false,true}) {
  auto args=base();
  const std::vector<std::wstring> encoding={L"-noautorotate",L"-i",(workspace/"source.bin").wstring(),L"-map",L"0:v:0",L"-map",L"0:a:0?",L"-c:v",hevc?L"libx265":L"libx264",L"-preset",hevc?L"fast":L"veryfast",L"-crf",hevc?L"26":L"23",L"-pix_fmt",L"yuv420p",L"-threads",L"1"};
  args.insert(args.end(),encoding.begin(),encoding.end());
  if(hevc)args.insert(args.end(),{L"-tag:v",L"hvc1",L"-x265-params",L"pools=none:frame-threads=1:rc-lookahead=4:bframes=2:ref=2:log-level=error"});
  else args.insert(args.end(),{L"-x264-params",L"rc-lookahead=4:sync-lookahead=0:ref=2"});
  args.insert(args.end(),{L"-c:a",copy_audio?L"copy":L"aac"});
  if(!copy_audio)args.insert(args.end(),{L"-b:a",L"160k"});
  auto file=hevc?"optimized.mp4":"feed.mp4";
  args.insert(args.end(),{L"-fps_mode",L"passthrough",L"-enc_time_base:v",L"demux",L"-movflags",L"+faststart",(workspace/file).wstring()});
  plan.push_back({hevc?"feed_optimized":"feed","video/mp4",file,std::move(args),hevc?240u:180u});
 }
 for(bool poster:{false,true}) {
  if(video&&!poster)continue;
  auto args=base();args.insert(args.end(),{L"-i",(workspace/"source.bin").wstring(),L"-frames:v",L"1"});
  if(poster)args.insert(args.end(),{L"-vf",L"scale=w='min(480,iw)':h='min(480,ih)':force_original_aspect_ratio=decrease"});
  auto file=poster?"poster.webp":"feed.webp";
  args.insert(args.end(),{L"-c:v",L"libwebp",L"-quality",poster?L"82":L"85",L"-threads",L"1",(workspace/file).wstring()});
  plan.push_back({poster?"thumbnail":"feed","image/webp",file,std::move(args),poster?30u:180u});
 }
 return plan;
}
// The ordinary inspection remains a one-frame software decode. Optional poster
// output reuses that frame, without opening another hardware encoder session.
inline std::vector<std::wstring> rendition_inspection_arguments(const std::filesystem::path& workspace,bool prepare_poster=false){
 auto args=std::vector<std::wstring>{L"-nostdin",L"-hide_banner",L"-v",L"info",L"-protocol_whitelist",L"file",L"-noautorotate",L"-i",(workspace/"source.bin").wstring(),L"-map",L"0:v:0",L"-frames:v",L"1",L"-f",L"null",L"-"};
 if(prepare_poster){const auto poster=rendition_plan(true,false,workspace).back().arguments;
  args.insert(args.begin(),{L"-filter_threads",L"1"});
  args.insert(args.end(),std::find(poster.begin(),poster.end(),L"-frames:v"),poster.end());}
 return args;
}
inline std::vector<Rendition> rendition_plan(bool video,bool copy_audio,const std::filesystem::path& workspace,
                                               const std::vector<adaptive::Profile>& profiles, const std::string& source_info={},const VideoTiming* timing=nullptr) {

 auto plan=rendition_plan(video,copy_audio,workspace);
 if(timing){
  const auto rate=timing->rate();
  if(!rate)throw std::runtime_error("Exact source video frame rate unavailable");
  // libx265 otherwise signals the average filter rate for irregular sources,
  // even with exact PTS. Set its VUI rate without resampling the frame stream.
  auto& args=plan[1].arguments;auto params=std::find(args.begin(),args.end(),L"-x265-params");
  if(params!=args.end())*(params+1)+=L":fps="+std::to_wstring(rate->first)+L"/"+std::to_wstring(rate->second);
 }
 for(auto& rendition:plan){
  if(rendition.role!="feed"&&rendition.role!="feed_optimized")continue;
  const auto codec=rendition.role=="feed_optimized"?adaptive::Codec::hevc:adaptive::Codec::h264;
  const adaptive::Profile* cpu_profile=nullptr;
  for(const auto& p:profiles)if(p.codec==codec&&p.backend==adaptive::Backend::cpu&&p.cpu_threads&&
    (!cpu_profile||p.measured_frames_per_second>cpu_profile->measured_frames_per_second))cpu_profile=&p;
  auto selected=profiles.end();
  for(auto i=profiles.begin();i!=profiles.end();++i)
   if(i->codec==codec&&i->backend!=adaptive::Backend::cpu&&
      (selected==profiles.end()||i->measured_frames_per_second>selected->measured_frames_per_second))selected=i;
  if(selected==profiles.end()){
   if(cpu_profile){const auto threads=std::to_wstring(cpu_profile->cpu_threads);auto& args=rendition.arguments;
    for(auto flag:{L"-threads",L"-filter_threads"}){auto i=std::find(args.begin(),args.end(),flag);if(i!=args.end()&&i+1!=args.end())*(i+1)=threads;}}
   continue;
  }
  auto& args=rendition.arguments;
  rendition.hardware=true;
  const auto input=std::find(args.begin(),args.end(),L"-i");
  if(selected->backend==adaptive::Backend::intel_qsv){
   args.insert(input,{L"-init_hw_device",L"qsv=ow_qsv:hw_any,child_device="+std::to_wstring(selected->adapter_index)+L",child_device_type=d3d11va",L"-filter_hw_device",L"ow_qsv"});
  }else if(selected->backend==adaptive::Backend::amd_amf){
   args.insert(input,{L"-init_hw_device",L"d3d11va=ow_d3d:"+std::to_wstring(selected->adapter_index),L"-init_hw_device",L"amf=ow_amf@ow_d3d",L"-filter_hw_device",L"ow_amf"});
   args.insert(std::find(args.begin(),args.end(),L"-map")+4,{L"-vf",L"vpp_amf"});
  }
  const auto encoder_position=static_cast<std::size_t>(std::distance(args.begin(),std::find(args.begin(),args.end(),L"-c:v"))+1);
  auto encoder=args.begin()+static_cast<std::ptrdiff_t>(encoder_position);
  if(selected->backend==adaptive::Backend::nvidia_nvenc){
   *encoder=codec==adaptive::Codec::hevc?L"hevc_nvenc":L"h264_nvenc";
   args.erase(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),args.begin()+static_cast<std::ptrdiff_t>(encoder_position+3));
   args.insert(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),{L"-gpu",std::to_wstring(selected->backend_index),L"-preset",L"p4",L"-rc",L"vbr",L"-cq",L"24",L"-b:v",L"0",L"-rc-lookahead",L"0"});
  }else if(selected->backend==adaptive::Backend::intel_qsv){
   *encoder=codec==adaptive::Codec::hevc?L"hevc_qsv":L"h264_qsv";
   args.erase(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),args.begin()+static_cast<std::ptrdiff_t>(encoder_position+3));
   args.insert(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),{L"-global_quality",L"24",L"-look_ahead",L"0",L"-bf",L"2"});
  }else{
   *encoder=codec==adaptive::Codec::hevc?L"hevc_amf":L"h264_amf";
   args.erase(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),args.begin()+static_cast<std::ptrdiff_t>(encoder_position+3));
   args.insert(args.begin()+static_cast<std::ptrdiff_t>(encoder_position+1),{L"-quality",L"speed",L"-rc",L"cqp",L"-qp_i",L"24",L"-qp_p",L"26"});
  }
  if(selected->backend==adaptive::Backend::nvidia_nvenc&&timing&&timing->constant_rate()&&cuda_sdr_decode_eligible(source_info)){
   // Keep decoded frames on the same CUDA device as this reserved encoder.
   // The underlying format is the inspected 8-bit 4:2:0; cuda is a frame type.
   args.insert(std::find(args.begin(),args.end(),L"-i"),{L"-hwaccel",L"cuda",L"-hwaccel_device",std::to_wstring(selected->backend_index),L"-hwaccel_output_format",L"cuda"});
   auto pix=std::find(args.begin(),args.end(),L"-pix_fmt");*(pix+1)=L"cuda";
  }
  auto crf=std::find(args.begin(),args.end(),L"-crf");if(crf!=args.end())args.erase(crf,crf+2);
  auto cpu_params=std::find(args.begin(),args.end(),codec==adaptive::Codec::hevc?L"-x265-params":L"-x264-params");
  if(cpu_params!=args.end())args.erase(cpu_params,cpu_params+2);
 }
 return plan;
}
inline std::vector<Rendition> rendition_plan(bool video,bool copy_audio,const std::filesystem::path& workspace,
                                               const adaptive::Profile& profile) {
 return rendition_plan(video,copy_audio,workspace,std::vector<adaptive::Profile>{profile});
}
}
