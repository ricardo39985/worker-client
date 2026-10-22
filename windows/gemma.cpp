#include "gemma.hpp"
namespace ow::win {
namespace {
std::filesystem::path local_path(const json::object& document,const char* name){
 const auto value=std::filesystem::path(wide(text(document,name,32700)));
 if(!value.is_absolute()||value.wstring().starts_with(L"\\\\")||!std::filesystem::exists(value))throw std::runtime_error("Gemma requires provisioned absolute local paths");
 return value;
}
void verify(const GemmaRuntime& runtime){
 for(const auto& pair:std::vector<std::pair<std::filesystem::path,std::string>>{{runtime.python,runtime.python_hash},{runtime.script,runtime.script_hash},{runtime.ffprobe,runtime.ffprobe_hash}})
  if(pair.second.size()!=64||sha256_file(pair.first)!=pair.second)throw std::runtime_error("Gemma provisioned dependency integrity differs");
}
}
std::optional<GemmaRuntime> load_gemma(const std::filesystem::path& owner_root){
 const auto path=owner_root/"gemma.json";if(!std::filesystem::exists(path))return std::nullopt;
 auto bytes=read_file(path);if(bytes.starts_with("\xef\xbb\xbf"))bytes.erase(0,3);
 const auto doc=parse(bytes).as_object();
 if(number(doc,"schema",1)!=1||text(doc,"profile")!=gemma::profile)throw std::runtime_error("Gemma configuration profile differs");
 if(!doc.at("enabled").as_bool())return std::nullopt;
 GemmaRuntime runtime{local_path(doc,"python"),local_path(doc,"script"),local_path(doc,"runtime_root"),local_path(doc,"ffprobe"),
  text(doc,"python_sha256",64),text(doc,"script_sha256",64),text(doc,"ffprobe_sha256",64),false};
 if(auto* value=doc.if_contains("advertise"))runtime.advertise=value->as_bool();
 verify(runtime);return runtime;
}
void run_gemma(const GemmaRuntime& runtime,const std::filesystem::path& ffmpeg,const std::filesystem::path& workspace,
 const std::string& modality,const std::function<bool()>& cancelled,Resources resources,Priority priority,bool probe){
 verify(runtime);
 if(cancelled())throw std::runtime_error("Gemma cancelled before inference");
 std::vector<std::wstring> args{L"-I",runtime.script.wstring(),L"--runtime-root",runtime.root.wstring(),
  L"--output",(workspace/"result.json").wstring(),L"--threads",std::to_wstring(std::min<std::uint64_t>(8,resources.cpu_threads))};
 if(probe)args.push_back(L"probe");
 else {args.insert(args.end(),{L"asset",L"--source",(workspace/"source.bin").wstring(),L"--modality",wide(modality)});}
 args.insert(args.end(),{L"--ffmpeg",ffmpeg.wstring(),L"--ffprobe",runtime.ffprobe.wstring()});
 Child child(runtime.python,args,workspace,resources,priority);
 const auto until=monotonic_ms()+(probe?300000:3600000);
 for(;;){
  if(cancelled()||monotonic_ms()>=until)throw std::runtime_error("Gemma inference cancelled or timed out");
  if(auto code=child.exit_code()){if(*code)throw std::runtime_error("Gemma inference failed; capability unavailable");break;}
  std::error_code error;const auto bytes=std::filesystem::file_size(workspace/"process.log",error);
  if(!error&&bytes>5*1024*1024)throw std::runtime_error("Gemma inference log exceeds budget");
  Sleep(100);
 }
 child.stop();
 auto document=parse(read_file(workspace/"result.json",gemma::result_limit),gemma::result_limit).as_object();
 if(probe){
  if(number(document,"schema",2)!=2||text(document,"profile")!=gemma::profile||number(document,"dimensions",768)!=768||!document.at("image_vectors_differ").as_bool())throw std::runtime_error("Gemma capability probe differs");
  const auto& modalities=document.at("modalities").as_array();if(modalities!=json::array{"text","image","video","audio"})throw std::runtime_error("Gemma modality probe is incomplete");
 }else gemma::validate_result(document,sha256_file(workspace/"source.bin"),modality);
}
}
