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
 std::optional<std::pair<std::int64_t,std::int64_t>> rate() const {
  if(constant_rate()){
   auto ticks=pts[1]-pts[0];auto g=std::gcd(ticks,denominator);ticks/=g;auto n=denominator/g;
   if(ticks>std::numeric_limits<std::int64_t>::max()/numerator)return std::nullopt;
   auto d=ticks*numerator;g=std::gcd(n,d);return std::pair{n/g,d/g};
  }
  // Human-formatted fractional tbr is rounded. Do not guess an exact rate for
  // irregular sources from that decimal. Integral rates (including 90k) are
  // representable exactly; the output must still pass the timing guard.
  if(reported_rate.empty())return std::nullopt;
  std::string number=reported_rate;std::int64_t multiplier=1;
  if(number.back()=='k'){multiplier=1000;number.pop_back();}
  else if(number.back()=='M'){multiplier=1000000;number.pop_back();}
  if(number.empty()||number.find_first_not_of("0123456789")!=std::string::npos)return std::nullopt;
  try{auto n=std::stoll(number);if(n<=0||n>1000000000/multiplier)return std::nullopt;return std::pair{n*multiplier,std::int64_t{1}};}catch(...){return std::nullopt;}
 }
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
  }
 }
 if(!header||out.pts.size()<2)return std::nullopt;
 // Supplement exact PTS comparison with the demuxer's nominal rate. In
 // particular NVENC can retain VFR PTS but signal a different VUI/tbr rate.
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
 if(source.reported_rate!=output.reported_rate)return false;
 if(source.pts.size()!=output.pts.size()||source.pts.empty()||source.numerator<=0||source.denominator<=0||output.numerator<=0||output.denominator<=0)return false;
 // Compare reduced rational timestamps without floating point or MSVC-specific
 // wide integer arithmetic. Origin shifts by the muxer do not retime frames.
 auto fraction=[](std::int64_t ticks,std::int64_t n,std::int64_t d)->std::optional<std::pair<std::int64_t,std::int64_t>>{
  auto g=std::gcd(ticks,d);ticks/=g;d/=g;g=std::gcd(n,d);n/=g;d/=g;
  if(ticks>std::numeric_limits<std::int64_t>::max()/n)return std::nullopt;
  return std::pair{ticks*n,d};
 };
 for(std::size_t i=0;i<source.pts.size();++i){
  auto a=fraction(source.pts[i]-source.pts.front(),source.numerator,source.denominator);
  auto b=fraction(output.pts[i]-output.pts.front(),output.numerator,output.denominator);
  if(!a||!b||*a!=*b)return false;
 }
 return true;
}
inline std::vector<std::wstring> video_timing_arguments(const std::filesystem::path& file){
 return {L"-nostdin",L"-hide_banner",L"-v",L"info",L"-protocol_whitelist",L"file",L"-i",file.wstring(),L"-map",L"0:v:0",L"-an",L"-c:v",L"copy",L"-f",L"framehash",L"-hash",L"adler32",L"-"};
}
}
