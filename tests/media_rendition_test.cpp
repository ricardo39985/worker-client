#include "test.hpp"
#include "ow/media_renditions.hpp"
#include <algorithm>
namespace { const ow::VideoTiming cfr{1,30,{0,1,2},{}}; }
TEST("app video rendition commands preserve dimensions and frame timing and copy existing AAC") {
 auto plan=ow::rendition_plan(true,true,"workspace");
 CHECK(plan.size()==3);
 CHECK(plan[0].role=="feed" && plan[1].role=="feed_optimized" && plan[2].role=="thumbnail");
 for(std::size_t i=0;i<2;++i){
  auto& args=plan[i].arguments;
  CHECK(std::find(args.begin(),args.end(),L"-vf")==args.end());
  CHECK(std::find(args.begin(),args.end(),L"-noautorotate")!=args.end());
  CHECK(std::find(args.begin(),args.end(),L"passthrough")!=args.end());
  CHECK(std::find(args.begin(),args.end(),L"copy")!=args.end());
  for(const auto& a:args){CHECK(a.find(L"https:")==std::wstring::npos);CHECK(a!=L"cmd.exe");}
 }
 CHECK(std::find(plan[1].arguments.begin(),plan[1].arguments.end(),L"hvc1")!=plan[1].arguments.end());
}
TEST("app image renders a full-size WebP and bounded poster with independent destinations") {
 auto plan=ow::rendition_plan(false,false,"workspace");
 CHECK(plan.size()==2 && plan[0].role=="feed" && plan[1].role=="thumbnail");
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-vf")==plan[0].arguments.end());
 CHECK(std::find(plan[1].arguments.begin(),plan[1].arguments.end(),L"-vf")!=plan[1].arguments.end());
 CHECK(plan[0].filename!=plan[1].filename);
 CHECK(plan[0].mime=="image/webp" && plan[1].mime=="image/webp");
}
TEST("non-AAC audio uses the existing app delivery bitrate") {
 auto plan=ow::rendition_plan(true,false,"workspace");
 for(std::size_t i=0;i<2;++i)CHECK(std::find(plan[i].arguments.begin(),plan[i].arguments.end(),L"160k")!=plan[i].arguments.end());
}

TEST("common SDR H264 and HEVC use device frames on the selected NVENC device") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;gpu.backend_index=3;
 for(const auto& source:{std::string("h264 (High) (avc1 / 0x31637661), yuv420p(tv, bt709, progressive), 3840x2160"),
                         std::string("hevc (Main) (hvc1 / 0x31637668), yuv420p(tv, bt709, progressive), 1920x1080")}) {
  for(const auto codec:{ow::adaptive::Codec::h264,ow::adaptive::Codec::hevc}){
   gpu.codec=codec;
   auto plan=ow::rendition_plan(true,false,"workspace",std::vector<ow::adaptive::Profile>{gpu},"Input #0, mov:\n  Stream #0:0: Video: "+source+"\nOutput #0, null:\n",&cfr);
   const auto& args=plan[codec==ow::adaptive::Codec::h264?0:1].arguments;
   auto input=std::find(args.begin(),args.end(),L"-i");auto device=std::find(args.begin(),args.end(),L"-hwaccel_device");
   CHECK(device<input && *(device+1)==L"3");
   auto format=std::find(args.begin(),args.end(),L"-hwaccel_output_format");CHECK(format<input && *(format+1)==L"cuda");
   auto pix=std::find(args.begin(),args.end(),L"-pix_fmt");CHECK(pix!=args.end() && *(pix+1)==L"cuda");
   CHECK(std::find(args.begin(),args.end(),L"-vf")==args.end());
   for(const auto& a:args){CHECK(a.find(L"hwdownload")==std::wstring::npos);CHECK(a.find(L"format=nv12")==std::wstring::npos);}
  }
 }
}
TEST("unproven source formats retain the original software decode and encoding options") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 const std::vector<ow::adaptive::Profile> profiles{gpu};const auto original=ow::rendition_plan(true,false,"workspace",profiles);
 for(const auto source:{"", "h264 (High 10), yuv420p10le, 1920x1080", "hevc (Rext), yuv422p, 1920x1080",
   "hevc (Main 10), yuv420p10le(tv, bt2020nc/smpte2084), 3840x2160", "h264 (High), yuv420p(tv, bt2020nc), 1920x1080",
   "hevc (Main), yuv420p(tv, arib-std-b67), 1920x1080", "vp9 (Profile 0), yuv420p, 1920x1080",
   "h264 (High), yuv444p, 1920x1080", "h264 (High), yuv420p(top first), 1920x1080", "h264 (High), yuv420p(top coded first (swapped)), 1920x1080", "h264 (High), yuv420p(bottom coded first (swapped)), 1920x1080"}) {
  auto plan=ow::rendition_plan(true,false,"workspace",profiles,std::string("Input #0, mov:\n  Stream #0:0: Video: ")+source+"\n");
  CHECK(plan[0].arguments==original[0].arguments);CHECK(plan[1].arguments==original[1].arguments);
 }
}
TEST("decode selection inspects the first input video only and leaves other backends and posters unchanged") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 auto plan=ow::rendition_plan(true,false,"workspace",std::vector<ow::adaptive::Profile>{gpu},"Input #0, mov:\n Stream #0:0: Video: vp9 (Profile 0), yuv420p, 1920x1080\n Stream #0:1: Video: h264 (High), yuv420p, 1920x1080\n");
 CHECK(plan[0].arguments==ow::rendition_plan(true,false,"workspace",gpu)[0].arguments);
 for(const auto backend:{ow::adaptive::Backend::cpu,ow::adaptive::Backend::intel_qsv,ow::adaptive::Backend::amd_amf}){
  gpu.backend=backend;
  auto original=ow::rendition_plan(true,false,"workspace",gpu);
  auto with_info=ow::rendition_plan(true,false,"workspace",std::vector<ow::adaptive::Profile>{gpu},"Input #0, mov:\n Stream #0:0: Video: h264 (High), yuv420p, 1920x1080\n");
  for(unsigned i=0;i<3;++i)CHECK(original[i].arguments==with_info[i].arguments);
 }
}

