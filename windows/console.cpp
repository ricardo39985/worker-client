#include "platform.hpp"
#include <iostream>
#include <array>
using namespace ow::win;
int wmain(int argc,wchar_t** argv){
 try{
  DWORD parent=0;auto root=data_directory();for(int i=1;i<argc;++i){if(std::wstring(argv[i])==L"--pid"&&i+1<argc)parent=static_cast<DWORD>(std::stoul(argv[++i]));else if(std::wstring(argv[i])==L"--data-root"&&i+1<argc)root=argv[++i];else throw std::runtime_error("Unknown console argument");}
  SetConsoleTitleW(L"Organizer Worker - Live Console (close safely to tray)");SetConsoleOutputCP(CP_UTF8);
  std::cout<<"Organizer Worker - live log viewer\nClosing this window does NOT stop jobs. Use the system tray to pause, set priority or exit.\n\n";
  Handle controller(parent?OpenProcess(SYNCHRONIZE,FALSE,parent):nullptr);if(parent&&!controller)fail("Locate tray controller");
  std::uint64_t position=0,last_file=0;std::array<char,16384> buffer{};
  while(!controller||WaitForSingleObject(controller.get(),0)==WAIT_TIMEOUT){
   Handle file(CreateFileW((root/"worker.log").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
   if(file){BY_HANDLE_FILE_INFORMATION info{};if(GetFileInformationByHandle(file.get(),&info)){auto identity=(static_cast<std::uint64_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow;if(identity!=last_file){last_file=identity;position=0;}}
    LARGE_INTEGER size{};if(GetFileSizeEx(file.get(),&size)){if(position>static_cast<std::uint64_t>(size.QuadPart))position=0;LARGE_INTEGER at{};at.QuadPart=static_cast<LONGLONG>(position);require(SetFilePointerEx(file.get(),at,nullptr,FILE_BEGIN),"Seek live log");for(;;){DWORD n=0;require(ReadFile(file.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&n,nullptr),"Read live log");if(!n)break;std::cout.write(buffer.data(),n);position+=n;}std::cout.flush();}}
   if(controller)WaitForSingleObject(controller.get(),250);else Sleep(250);
  }
  std::cout<<"\nWorker exited.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
