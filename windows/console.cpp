#include "platform.hpp"
#include "ow/console.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace ow;using namespace ow::win;
namespace {
class ConsoleOutput {
 HANDLE output_{GetStdHandle(STD_OUTPUT_HANDLE)};DWORD original_mode_{};WORD original_attributes_{FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE};
 bool interactive_{},color_{},vt_{};std::size_t dashboard_rows_{};
 WORD attribute(ConsoleColor color) const {
  switch(color){
   case ConsoleColor::red:return FOREGROUND_RED|FOREGROUND_INTENSITY;
   case ConsoleColor::orange:return FOREGROUND_RED|FOREGROUND_GREEN;
   case ConsoleColor::green:return FOREGROUND_GREEN|FOREGROUND_INTENSITY;
   case ConsoleColor::blue:return FOREGROUND_BLUE|FOREGROUND_INTENSITY;
   case ConsoleColor::white:return original_attributes_;
  }
  return original_attributes_;
 }
 bool dashboard_on() const{return interactive_&&color_;}
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
  interactive_=output_&&output_!=INVALID_HANDLE_VALUE&&GetConsoleMode(output_,&original_mode_);
  color_=interactive_&&std::getenv("NO_COLOR")==nullptr;
  if(!interactive_)return;CONSOLE_SCREEN_BUFFER_INFO info{};if(GetConsoleScreenBufferInfo(output_,&info))original_attributes_=info.wAttributes;
  SetConsoleOutputCP(CP_UTF8);
  if(color_){auto requested=original_mode_|ENABLE_VIRTUAL_TERMINAL_PROCESSING;vt_=SetConsoleMode(output_,requested)!=FALSE;}
 }
 ~ConsoleOutput(){if(color_){clear_dashboard();SetConsoleTextAttribute(output_,original_attributes_);SetConsoleMode(output_,original_mode_);}}
 bool interactive() const{return interactive_;}
 bool dashboard_enabled() const{return dashboard_on();}
 void event(const ConsoleLine& line){
  if(!interactive_){std::cout<<line.text<<'\n';std::cout.flush();return;}
  clear_dashboard();
  if(color_){
   if(vt_)std::cout<<ansi_color(line.color)<<line.text<<"\x1b[0m\n";
   else{SetConsoleTextAttribute(output_,attribute(line.color));std::cout<<line.text<<'\n';SetConsoleTextAttribute(output_,original_attributes_);}
  }else std::cout<<line.text<<'\n';
  std::cout.flush();
 }
 void dashboard(const LiveConsoleView& view){
  if(!dashboard_on())return;
  clear_dashboard();if(view.rows().empty())return;
  if(vt_){std::string frame;for(const auto& row:view.rows())frame+=view.render_row(row,true)+'\n';std::cout<<frame;}
  else for(const auto& row:view.rows()){SetConsoleTextAttribute(output_,attribute(row.color));std::cout<<view.render_row(row,false)<<'\n';SetConsoleTextAttribute(output_,original_attributes_);}
  dashboard_rows_=view.rows().size();std::cout.flush();
 }
};
}
int wmain(int argc,wchar_t** argv){
 try{
  DWORD parent=0;auto root=data_directory();for(int i=1;i<argc;++i){if(std::wstring(argv[i])==L"--pid"&&i+1<argc)parent=static_cast<DWORD>(std::stoul(argv[++i]));else if(std::wstring(argv[i])==L"--data-root"&&i+1<argc)root=argv[++i];else throw std::runtime_error("Unknown console argument");}
  SetConsoleTitleW(L"Organizer Worker - Live Console (close safely to tray)");ConsoleOutput output;
  Handle controller(parent?OpenProcess(SYNCHRONIZE,FALSE,parent):nullptr);if(parent&&!controller)fail("Locate tray controller");
  std::uint64_t position=0,last_file=0;std::array<char,16384> buffer{};std::string pending;LiveConsoleView view;
  while(!controller||WaitForSingleObject(controller.get(),0)==WAIT_TIMEOUT){
   Handle file(CreateFileW((root/"worker.log").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
   if(file){BY_HANDLE_FILE_INFORMATION info{};if(GetFileInformationByHandle(file.get(),&info)){auto identity=(static_cast<std::uint64_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow;if(identity!=last_file){last_file=identity;position=0;pending.clear();}}
    LARGE_INTEGER size{};if(GetFileSizeEx(file.get(),&size)){if(position>static_cast<std::uint64_t>(size.QuadPart)){position=0;pending.clear();}LARGE_INTEGER at{};at.QuadPart=static_cast<LONGLONG>(position);require(SetFilePointerEx(file.get(),at,nullptr,FILE_BEGIN),"Seek live log");for(;;){DWORD n=0;require(ReadFile(file.get(),buffer.data(),static_cast<DWORD>(buffer.size()),&n,nullptr),"Read live log");if(!n)break;position+=n;
      pending.append(buffer.data(),n);std::size_t start=0;for(;;){auto end=pending.find('\n',start);if(end==std::string::npos)break;auto line=pending.substr(start,end-start);if(!line.empty()&&line.back()=='\r')line.pop_back();auto feed=view.consume(line);if(feed.event)output.event(*feed.event);if(feed.activity&&!output.dashboard_enabled())output.event(*feed.activity);if(feed.redraw&&output.dashboard_enabled())output.dashboard(view);start=end+1;}if(start)pending.erase(0,start);
     }std::cout.flush();}}
   if(controller)WaitForSingleObject(controller.get(),250);else Sleep(250);
  }
  output.event({format_local_timestamp(utc_ms())+" worker exited",ConsoleColor::white});return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
