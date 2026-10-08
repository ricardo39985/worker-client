#include <winsock2.h>
#include <ws2tcpip.h>
#include "embedding.hpp"
#include "ow/inference.hpp"
#include <future>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cwctype>
namespace ow::win {
namespace {
constexpr const char* ModelHash="2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135";
constexpr const char* ProjectorHash="c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f";
struct ModelLoading:std::runtime_error{using std::runtime_error::runtime_error;};
struct Internet {HINTERNET h{};~Internet(){if(h)WinHttpCloseHandle(h);} Internet(const Internet&)=delete;Internet& operator=(const Internet&)=delete;explicit Internet(HINTERNET value):h(value){if(!h)throw std::runtime_error("Local inference transport unavailable");}};
unsigned port(){
 WSADATA w{};if(WSAStartup(MAKEWORD(2,2),&w))throw std::runtime_error("Local inference socket initialization failed");
 SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s==INVALID_SOCKET){WSACleanup();throw std::runtime_error("Local inference socket unavailable");}
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);int size=sizeof(address);
 bool ok=bind(s,reinterpret_cast<sockaddr*>(&address),size)==0&&getsockname(s,reinterpret_cast<sockaddr*>(&address),&size)==0;
 unsigned result=ntohs(address.sin_port);closesocket(s);WSACleanup();if(!ok)throw std::runtime_error("Local inference port unavailable");return result;
}
std::string local_request(unsigned p,const std::string& key,const std::string& body){
 Internet session(WinHttpOpen(L"OrganizerEmbedding/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 WinHttpSetTimeouts(session.h,2000,2000,10000,600000);
 Internet connection(WinHttpConnect(session.h,L"127.0.0.1",static_cast<INTERNET_PORT>(p),0));
 Internet request(WinHttpOpenRequest(connection.h,L"POST",L"/v1/embeddings",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0));
 DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;require(WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)),"Disable inference redirects");
 auto headers=wide("Content-Type: application/json\r\nAuthorization: Bearer "+key+"\r\n");
 if(!WinHttpSendRequest(request.h,headers.c_str(),static_cast<DWORD>(headers.size()),const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0))throw ModelLoading("CPU model is loading");
 if(!WinHttpReceiveResponse(request.h,nullptr))throw std::runtime_error("Local inference response failed");
 DWORD status{},length=sizeof(status);require(WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX),"Read inference status");
 if(status==503)throw ModelLoading("CPU model is loading");
 if(status!=200)throw std::runtime_error("Local inference request was rejected");
 std::string output;char buffer[8192];DWORD received;
 for(;;){if(!WinHttpReadData(request.h,buffer,sizeof(buffer),&received))throw std::runtime_error("Local inference read failed");if(!received)break;if(output.size()+received>65536)throw std::runtime_error("Local inference response exceeds budget");output.append(buffer,received);}
 return output;
}
std::string base64_file(const std::filesystem::path& file){
 auto bytes=read_file(file,4*1024*1024);static constexpr char table[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve((bytes.size()+2)/3*4);
 for(std::size_t i=0;i<bytes.size();i+=3){unsigned v=static_cast<unsigned char>(bytes[i])<<16;if(i+1<bytes.size())v|=static_cast<unsigned char>(bytes[i+1])<<8;if(i+2<bytes.size())v|=static_cast<unsigned char>(bytes[i+2]);out+=table[(v>>18)&63];out+=table[(v>>12)&63];out+=i+1<bytes.size()?table[(v>>6)&63]:'=';out+=i+2<bytes.size()?table[v&63]:'=';}return out;
}
void verify(const Config& c,const std::function<bool()>& cancelled){
 if(c.embedding_model.empty()||c.llama_server.empty())throw std::runtime_error("EmbeddingGemma 2 is not configured");
 auto hash=[&](const std::filesystem::path& p){if(!p.is_absolute()||!std::filesystem::is_regular_file(p)||(GetFileAttributesW(p.c_str())&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Invalid installed inference file");return sha256_file(p,[&](std::uint64_t){if(cancelled&&cancelled())throw std::runtime_error("Inference cancelled");});};
 if(hash(c.embedding_model)!=ModelHash||hash(c.embedding_projector)!=ProjectorHash)throw std::runtime_error("EmbeddingGemma 2 integrity check failed");
 if(c.llama_files.empty())throw std::runtime_error("Inference runtime manifest missing");
 bool server=false;for(const auto& f:c.llama_files){if(hash(f.first)!=f.second)throw std::runtime_error("Inference runtime integrity check failed");if(f.first==c.llama_server)server=true;}
 if(!server)throw std::runtime_error("Inference executable not in approved manifest");
 for(const auto& f:std::filesystem::directory_iterator(c.llama_server.parent_path())){auto ext=f.path().extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);if(ext!=L".exe"&&ext!=L".dll")continue;bool approved=false;for(const auto& entry:c.llama_files)if(entry.first==f.path())approved=true;if(!approved)throw std::runtime_error("Unexpected inference runtime file; rerun setup repair");}
}
json::array embed(const Config& c,const std::filesystem::path& workspace,const json::value& input,bool media,Resources resources,Priority priority,const std::function<bool()>& cancelled){
 auto p=port();auto key=random_hex(32);
 std::vector<std::wstring> args={L"--model",c.embedding_model.wstring(),L"--host",L"127.0.0.1",L"--port",std::to_wstring(p),L"--api-key",wide(key),L"--embeddings",L"--pooling",L"mean",L"--parallel",L"1",L"--ctx-size",L"2048",L"--batch-size",L"2048",L"--ubatch-size",L"2048",L"--threads",std::to_wstring(resources.cpu_threads),L"--threads-batch",std::to_wstring(resources.cpu_threads),L"--n-gpu-layers",L"0",L"--no-webui"};
 if(media){args.push_back(L"--mmproj");args.push_back(c.embedding_projector.wstring());args.push_back(L"--no-mmproj-offload");args.push_back(L"--image-max-tokens");args.push_back(L"256");args.push_back(L"--mtmd-batch-max-tokens");args.push_back(L"256");}
 Child child(c.llama_server,args,workspace,resources,priority);auto deadline=monotonic_ms()+120000;
 const auto body=json::serialize(json::object{{"model","embeddinggemma-2"},{"input",json::array{input}}});
 // Failed connection during model loading is retried. Once a request reaches
 // the model, it executes once; no unauthenticated/public listener is used.
 for(;;){
  if(cancelled&&cancelled())throw std::runtime_error("Inference cancelled");if(child.exit_code())throw std::runtime_error("CPU inference process failed");
  auto request=std::async(std::launch::async,[&]{return local_request(p,key,body);});
  while(request.wait_for(std::chrono::milliseconds(100))!=std::future_status::ready){
   if((cancelled&&cancelled())||monotonic_ms()>deadline){child.stop();request.wait();throw std::runtime_error("CPU inference cancelled or timed out");}
  }
  try{
   auto reply=parse(request.get(),65536).as_object();const auto& data=reply.at("data").as_array();if(data.size()!=1)throw std::runtime_error("Unexpected embedding batch");
   const auto& raw=data[0].as_object().at("embedding").as_array();std::vector<double> vector;for(const auto& v:raw)vector.push_back(v.to_number<double>());
   auto values=normalize_embedding(std::move(vector));json::array result;for(auto v:values)result.emplace_back(v);return result;
  }catch(const ModelLoading&){if(monotonic_ms()>deadline)throw std::runtime_error("CPU model startup timed out");Sleep(200);}
 }
}
void convert(const Config& c,const JobSpec& s,const std::filesystem::path& w,const std::vector<std::wstring>& more,Priority priority,const std::function<bool()>& cancel){
 std::vector<std::wstring> args={L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-protocol_whitelist",L"file",L"-format_whitelist",L"mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe,wav,mp3",L"-threads",L"1",L"-filter_threads",L"1",L"-i",(w/"source.bin").wstring()};args.insert(args.end(),more.begin(),more.end());
 if(c.ffmpeg.empty()||sha256_file(c.ffmpeg)!=c.ffmpeg_sha256)throw std::runtime_error("Inference media preprocessing requires verified FFmpeg");
 Child child(c.ffmpeg,args,w,s.offer.resources,priority);auto end=monotonic_ms()+30000;
 while(!child.exit_code()){if(cancel()||monotonic_ms()>end)throw std::runtime_error("Inference preprocessing cancelled or timed out");Sleep(100);}if(*child.exit_code())throw std::runtime_error("Inference preprocessing failed");
}
}
void execute_embedding(const Config& c,const JobSpec& s,const std::filesystem::path& w,Priority priority,const std::function<bool()>& cancel,const std::function<void(const char*)>& phase){
 phase("verify CPU model and runtime");verify(c,cancel);json::value input;
 bool media=s.modality!="text";
 if(!media){auto content=read_file(w/"source.bin",4096);if(content.empty()||content.find('\0')!=std::string::npos)throw std::runtime_error("Invalid embedding text input");input="title: none | text: "+content;}
 else{
  phase("prepare bounded inference sample");json::array parts;
  if(s.modality=="audio"){
   convert(c,s,w,{L"-map",L"0:a:0",L"-t",L"10",L"-ar",L"16000",L"-ac",L"1",L"-c:a",L"pcm_s16le",L"-threads",L"1",(w/"sample.wav").wstring()},priority,cancel);
   parts.emplace_back(json::object{{"type","input_audio"},{"input_audio",{{"data",base64_file(w/"sample.wav")},{"format","wav"}}}});
  }else{
   bool video=s.modality=="video";
   convert(c,s,w,{L"-map",L"0:v:0",L"-vf",video?L"fps=1,scale=w='min(256,iw)':h='min(256,ih)':force_original_aspect_ratio=decrease":L"scale=w='min(256,iw)':h='min(256,ih)':force_original_aspect_ratio=decrease",L"-frames:v",video?L"4":L"1",L"-c:v",L"mjpeg",L"-q:v",L"3",L"-threads",L"1",(w/"sample-%02d.jpg").wstring()},priority,cancel);
   for(int i=1;i<=(video?4:1);++i){auto file=w/("sample-0"+std::to_string(i)+".jpg");if(!std::filesystem::exists(file))break;parts.emplace_back(json::object{{"type","image_url"},{"image_url",{{"url","data:image/jpeg;base64,"+base64_file(file)}}}});}
   if(parts.empty())throw std::runtime_error("No inference image samples decoded");
  }
  input=json::object{{"content",std::move(parts)}};
 }
 phase("CPU embedding inference");auto vector=embed(c,w,input,media,s.offer.resources,priority,cancel);
 atomic_write(w/"embedding.json",json::serialize(json::object{{"schema",1},{"profile",embedding_profile},{"modality",s.modality},{"vector",std::move(vector)}}));
}
EmbeddingProbe probe_embedding(const Config& c,const std::function<bool()>& cancel,bool include_media){
 EmbeddingProbe p;auto w=c.root/"probes"/("embedding-"+random_hex(8));
 try{
  verify(c,cancel);std::filesystem::create_directories(w);
  auto a=embed(c,w,"title: none | text: A red flower",false,{768,0,128,1},Priority::low,cancel);
  auto b=embed(c,w,"title: none | text: A passenger train",false,{768,0,128,1},Priority::low,cancel);
  if(a==b)throw std::runtime_error("Inference probe did not distinguish text inputs");p.text=true;
  // Pixel-content discrimination catches accidentally ignored media payloads.
  if(include_media&&!c.ffmpeg.empty()){
   if(sha256_file(c.ffmpeg)!=c.ffmpeg_sha256)throw std::runtime_error("Unverified preprocessing runtime");
   JobSpec s;s.modality="image";s.offer.resources={2048,0,128,1};
   std::vector<json::array> samples;
   for(auto color:{"red","blue"}){
    Child child(c.ffmpeg,{L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"-y",L"-f",L"lavfi",L"-i",wide(std::string("color=c=")+color+":s=32x32"),L"-frames:v",L"1",L"-threads",L"1",L"-c:v",L"png",L"-f",L"image2",(w/"source.bin").wstring()},w,s.offer.resources,Priority::low);
    // Use a known format and filename; no shell or network decoder inputs.
    auto until=monotonic_ms()+30000;while(!child.exit_code()){if((cancel&&cancel())||monotonic_ms()>until)throw std::runtime_error("Media probe cancelled");Sleep(100);}if(*child.exit_code())throw std::runtime_error("Media probe sample failed");
    execute_embedding(c,s,w,Priority::low,cancel,[](const char*){});samples.push_back(parse(read_file(w/"embedding.json")).as_object().at("vector").as_array());
   }
   if(samples[0]==samples[1])throw std::runtime_error("Inference probe ignored image content");p.media=true;
  }
  if(p.media){
   // Probe multi-frame input independently of single-image support.
   json::array parts;for(int i=0;i<4;++i)parts.emplace_back(json::object{{"type","image_url"},{"image_url",{{"url","data:image/png;base64,"+base64_file(w/"source.bin")}}}});
   embed(c,w,json::object{{"content",std::move(parts)}},true,{2048,0,128,1},Priority::low,cancel);p.video=true;
   std::vector<json::array> tones;
   for(auto frequency:{"440","880"}){
    Child child(c.ffmpeg,{L"-nostdin",L"-v",L"error",L"-y",L"-f",L"lavfi",L"-i",wide(std::string("sine=frequency=")+frequency+":duration=1"),L"-ar",L"16000",L"-ac",L"1",L"-c:a",L"pcm_s16le",(w/"tone.wav").wstring()},w,{2048,0,128,1},Priority::low);
    auto end=monotonic_ms()+30000;while(!child.exit_code()){if((cancel&&cancel())||monotonic_ms()>end)throw std::runtime_error("Audio probe cancelled");Sleep(100);}if(*child.exit_code())throw std::runtime_error("Audio sample probe failed");
    json::array content{json::object{{"type","input_audio"},{"input_audio",{{"data",base64_file(w/"tone.wav")},{"format","wav"}}}}};
    tones.push_back(embed(c,w,json::object{{"content",std::move(content)}},true,{2048,0,128,1},Priority::low,cancel));
   }
   if(tones[0]==tones[1])throw std::runtime_error("Inference probe ignored audio content");p.audio=true;
  }
  p.reason="EmbeddingGemma 2 CPU text verified; media="+std::string(p.media?"verified":"unavailable");
 }catch(const std::exception&){p.reason="EmbeddingGemma 2 CPU probe unavailable; inference stays disabled. Rerun setup to repair or inspect native probe.";}
 std::error_code e;std::filesystem::remove_all(w,e);return p;
}
}
