#include "platform.hpp"
#include "ow/progress.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace ow;using namespace ow::win;
namespace {
class ConsoleOutput {
 HANDLE output_{GetStdHandle(STD_OUTPUT_HANDLE)};DWORD original_mode_{};WORD original_attributes_{FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE};
 bool interactive_{},vt_{};std::size_t dashboard_rows_{};
 static WORD category(const std::string& line){
  if(line.find("FAILED")!=std::string::npos||line.find("ERROR")!=std::string::npos||line.find("REVOKED")!=std::string::npos)return FOREGROUND_RED|FOREGROUND_INTENSITY;
  if(line.find("CANCELLED")!=std::string::npos||line.find("DECLINED")!=std::string::npos||line.find("OFFLINE")!=std::string::npos||line.find("Paused")!=std::string::npos||line.find("DRAINING")!=std::string::npos||line.find("EXITING")!=std::string::npos)return FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY;
  if(line.find("SUCCEEDED")!=std::string::npos||line.find("ACKNOWLEDGED")!=std::string::npos||line.find("CONNECTED")!=std::string::npos)return FOREGROUND_GREEN|FOREGROUND_INTENSITY;
  if(line.find("JOB ")!=std::string::npos)return FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;
  return FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE;
 }
 static const char* ansi(const std::string& line){
  auto color=category(line);if(color&FOREGROUND_RED&&!(color&FOREGROUND_GREEN))return "\x1b[91m";
  if(color&FOREGROUND_RED&&color&FOREGROUND_GREEN&&!(color&FOREGROUND_BLUE))return "\x1b[93m";
  if(color&FOREGROUND_GREEN&&!(color&FOREGROUND_RED)&&!(color&FOREGROUND_BLUE))return "\x1b[92m";
  if(color&FOREGROUND_BLUE&&color&FOREGROUND_GREEN)return "\x1b[96m";return "\x1b[0m";
 }
 void clear_dashboard(){
  if(!dashboard_rows_)return;
  std::cout.flush();
  if(vt_){for(std::size_t i=0;i<dashboard_rows_;++i)std::cout<<"\x1b[1A\r\x1b[2K";}
  else{CONSOLE_SCREEN_BUFFER_INFO info{};if(GetConsoleScreenBufferInfo(output_,&info)){
    auto top=static_cast<SHORT>(std::max<int>(0,info.dwCursorPosition.Y-static_cast<int>(dashboard_rows_)));COORD start{0,top};
    DWORD cells=static_cast<DWORD>(info.dwSize.X)*static_cast<DWORD>(dashboard_rows_),written{};
    FillConsoleOutputCharacterW(output_,L' ',cells,start,&written);FillConsoleOutputAttribute(output_,original_attributes_,cells,start,&written);SetConsoleCursorPosition(output_,start);
   }}
  dashboard_rows_=0;
 }
public:
 ConsoleOutput(){
  interactive_=output_&&output_!=INVALID_HANDLE_VALUE&&GetConsoleMode(output_,&original_mode_)&&std::getenv("NO_COLOR")==nullptr;
  if(!interactive_)return;CONSOLE_SCREEN_BUFFER_INFO info{};if(GetConsoleScreenBufferInfo(output_,&info))original_attributes_=info.wAttributes;
  SetConsoleOutputCP(CP_UTF8);auto requested=original_mode_|ENABLE_VIRTUAL_TERMINAL_PROCESSING;vt_=SetConsoleMode(output_,requested)!=FALSE;
 }
 ~ConsoleOutput(){if(interactive_){clear_dashboard();SetConsoleTextAttribute(output_,original_attributes_);SetConsoleMode(output_,original_mode_);}}
 bool interactive() const{return interactive_;}
 void event(const std::string& line){
  if(!interactive_){std::cout<<line<<'\n';std::cout.flush();return;}
  clear_dashboard();if(vt_)std::cout<<ansi(line)<<line<<"\x1b[0m\n";
  else{SetConsoleTextAttribute(output_,category(line));std::cout<<line<<'\n';SetConsoleTextAttribute(output_,original_attributes_);}std::cout.flush();
 }
 void dashboard(const LiveConsoleView& view){
  clear_dashboard();if(view.rows().empty())return;
  auto frame=view.render({vt_,vt_,false});
  if(vt_)std::cout<<frame;else{SetConsoleTextAttribute(output_,FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);std::cout<<frame;SetConsoleTextAttribute(output_,original_attributes_);}
  dashboard_rows_=view.rows().size();std::cout.flush();
 }
};
}
int wmain(int argc,wchar_t** argv){
 try{
  DWORD parent=0;auto root=data_directory();for(int i=1;i<argc;++i){if(std::wstring(argv[i])==L"--pid"&&i+1<argc)parent=static_cast<DWORD>(std::stoul(argv[++i]));else if(std::wstring(argv[i])==L"--data-root"&&i+1<argc)root=argv[++i];else throw std::runtime_error("Unknown console argument");}
  SetConsoleTitleW(L"Organizer Worker - Live Console (close safely to tray)");ConsoleOutput output;
  std::cout<<"Organizer Worker - live log viewer\nClosing this window does NOT stop jobs. Use the system tray to pause, set priority or exit.\n\n";
  Handle controller(parent?OpenProcess(SYNCHRONIZE,FALSE,parent):nullptr);if(parent&&!controller)fail("Locate tray controller");
  std::uint64_t position=0,last_file=0;std::array<char,16384> buffer{};std::string pending;LiveConsoleView view;
  while(!controller||WaitForSingleObject(controller.get(),0)==WAIT_TIMEOUT){
   Handle file(CreateFileW((root/"worker.log").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
   if(file){BY_HANDLE_FILE_INFORMATION info{};if(GetFileInformationByHandle(file.get(),&info)){auto identity=(static_cast<std::uint64_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow;if(identity!=last_file){last_file=identity;position=0;pending.clear();}}
    LARGE_INTEGER size{};if(GetFileSizeEx(file.get(),&size)){if(position>static_cast<std::uint64_t>(size.QuadPart)){position=0;pending.clear();}LARGE_INTEGER at{};at.QuadPart=static_cast<LONGLONG>(position);require(SetFilePointerEx(file.get(),at,nullptr,FILE_BEGIN),"Seek live log");for(;;){DWORD n=0;require(ReadFile(file.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&n,nullptr),"Read live log");if(!n)break;position+=n;
      if(!output.interactive()){std::cout.write(buffer.data(),n);continue;}
      pending.append(buffer.data(),n);std::size_t start=0;for(;;){auto end=pending.find('\n',start);if(end==std::string::npos)break;auto line=pending.substr(start,end-start);if(!line.empty()&&line.back()=='\r')line.pop_back();if(view.consume(line))output.dashboard(view);else{output.event(line);output.dashboard(view);}start=end+1;}if(start)pending.erase(0,start);
     }std::cout.flush();}}
   if(controller)WaitForSingleObject(controller.get(),250);else Sleep(250);
  }
  output.event("Worker exited.");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
