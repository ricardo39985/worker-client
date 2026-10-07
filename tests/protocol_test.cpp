#include <algorithm>
#include "test.hpp"
#include "ow/protocol.hpp"
using namespace ow;
namespace {
json::object offer_message(){return {{"protocol",1},{"type","offer"},{"job_id","job-a"},{"attempt_id","attempt-a"},{"capability","conversion.video.h264"},{"offer_ttl_ms",10000},{"timeout_ms",30000},{"resources",{{"ram_mb",1024},{"vram_mb",0},{"scratch_mb",100},{"cpu_threads",2}}},{"input",{{"url","https://media.example.test/in?sig=abc"},{"sha256",std::string(64,'a')},{"bytes",1024}}},{"output",{{"put_url","https://media.example.test/out?sig=def"},{"max_bytes",100000}}}};}
}
TEST("media transfer requires exact approved HTTPS host"){
 const std::vector<std::string> hosts{"media.example.test"};CHECK(allowed_url("https://media.example.test/a?x=1",hosts));CHECK(allowed_url("https://MEDIA.EXAMPLE.TEST:443/a",hosts));
 CHECK(!allowed_url("http://media.example.test/a",hosts));CHECK(!allowed_url("https://media.example.test.evil/a",hosts));CHECK(!allowed_url("https://media.example.test@evil/a",hosts));CHECK(!allowed_url("https://media.example.test:22/a",hosts));CHECK(!allowed_url("https://127.0.0.1/a",hosts));CHECK(!allowed_url("https://media.example.test/a\r\nX: y",hosts));CHECK(!allowed_url("https://media.example.test./a",hosts));
}
TEST("well formed offer translates to validated typed job"){
 auto s=decode_offer(offer_message(),500,{"media.example.test"});CHECK(s.offer.attempt_id=="attempt-a");CHECK(s.offer.reserve_until==10500);CHECK(s.offer.resources.cpu_threads==2);
}
TEST("wire requests cannot smuggle arbitrary commands as capabilities"){
 auto m=offer_message();m["capability"]="powershell";THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("negative or floating resource values are rejected"){
 auto m=offer_message();m["resources"].as_object()["ram_mb"]=-1;THROWS(decode_offer(m,0,{"media.example.test"}));m["resources"].as_object()["ram_mb"]=1.5;THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("unverified GPU is never requested by the CPU converter"){
 auto m=offer_message();m["resources"].as_object()["vram_mb"]=128;THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("download digest is mandatory and strictly encoded"){
 auto m=offer_message();m["input"].as_object()["sha256"]="";THROWS(decode_offer(m,0,{"media.example.test"}));m["input"].as_object()["sha256"]=std::string(64,'z');THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("input and output must fit declared temporary storage"){
 auto m=offer_message();m["resources"].as_object()["scratch_mb"]=0;THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("control message size and parser nesting are bounded"){
 THROWS(parse(std::string(256*1024+1,' ')));THROWS(parse(std::string(40,'[')+"0"+std::string(40,']')));
}
TEST("null bytes cannot reach Win32 URL or process APIs"){
 json::object m{{"url",std::string("abc\0def",7)}};THROWS(text(m,"url"));THROWS(quote_windows_argument(std::wstring(L"a\0b",3)));
}
TEST("Windows argument quoting preserves spaces quotes and trailing slashes"){
 CHECK(quote_windows_argument(L"a b")==L"\"a b\"");CHECK(quote_windows_argument(L"a\"b")==L"\"a\\\"b\"");CHECK(quote_windows_argument(L"C:\\path\\")==L"\"C:\\path\\\\\"");CHECK(quote_windows_argument(L"")==L"\"\"");
}
TEST("converter receives local files not remote URLs or shell commands"){
 auto s=decode_offer(offer_message(),0,{"media.example.test"});auto args=ffmpeg_arguments(s,std::filesystem::path("workspace")/"job-1");
 CHECK(args.back().find(L"result.mp4")!=std::wstring::npos);for(const auto& a:args){CHECK(a.find(L"https:")==std::wstring::npos);CHECK(a!=L"cmd.exe");}CHECK(std::find(args.begin(),args.end(),L"-nostdin")!=args.end());
}
TEST("leases use the shorter of local and server remaining time"){
 CHECK(lease_deadline(100000,160000,110000,5000)==53000);
 CHECK(lease_deadline(110000,160000,100000,5000)==53000);
}
TEST("expired and unsigned-underflow lease cases fail closed"){
 THROWS(lease_deadline(100000,99999,100000,0));
 THROWS(lease_deadline(100000,120000,120000,0));
 THROWS(lease_deadline(100000,101000,100000,0));
 THROWS(lease_deadline(100000,160000,0,0));
 THROWS(lease_deadline(-1,160000,100000,0));
}
TEST("lease duration and monotonic addition cannot overflow"){
 THROWS(lease_deadline(100000,500001,100000,0));
 THROWS(lease_deadline(100000,160000,100000,INT64_MAX-1000));
 CHECK(lease_deadline(100000,400000,100000,0)==298000);
}
TEST("app rendition contract requires every fixed output and budgets all simultaneous files"){
 auto m=offer_message();m["capability"]="media.video.renditions.v1";
 m["parameters"]=json::object{{"copy_audio",true}};
 m["output"]=json::object{{"max_bytes",100000},{"artifacts",{{"feed",{{"put_url","https://media.example.test/a"},{"content_type","video/mp4"}}},{"feed_optimized",{{"put_url","https://media.example.test/b"},{"content_type","video/mp4"}}},{"thumbnail",{{"put_url","https://media.example.test/c"},{"content_type","image/webp"}}}}}};
 auto s=decode_offer(m,0,{"media.example.test"});CHECK(s.artifacts.size()==3 && s.copy_audio);
 m["output"].as_object()["artifacts"].as_object()["feed"].as_object()["put_url"]="https://evil.example.test/out";THROWS(decode_offer(m,0,{"media.example.test"}));
 m["output"].as_object()["artifacts"].as_object().erase("feed");THROWS(decode_offer(m,0,{"media.example.test"}));
}
TEST("app jobs refuse arbitrary output roles and mixed media type declarations"){
 auto m=offer_message();m["capability"]="media.image.renditions.v1";
 m["output"]=json::object{{"max_bytes",100000},{"artifacts",{{"feed",{{"put_url","https://media.example.test/a"},{"content_type","image/jpeg"}}},{"thumbnail",{{"put_url","https://media.example.test/b"},{"content_type","image/webp"}}}}}};
 THROWS(decode_offer(m,0,{"media.example.test"}));
 m["output"].as_object()["artifacts"].as_object()["feed"].as_object()["content_type"]="image/webp";
 CHECK(decode_offer(m,0,{"media.example.test"}).artifacts.size()==2);
 m["output"].as_object()["artifacts"].as_object()["execute"]=json::object{};THROWS(decode_offer(m,0,{"media.example.test"}));
}

