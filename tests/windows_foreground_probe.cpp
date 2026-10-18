// Passive, normal-user interactive-session observer. No key/window contents,
// activation, synthetic input or control channel. Everything stays in staging.
#include "platform.hpp"
#include <fstream>
#include <shellapi.h>
using namespace ow;using namespace ow::win;
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){try{
 int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return 2;
 if(argc!=2){LocalFree(argv);return 2;}const std::filesystem::path root=argv[1];LocalFree(argv);
 const auto approved=data_directory().parent_path()/"Temp"/"ow-worker-flex-20261018-r2";
 if(root.lexically_normal()!=approved.lexically_normal())return 2;
 DWORD session=0;ProcessIdToSessionId(GetCurrentProcessId(),&session);
 if(session!=WTSGetActiveConsoleSessionId())return 3;
 const auto until=monotonic_ms()+7200000;
 while(monotonic_ms()<until&&!std::filesystem::exists(root/"observer.stop")){
  LASTINPUTINFO input{sizeof(input)};if(!GetLastInputInfo(&input))return 4;
  const auto age=static_cast<DWORD>(GetTickCount()-input.dwTime);
  const auto window=GetForegroundWindow();DWORD_PTR reply{};const auto begin=monotonic_ms();
  const bool responsive=window&&SendMessageTimeoutW(window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,100,&reply)!=0;
  json::object record{{"observed_utc_ms",utc_ms()},{"session",session},{"pid",GetCurrentProcessId()},{"input_age_ms",age},{"foreground_active",age<3000},{"window_present",window!=nullptr},{"message_response_known",responsive},{"message_response_ms",monotonic_ms()-begin}};
  const auto line=json::serialize(record);for(unsigned retry=0;;++retry){try{atomic_write(root/"foreground.json",line);break;}catch(...){if(retry==5)throw;Sleep(10);}}
  std::ofstream(root/"foreground-samples.jsonl",std::ios::app|std::ios::binary)<<line<<'\n';Sleep(100);
 }
 return 0;
}catch(...){return 1;}}