TEST("output descriptions and metadata cannot qualify an unknown input for CUDA decode") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 const std::vector<ow::adaptive::Profile> profiles{gpu};auto original=ow::rendition_plan(true,false,"workspace",profiles);
 for(const auto log:{"Input #0, mov:\n Metadata:\n  title: Video: h264 (High), yuv420p, 1920x1080\n Stream #0:0: Video: vp9 (Profile 0), yuv420p, 1920x1080\n",
  "Input #0, mov:\n Stream #0:0: Audio: aac\nOutput #0, mp4:\n Stream #0:0: Video: h264 (High), yuv420p, 1920x1080\n"}) {
  CHECK(ow::rendition_plan(true,false,"workspace",profiles,log)[0].arguments==original[0].arguments);
 }
}
TEST("device decode preserves every existing output encoder option") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 for(const auto codec:{ow::adaptive::Codec::h264,ow::adaptive::Codec::hevc}){
  gpu.codec=codec;const std::vector<ow::adaptive::Profile> profiles{gpu};
  auto before=ow::rendition_plan(true,true,"workspace",profiles,"",&cfr);
  auto after=ow::rendition_plan(true,true,"workspace",profiles,"Input #0, mov:\n Stream #0:0[0x1](und): Video: h264 (High), yuv420p(tv, bt709, progressive), 1920x1080\n",&cfr);
  const unsigned index=codec==ow::adaptive::Codec::h264?0:1;auto& args=after[index].arguments;
  for(const auto flag:{L"-hwaccel",L"-hwaccel_device",L"-hwaccel_output_format"}){auto p=std::find(args.begin(),args.end(),flag);CHECK(p!=args.end());args.erase(p,p+2);}
  auto pix=std::find(args.begin(),args.end(),L"-pix_fmt");*(pix+1)=L"yuv420p";
  for(unsigned i=0;i<3;++i)CHECK(after[i].arguments==before[i].arguments);
 }
}

TEST("input inspection can produce the identical fixed poster from the same decode") {
 auto args=ow::rendition_inspection_arguments("workspace",true);
 auto ordinary=ow::rendition_inspection_arguments("workspace",false);
 CHECK(std::count(args.begin(),args.end(),L"-i")==1);
 CHECK(std::find(args.begin(),args.end(),(std::filesystem::path("workspace")/"source.bin").wstring())!=args.end());
 const auto poster=ow::rendition_plan(true,false,"workspace").back().arguments;
 auto output=std::find(poster.begin(),poster.end(),L"-frames:v");
 auto expected=ordinary;expected.insert(expected.begin(),{L"-filter_threads",L"1"});expected.insert(expected.end(),output,poster.end());
 CHECK(args==expected);
}

