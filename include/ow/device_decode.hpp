#pragma once
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <string>
namespace ow {
// FFmpeg input inspection is already needed for guarded geometry. Reuse it;
// unknown codecs/profiles, chroma, depth, HDR and interlace stay on software.
inline bool cuda_sdr_decode_eligible(const std::string& inspection) {
 const auto input=inspection.find("Input #0,");
 if(input==std::string::npos)return false;
 const auto end=inspection.find("Output #",input);
 const auto header=inspection.substr(input,end==std::string::npos?end:end-input);
 static const std::regex stream(R"(^[ \t]*Stream #0:[0-9]+(?:\[[^\]\r\n]*\])?(?:\([^\)\r\n]*\))?: (Video: [^\r\n]*))");
 std::smatch match;std::istringstream rows(header);std::string line;bool found=false;
 while(std::getline(rows,line))if(std::regex_search(line,match,stream)){line=match[1].str();found=true;break;}
 if(!found)return false;
 static const std::regex codec(R"(^Video: (?:h264 \((?:Constrained Baseline|Baseline|Main|High)\)|hevc \(Main\))[, ])");
 static const std::regex format(R"(, yuv420p(?:[ ,(]|$))");
 if(!std::regex_search(line,codec)||!std::regex_search(line,format))return false;
 for(const auto* unsupported:{"bt2020","smpte2084","arib-std-b67","top first","bottom first","top coded first","bottom coded first","interlaced"})
  if(line.find(unsupported)!=std::string::npos)return false;
 return true;
}
inline bool inspection_poster_reusable(const std::string& inspection){
 if(!cuda_sdr_decode_eligible(inspection))return false;
 // The normal poster auto-rotates; geometry inspection deliberately does not.
 // Discard the speculative poster and use its normal path for such sources.
 auto text=inspection;std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 for(const auto* marker:{"rotate","rotation","displaymatrix","display matrix","orientation"})
  if(text.find(marker)!=std::string::npos)return false;
 const auto video=text.find("video: ");
 return video!=std::string::npos&&text.find("video: ",video+1)==std::string::npos;
}
}
