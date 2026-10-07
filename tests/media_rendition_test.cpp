#include "test.hpp"
#include "ow/media_renditions.hpp"
#include <algorithm>
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