TEST("poster reuse is restricted to a single ordinary unrotated SDR video stream") {
 const std::string source="Input #0, mov:\n Stream #0:0: Video: h264 (High), yuv420p(tv, bt709, progressive), 1920x1080\n";
 CHECK(ow::inspection_poster_reusable(source));
 for(const auto extra:{" Side data:\n  displaymatrix: rotation of -90.00 degrees\n", " Metadata:\n  rotate: 180\n", " Stream #0:2: Video: h264 (High), yuv420p, 640x480\n"})
  CHECK(!ow::inspection_poster_reusable(source+extra));
 CHECK(!ow::inspection_poster_reusable("Input #0, mov:\n Stream #0:0: Video: hevc (Main 10), yuv420p10le, 1920x1080\n"));
}

TEST("output with changed rate or dropped duplicated retimed frames is rejected") {
 const auto source=ow::video_timing("#tb 0: 1/30000\n0, 0, 0, 1001, 9, x\n0, 1001, 1001, 1001, 9, x\n0, 2002, 2002, 1001, 9, x\n");
 CHECK(source && source->constant_rate());
 CHECK(ow::same_video_timing(*source,*source));
 auto wrong_nominal=*source;wrong_nominal.reported_rate="25";CHECK(!ow::same_video_timing(*source,wrong_nominal));
 for(const auto bad:{"#tb 0: 1/30\n0, 0, 0, 1, 9, x\n0, 1, 1, 1, 9, x\n0, 2, 2, 1, 9, x\n",
  "#tb 0: 1/30000\n0, 0, 0, 1001, 9, x\n0, 2002, 2002, 1001, 9, x\n"}){
  auto output=ow::video_timing(bad);CHECK(output);CHECK(!ow::same_video_timing(*source,*output));
 }
 CHECK(!ow::video_timing("#tb 0: 0/0\n"));
 CHECK(!ow::video_timing("#tb 0: 1/30\n0, 0, 0, 1, 9, x\n0, 1, 0, 1, 9, x\n"));
}
TEST("variable frame timing keeps all source timestamps and requires software decode") {
 const auto vfr=ow::video_timing("Input #0, mov:\n Stream #0:0: Video: h264 (High), yuv420p, 1920x1080, 30 tbr, 1k tbn\nOutput #0, framehash:\n#tb 0: 1/90000\n0, 0, 0, 3000, 9, x\n0, 3000, 3000, 3001, 9, x\n0, 6001, 6001, 3000, 9, x\n");
 CHECK(vfr && !vfr->constant_rate());
 CHECK(ow::same_video_timing(*vfr,*vfr));
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 auto plan=ow::rendition_plan(true,false,"workspace",std::vector<ow::adaptive::Profile>{gpu},"Input #0, mov:\n Stream #0:0: Video: h264 (High), yuv420p, 1920x1080\n",&*vfr);
 CHECK(!plan[0].hardware);
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-hwaccel")==plan[0].arguments.end());
}

TEST("timing inspection tolerates B frame packet order and equivalent time bases") {
 auto a=ow::video_timing("#tb 0: 1/30\n0, -1, 0, 1, 1, x\n0, 0, 2, 1, 1, x\n0, 1, 1, 1, 1, x\n");
 auto b=ow::video_timing("#tb 0: 1/90000\n0, 0, 0, 3000, 1, x\n0, 3000, 3000, 3000, 1, x\n0, 6000, 6000, 3000, 1, x\n");
 CHECK(a&&b&&ow::same_video_timing(*a,*b));
 auto unknown=*a;unknown.pts={0,1,3};unknown.reported_rate="29.97";CHECK(!unknown.rate());
}
TEST("CUDA decode requires complete constant rate evidence") {
 ow::adaptive::Profile gpu;gpu.backend=ow::adaptive::Backend::nvidia_nvenc;
 auto plan=ow::rendition_plan(true,false,"workspace",std::vector<ow::adaptive::Profile>{gpu},"Input #0, mov:\n Stream #0:0: Video: h264 (High), yuv420p, 1920x1080\n");
 CHECK(plan[0].hardware);
 CHECK(std::find(plan[0].arguments.begin(),plan[0].arguments.end(),L"-hwaccel")==plan[0].arguments.end());
}
