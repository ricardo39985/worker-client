#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace ow {
struct Rendition {
 std::string role,mime,filename;
 std::vector<std::wstring> arguments;
 unsigned timeout_seconds;
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
  args.insert(args.end(),{L"-fps_mode",L"passthrough",L"-movflags",L"+faststart",(workspace/file).wstring()});
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
}
