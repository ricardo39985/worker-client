#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <numeric>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
namespace ow {
// Packet inspection uses the already approved FFmpeg, without decoding or a new
// executable dependency. PTS are sorted because packet order includes B frames.
struct VideoTiming {
 std::int64_t numerator{},denominator{};
 std::vector<std::int64_t> pts;
 std::string reported_rate;
 // Total demuxed sample duration, independent of B-frame packet order.
 std::int64_t duration_ticks{};
 bool constant_rate() const {
  if(pts.size()<2)return false;
  const auto step=pts[1]-pts[0];
  for(std::size_t i=2;i<pts.size();++i)if(pts[i]-pts[i-1]!=step)return false;
  return step>0;
 }
};
inline std::optional<VideoTiming> video_timing(const std::string& text){
 if(text.size()>5*1024*1024)return std::nullopt;
 VideoTiming out;std::istringstream rows(text);std::string line;bool header=false;
 constexpr std::int64_t bound=1000000000000LL;
 while(std::getline(rows,line)){
  if(line.rfind("#tb 0: ",0)==0){
   if(header)return std::nullopt;
   std::istringstream rate(line.substr(7));char slash{};
   if(!(rate>>out.numerator>>slash>>out.denominator)||slash!='/'||out.numerator<=0||out.denominator<=0||out.numerator>1000000000||out.denominator>1000000000)return std::nullopt;
   header=true;
  }else if(line.rfind("0,",0)==0){
   std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);
   std::int64_t stream{},dts{},pts{},duration{};
   if(!header||!(row>>stream>>dts>>pts>>duration)||pts < -bound||pts>bound||duration<=0||duration>bound||out.pts.size()>=300000)return std::nullopt;
   out.pts.push_back(pts);
   out.duration_ticks+=duration;
  }
 }
 if(!header||out.pts.empty())return std::nullopt;
 // Retain the demuxer's nominal rate for diagnostics only. A human-rounded
 // header is not evidence of the actual frame cadence.
 const auto input=text.find("Input #0,");
 if(input!=std::string::npos){
  const auto end=text.find("Output #",input);const auto header_text=text.substr(input,end==std::string::npos?end:end-input);
  static const std::regex rate(R"(^[ \t]*Stream #0:[0-9]+(?:\[[^\]\r\n]*\])?(?:\([^\)\r\n]*\))?: Video:[^\r\n]*, ([0-9]+(?:\.[0-9]+)?[kM]?) tbr(?:[, ]|$))");std::smatch match;
  std::istringstream header_rows(header_text);std::string row;
  while(std::getline(header_rows,row))if(std::regex_search(row,match,rate)){out.reported_rate=match[1].str();break;}
 }
 std::sort(out.pts.begin(),out.pts.end());
 if(std::adjacent_find(out.pts.begin(),out.pts.end())!=out.pts.end())return std::nullopt;
 const auto common=std::gcd(out.numerator,out.denominator);out.numerator/=common;out.denominator/=common;
 return out;
}
inline bool same_video_timing(const VideoTiming& source,const VideoTiming& output){
 // Nominal/VUI rates describe codec metadata, not the presentation timeline.
 // In particular a rounded tbr must neither veto an encode nor validate one.
 if(source.pts.size()!=output.pts.size()||source.pts.empty()||source.numerator<=0||source.denominator<=0||output.numerator<=0||output.denominator<=0)return false;
 // Compare reduced rational timestamps without floating point or MSVC-specific
 // wide integer arithmetic. Origin shifts by the muxer do not retime frames.
 auto fraction=[](std::int64_t ticks,std::int64_t n,std::int64_t d)->std::optional<std::pair<std::int64_t,std::int64_t>>{
  auto g=std::gcd(ticks,d);ticks/=g;d/=g;g=std::gcd(n,d);n/=g;d/=g;
  if(ticks<0||ticks>std::numeric_limits<std::int64_t>::max()/n)return std::nullopt;
  return std::pair{ticks*n,d};
 };
 for(std::size_t i=0;i<source.pts.size();++i){
  auto a=fraction(source.pts[i]-source.pts.front(),source.numerator,source.denominator);
  auto b=fraction(output.pts[i]-output.pts.front(),output.numerator,output.denominator);
  if(!a||!b||*a!=*b)return false;
 }
 if(source.duration_ticks<=0||output.duration_ticks<=0)return false;
 const auto a=fraction(source.duration_ticks,source.numerator,source.denominator);
 const auto b=fraction(output.duration_ticks,output.numerator,output.denominator);
 return a&&b&&*a==*b;
}
inline std::vector<std::wstring> video_timing_arguments(const std::filesystem::path& file){
 return {L"-nostdin",L"-hide_banner",L"-v",L"info",L"-protocol_whitelist",L"file",L"-i",file.wstring(),L"-map",L"0:v:0",L"-an",L"-c:v",L"copy",L"-f",L"framehash",L"-hash",L"adler32",L"-"};
}
}
