#include "platform.hpp"
#include "ow/protocol.hpp"
#include <fstream>
using namespace ow;using namespace ow::win;
int wmain(int argc,wchar_t** argv){
 try{
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
