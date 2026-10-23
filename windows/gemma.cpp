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
GemmaQuery::GemmaQuery(const GemmaRuntime& runtime,const std::filesystem::path& root,Resources limits,Priority priority,const std::function<bool()>& cancelled)
 :runtime_(runtime),workspace_(root/"query-model"/("session-"+random_hex(8))),limits_(limits),priority_(priority){
 try{start(cancelled);}catch(...){stop();std::error_code error;std::filesystem::remove_all(workspace_,error);throw;}
}
GemmaQuery::~GemmaQuery(){stop();std::error_code error;std::filesystem::remove_all(workspace_,error);}
void GemmaQuery::stop() noexcept {try{if(child_)child_->stop();}catch(...){}child_.reset();}
void GemmaQuery::start(const std::function<bool()>& cancelled){
 verify(runtime_);std::filesystem::create_directories(workspace_);
 std::error_code error;std::filesystem::remove(workspace_/"ready.json",error);
 child_=std::make_unique<Child>(runtime_.python,std::vector<std::wstring>{L"-I",runtime_.script.wstring(),L"--runtime-root",runtime_.root.wstring(),L"--output",(workspace_/"ready.json").wstring(),L"--threads",std::to_wstring(limits_.cpu_threads),L"serve-query"},workspace_,limits_,priority_);
 const auto until=monotonic_ms()+120000;
 for(;;){
  if(cancelled()||monotonic_ms()>=until)throw std::runtime_error("Resident query startup cancelled or timed out");
  if(child_->exit_code())throw std::runtime_error("Resident query process unavailable");
  if(std::filesystem::exists(workspace_/"ready.json")){
   gemma::validate_query(parse(read_file(workspace_/"ready.json",65536)).as_object());return;
  }
  Sleep(10);
 }
}
void GemmaQuery::encode(const std::filesystem::path& attempt,const std::function<bool()>& cancelled,Priority priority){
 std::lock_guard lock(mutex_);
 if(cancelled())throw std::runtime_error("Query cancelled before inference");
 if(!child_||child_->exit_code()){stop();try{start(cancelled);}catch(...){stop();throw;}}
 child_->priority(priority);
 const auto identity=random_hex(16);const auto request=workspace_/(identity+".query"),result=workspace_/(identity+".result"),failure=workspace_/(identity+".error");
 struct Cleanup {std::vector<std::filesystem::path> paths;~Cleanup(){std::error_code e;for(const auto& p:paths)std::filesystem::remove(p,e);}} cleanup{{request,result,failure}};
 const auto source=read_file(attempt/"source.bin",4096);if(source.empty())throw std::runtime_error("Empty query");
 atomic_write(request,source);const auto until=monotonic_ms()+120000;
 try{
  for(;;){
   if(cancelled()||monotonic_ms()>=until)throw std::runtime_error("Resident query cancelled or timed out");
   if(child_->exit_code()||std::filesystem::exists(failure))throw std::runtime_error("Resident query unavailable");
   if(std::filesystem::exists(result)){
    auto bytes=read_file(result,65536);gemma::validate_query(parse(bytes).as_object());
    if(cancelled())throw std::runtime_error("Query cancelled before publication");
    atomic_write(attempt/"result.json",bytes);return;
   }
   Sleep(10);
  }
 }catch(...){stop();throw;} // Drain the complete tree before releasing this query's CPU reservation.
}
GemmaExecution run_gemma(const GemmaRuntime& runtime,const std::filesystem::path& ffmpeg,const std::filesystem::path& workspace,
 const std::string& modality,const std::function<bool()>& cancelled,Resources resources,Priority priority,bool probe,bool query){
 verify(runtime);
 if(cancelled())throw std::runtime_error("Gemma cancelled before inference");
 std::vector<std::wstring> args{L"-I",runtime.script.wstring(),L"--runtime-root",runtime.root.wstring(),
  L"--output",(workspace/"result.json").wstring(),L"--threads",std::to_wstring(std::min<std::uint64_t>(8,resources.cpu_threads))};
 if(probe)args.push_back(L"probe");
 else if(query)args.insert(args.end(),{L"query",L"--text-file",(workspace/"source.bin").wstring()});
 else {args.insert(args.end(),{L"asset",L"--source",(workspace/"source.bin").wstring(),L"--modality",wide(modality)});}
 if(!query)args.insert(args.end(),{L"--ffmpeg",ffmpeg.wstring(),L"--ffprobe",runtime.ffprobe.wstring()});
 Child child(runtime.python,args,workspace,resources,priority);
 const auto started=monotonic_ms(),until=started+(probe?300000:3600000);
 for(;;){
  if(cancelled()||monotonic_ms()>=until)throw std::runtime_error("Gemma inference cancelled or timed out");
  if(auto code=child.exit_code()){
   if(*code)throw std::runtime_error("Gemma inference failed (exit="+std::to_string(*code)+
      ", peak_commit_mb="+std::to_string(child.peak_commit_mb())+", limit_mb="+std::to_string(resources.ram_mb)+")");
   break;
  }
  std::error_code error;const auto bytes=std::filesystem::file_size(workspace/"process.log",error);
  if(!error&&bytes>5*1024*1024)throw std::runtime_error("Gemma inference log exceeds budget");
  Sleep(100);
 }
 const auto peak=child.peak_commit_mb();child.stop();
 auto document=parse(read_file(workspace/"result.json",gemma::result_limit),gemma::result_limit).as_object();
 if(probe){
  if(number(document,"schema",2)!=2||text(document,"profile")!=gemma::profile||number(document,"dimensions",768)!=768||!document.at("image_vectors_differ").as_bool())throw std::runtime_error("Gemma capability probe differs");
  const auto& modalities=document.at("modalities").as_array();if(modalities!=json::array{"text","image","video","audio"})throw std::runtime_error("Gemma modality probe is incomplete");
 }else if(query)gemma::validate_query(document);
 else gemma::validate_result(document,sha256_file(workspace/"source.bin"),modality);
 return {peak,monotonic_ms()-started};
}
}
