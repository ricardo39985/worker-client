#include "ow/secret_origin.hpp"
#include "ow/json.hpp"
namespace ow {
namespace {
void erase(void* data,std::size_t size) noexcept {
 auto* bytes=static_cast<volatile unsigned char*>(data);
 for(std::size_t i=0;i<size;++i)bytes[i]=0;
}
struct Wipe {
 std::string& input;std::span<unsigned char> workspace;
 ~Wipe(){erase(input.data(),input.size());erase(workspace.data(),workspace.size());}
};
}
std::string consume_secret_origin(std::string& plaintext,std::span<unsigned char> workspace){
 Wipe cleanup{plaintext,workspace};
 // BOTH parser scratch and result strings use the bounded caller buffer.
 // static_resource throws on exhaustion; it has no upstream/default fallback.
 json::static_resource arena(workspace.data(),workspace.size());
 json::storage_ptr storage(&arena);json::parse_options options;options.max_depth=32;
 json::stream_parser parser(storage,options);parser.reset(storage);
 parser.write(plaintext);parser.finish();auto value=parser.release();
 const auto& record=value.as_object();return text(record,"coordinator_url",1024);
}
}
