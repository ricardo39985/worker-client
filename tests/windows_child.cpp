#include "platform.hpp"
#include "ow/protocol.hpp"
#include <fstream>
using namespace ow;using namespace ow::win;
int wmain(int argc,wchar_t** argv){
 try{
  bool query_host=false;std::filesystem::path ready;
  for(int n=1;n<argc;++n){if(std::wstring(argv[n])==L"serve-query")query_host=true;if(std::wstring(argv[n])==L"--output"&&n+1<argc)ready=argv[n+1];}
  if(query_host){
   std::ofstream("model.pid")<<GetCurrentProcessId()<<'\n';
   const auto exe=executable_directory()/"ow_test_child.exe";auto command=quote_windows_argument(exe.wstring())+L" --leaf";
   STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
   require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Create model fixture descendant");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
   json::array vector;vector.emplace_back(1.0);for(int n=1;n<768;++n)vector.emplace_back(0.0);
   const auto result=json::serialize(json::object{{"schema",2},{"profile","embeddinggemma2-q8-b11475-768-segments-v2"},{"vector",vector}});
   atomic_write(ready,result);
   for(;;){for(const auto& file:std::filesystem::directory_iterator(ready.parent_path()))if(file.path().extension()==".query"){
    if(read_file(file.path(),4096)=="stall")continue;
    auto output=file.path();output.replace_extension(".result");atomic_write(output,result);std::filesystem::remove(file.path());
   }Sleep(10);}
  }
  if(argc==3&&std::wstring(argv[1])==L"--commit-mb"){
   const SIZE_T bytes=static_cast<SIZE_T>(std::stoull(argv[2]))*1024*1024;
   auto memory=static_cast<volatile unsigned char*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
   if(!memory){std::ofstream("allocation.txt")<<"Cannot allocate memory";return 12;}
   for(SIZE_T i=0;i<bytes;i+=4096)memory[i]=1;
   VirtualFree(const_cast<unsigned char*>(memory),0,MEM_RELEASE);return 0;
  }
  bool leaf=argc>1&&std::wstring(argv[1])==L"--leaf";
  std::ofstream(leaf?"leaf.pid":"root.pid")<<GetCurrentProcessId()<<'\n';
  if(!leaf){
   auto exe=executable_directory()/"ow_test_child.exe";
   auto command=quote_windows_argument(exe.wstring())+L" --leaf";
   STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
   require(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi),"Create test descendant");
   CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
  }
  Sleep(120000);return 0;
 }catch(...){return 1;}
}
