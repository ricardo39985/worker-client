#include "worker.hpp"
#include "control_pipe.hpp"
#include "owner_task.hpp"
#include <charconv>
#include <iostream>
#include <iterator>
namespace {
void human(const ow::json::object& response){
 if(auto* rows=response.if_contains("settings")){for(const auto& row:rows->as_array()){const auto& r=row.as_object();std::cout<<ow::text(r,"name")<<" = "<<ow::json::serialize(r.at("current"))<<" (effective "<<ow::json::serialize(r.at("effective"))<<", default "<<ow::json::serialize(r.at("default"))<<", "<<ow::text(r,"type")<<(r.at("restart_required").as_bool()?", RESTART REQUIRED":"")<<")\n";}}
 else for(const auto& [key,value]:response)std::cout<<key<<": "<<ow::json::serialize(value)<<'\n';
}
}
int wmain(int argc,wchar_t** argv){
 using namespace ow;using namespace ow::win;bool json_output=false,offline=false;const char* phase="arguments";
 try{
  std::filesystem::path selected_root;
  require(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32|LOAD_LIBRARY_SEARCH_USER_DIRS),"Restrict DLL search");std::vector<std::string> args;
  for(int i=1;i<argc;++i){auto a=utf8(argv[i]);if(a=="--help"||a=="-h"){std::cout<<control_help();return 0;}if(a=="--json")json_output=true;else if(a=="--offline")offline=true;else if(a=="--data-root"){if(++i>=argc)throw std::runtime_error("Missing live target root");selected_root=argv[i];if(!selected_root.is_absolute()||selected_root.wstring().starts_with(L"\\\\"))throw std::runtime_error("Absolute local root required");}else args.push_back(std::move(a));}
  if(!args.empty()&&(args[0]=="start"||args[0]=="restart"||args[0]=="enroll"||args[0]=="enrollment")){
   if(offline||!selected_root.empty())throw ControlFailure(2,"Lifecycle requires installed owner state; no offline/custom-root activation");
   Tick timeout=args[0]=="restart"?1800000:600000;
   if(args.size()==3&&args[1]=="--wait-seconds"&&(args[0]=="start"||args[0]=="restart")){unsigned n{};auto [end,error]=std::from_chars(args[2].data(),args[2].data()+args[2].size(),n);if(error!=std::errc{}||end!=args[2].data()+args[2].size()||!n||n>1800)throw ControlFailure(2,"Wait must be 1..1800 seconds");timeout=static_cast<Tick>(n)*1000;}else if(args.size()!=1)throw ControlFailure(2,"Unexpected lifecycle arguments; credentials never accepted");
   phase="lifecycle";auto root=data_directory();auto result=args[0]=="enroll"?enroll_owner_task(root):args[0]=="enrollment"?inspect_owner_task(root):owner_lifecycle(root,args[0]=="restart",timeout);if(json_output)std::cout<<json::serialize(result)<<'\n';else human(result);return static_cast<int>(number(result,"exit_code",5));
  }
  phase="stdin";std::string input;if(args==std::vector<std::string>{"config","set","--stdin"}){for(char c;std::cin.get(c);){if(input.size()>=control_config_limit)throw std::runtime_error("Configuration input exceeds limit");input+=c;}}
  phase="request";auto request=control_request(args,input);auto root=selected_root.empty()?data_directory():selected_root;if(offline&&!selected_root.empty())throw std::runtime_error("Offline writes only support the installed owner root");json::object result;
  if(offline){
   if(elevated())throw ControlFailure(4,"Offline configuration requires the normal owner token");
   for(const auto& path:{root,root/"worker.json",root/"instance.lock",root/"journal.sqlite3",root/"journal.sqlite3-wal",root/"journal.sqlite3-shm"}){const auto attributes=GetFileAttributesW(path.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw ControlFailure(4,"Reparse-point state path denied");}
   if(!std::filesystem::is_regular_file(root/"worker.json"))throw std::runtime_error("No installed configuration");
   Handle lock(CreateFileW((root/"instance.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));if(!lock){result={{"schema",1},{"ok",false},{"exit_code",3},{"error","Worker/setup owns the state; omit --offline for live IPC"}};}else result=offline_control(root,request);
  }else{
   try{result=control_call(root,request);}catch(const std::exception&){result={{"schema",1},{"ok",false},{"exit_code",3},{"error","Owner control unavailable/timeout: worker stopped, old version, access denied or busy. No offline fallback was attempted."}};}
  }
  if(json_output)std::cout<<json::serialize(result)<<'\n';else human(result);return static_cast<int>(number(result,"exit_code",5));
 }catch(const ControlFailure& e){json::object result{{"schema",1},{"ok",false},{"exit_code",e.exit_code},{"error",e.what()}};if(json_output)std::cout<<json::serialize(result)<<'\n';else human(result);return e.exit_code;
 }catch(const std::exception&){json::object result{{"schema",1},{"ok",false},{"exit_code",2},{"error","Command rejected. Use --help/config list; check configuration and permissions. No credentials are accepted."},{"phase",phase}};if(json_output)std::cout<<json::serialize(result)<<'\n';else human(result);return 2;}
}
