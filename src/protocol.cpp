#include "ow/protocol.hpp"
#include "ow/media_renditions.hpp"
#include "ow/inference.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
namespace ow {
Tick lease_deadline(Tick server,Tick expires,Tick local,Tick now){
 if(server<0||local<0||now<0||expires<=server||expires<=local)throw std::runtime_error("Invalid lease clock/expiry");
 auto skew=server>local?server-local:local-server;
 if(skew>30000||expires-server>300000)throw std::runtime_error("Unsafe lease clock/expiry; synchronize system clock");
 auto remaining=std::min(expires-server,expires-local);
 if(remaining<=2000||now>INT64_MAX-(remaining-2000))throw std::runtime_error("Lease does not have safe execution time");
 return now+(remaining-2000);
}

bool allowed_url(const std::string& url,const std::vector<std::string>& hosts){
 if(url.size()>8192||url.rfind("https://",0)!=0||url.find('#')!=std::string::npos)return false;
 for(unsigned char c:url)if(c<=32||c>=127||c=='\\')return false;
 auto end=url.find_first_of("/?",8);std::string host=url.substr(8,end==std::string::npos?end:end-8);
 if(host.ends_with(":443"))host.resize(host.size()-4);
 if(host.empty()||host.front()=='.'||host.back()=='.')return false;
 for(unsigned char c:host)if(!std::isalnum(c)&&c!='.'&&c!='-')return false;
 std::transform(host.begin(),host.end(),host.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 return std::find(hosts.begin(),hosts.end(),host)!=hosts.end();
}
JobSpec decode_offer(const json::object& m,Tick now,const std::vector<std::string>& hosts){
 if(number(m,"protocol",1)!=1||text(m,"type",32)!="offer")throw std::runtime_error("unsupported protocol message");
 JobSpec s;s.offer.job_id=text(m,"job_id",128);s.offer.attempt_id=text(m,"attempt_id",128);s.offer.capability=text(m,"capability",128);
 if(!safe_id(s.offer.job_id)||!safe_id(s.offer.attempt_id))throw std::runtime_error("invalid job identity");
 const bool embedding=s.offer.capability==embedding_capability;
 const bool media_video=s.offer.capability=="media.video.renditions.v1",media_image=s.offer.capability=="media.image.renditions.v1";
 if(!embedding&&!media_video&&!media_image&&s.offer.capability!="conversion.video.h264"&&s.offer.capability!="conversion.image.jpeg")throw std::runtime_error("capability is not installed in this build");
 auto r=m.at("resources").as_object();s.offer.resources={number(r,"ram_mb",1024*1024),number(r,"vram_mb",1024*1024),number(r,"scratch_mb",1024*1024),number(r,"cpu_threads",64)};
 if(s.offer.resources.ram_mb<(embedding?768u:((media_video||s.offer.capability=="conversion.video.h264")?512u:256u))||s.offer.resources.vram_mb!=0||!s.offer.resources.cpu_threads)throw std::runtime_error("resource declaration incompatible with adapter");
 auto ttl=number(m,"offer_ttl_ms",60000);if(ttl<1000)throw std::runtime_error("invalid offer TTL");s.offer.reserve_until=now+static_cast<Tick>(ttl);
 const auto& in=m.at("input").as_object();s.input_url=text(in,"url");s.input_sha256=text(in,"sha256",64);s.input_bytes=number(in,"bytes",1024ull*1024*1024);
 const auto& out=m.at("output").as_object();s.output_max_bytes=number(out,"max_bytes",1024ull*1024*1024);
 if(!s.input_bytes||!s.output_max_bytes||!allowed_url(s.input_url,hosts))throw std::runtime_error("unapproved input endpoint or byte limit");
 if(embedding){
  const auto& params=m.at("parameters").as_object();s.modality=text(params,"modality",16);
  if(s.offer.resources.ram_mb<(s.modality=="text"?768u:2048u))throw std::runtime_error("Inference modality exceeds memory declaration");
  if(params.size()!=2||text(params,"profile",64)!=embedding_profile||(s.modality!="text"&&s.modality!="image"&&s.modality!="audio"&&s.modality!="video")||s.output_max_bytes>65536||(s.modality=="text"&&s.input_bytes>4096)||s.input_bytes>104857600)throw std::runtime_error("Invalid typed inference parameters");
 }
 if(media_video||media_image){
  if(s.input_bytes>104857600||s.output_max_bytes>104857600)throw std::runtime_error("app media exceeds installed profile bounds");
  if(media_video){auto& parameters=m.at("parameters").as_object();auto* audio=parameters.if_contains("copy_audio");if(!audio||!audio->is_bool()||parameters.size()!=1)throw std::runtime_error("invalid typed app parameters");s.copy_audio=audio->as_bool();}
  auto plan=rendition_plan(media_video,s.copy_audio,"workspace");auto& artifacts=out.at("artifacts").as_object();
  if(artifacts.size()!=plan.size())throw std::runtime_error("invalid rendition output set");
  for(const auto& item:plan){auto& target=artifacts.at(item.role).as_object();auto url=text(target,"put_url");auto mime=text(target,"content_type",64);
   if(!allowed_url(url,hosts)||mime!=item.mime)throw std::runtime_error("unapproved rendition output");
   for(const auto& prior:s.artifacts)if(prior.url==url)throw std::runtime_error("rendition outputs must be independent");
   s.artifacts.push_back({item.role,url,mime});
  }
 }else{s.output_url=text(out,"put_url");if(!allowed_url(s.output_url,hosts))throw std::runtime_error("unapproved output endpoint");}
 if(s.input_sha256.size()!=64||!std::all_of(s.input_sha256.begin(),s.input_sha256.end(),[](unsigned char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))throw std::runtime_error("invalid input SHA-256");
 if((s.input_bytes+s.output_max_bytes*(s.artifacts.empty()?1:s.artifacts.size())+1024*1024-1)/(1024*1024)>s.offer.resources.scratch_mb)throw std::runtime_error("insufficient declared scratch disk");
 s.timeout_ms=number(m,"timeout_ms",3600000);if(s.timeout_ms<1000)throw std::runtime_error("invalid task timeout");
 // Identity fingerprint excludes time-limited offer delivery metadata, but includes
 // every execution-affecting field. The coordinator must retry with the same spec.
 auto identity=m;identity.erase("offer_ttl_ms");identity.erase("message_id");s.offer.fingerprint=json::serialize(identity);return s;
}
std::string output_name(const JobSpec& s){return s.offer.capability=="conversion.video.h264"?"result.mp4":"result.jpg";}
std::vector<std::wstring> ffmpeg_arguments(const JobSpec& s,const std::filesystem::path& w){
 if(s.offer.capability!="conversion.video.h264"&&s.offer.capability!="conversion.image.jpeg")throw std::runtime_error("unsupported converter");
 auto n=std::to_wstring(s.offer.resources.cpu_threads);
 std::vector<std::wstring> args={L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-protocol_whitelist",L"file",L"-format_whitelist",L"mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe",L"-threads",n,L"-filter_threads",n,L"-filter_complex_threads",n,L"-i",(w/"source.bin").wstring(),L"-map_metadata",L"-1"};
 if(s.offer.capability=="conversion.video.h264"){
  const std::vector<std::wstring> more={L"-map",L"0:v:0",L"-map",L"0:a:0?",L"-vf",L"scale=w='min(1280,trunc(iw/2)*2)':h=-2,setsar=1",L"-c:v",L"libx264",L"-preset",L"veryfast",L"-crf",L"23",L"-pix_fmt",L"yuv420p",L"-threads",n,L"-c:a",L"aac",L"-b:a",L"128k",L"-movflags",L"+faststart"};args.insert(args.end(),more.begin(),more.end());
 }else{
  const std::vector<std::wstring> more={L"-map",L"0:v:0",L"-frames:v",L"1",L"-vf",L"scale=w='min(1920,trunc(iw/2)*2)':h=-2",L"-c:v",L"mjpeg",L"-q:v",L"3",L"-threads",n};args.insert(args.end(),more.begin(),more.end());
 }
 args.push_back((w/output_name(s)).wstring());return args;
}
std::wstring quote_windows_argument(const std::wstring& a){
 // MSVC CommandLineToArgv rules, not cmd.exe escaping. No shell is ever used.
 if(a.find(L'\0')!=std::wstring::npos)throw std::runtime_error("NUL in process argument");
 std::wstring out=L"\"";std::size_t slashes=0;
 for(wchar_t c:a){if(c==L'\\'){++slashes;continue;}if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}else{out.append(slashes,L'\\');out+=c;}slashes=0;}
 out.append(slashes*2,L'\\');out+=L'\"';return out;
}
}

