#include "capacity.hpp"
#include <pdh.h>
#include <pdhmsg.h>
#include <thread>
#include <wtsapi32.h>
#include <algorithm>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <set>
namespace ow::win {
namespace {
std::uint64_t ticks(FILETIME t){return (std::uint64_t{t.dwHighDateTime}<<32)|t.dwLowDateTime;}
bool foreground(){
 const auto session=WTSGetActiveConsoleSessionId();
 if(session==0xffffffff)return false;
 DWORD own=0;if(ProcessIdToSessionId(GetCurrentProcessId(),&own)&&own==session){
  LASTINPUTINFO input{sizeof(input)};if(GetLastInputInfo(&input)){
   if(static_cast<DWORD>(GetTickCount()-input.dwTime)<3000)return true;
   if(auto window=GetForegroundWindow()){DWORD_PTR reply{};const auto begin=monotonic_ms();
    if(!SendMessageTimeoutW(window,WM_NULL,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,16,&reply)||monotonic_ms()-begin>=16)return true;
   }
   return false;
  }
 }
 LPWSTR bytes=nullptr;DWORD size=0;
 if(WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSSessionInfo,&bytes,&size)){
  const auto* info=reinterpret_cast<const WTSINFOW*>(bytes);
  const auto last=size>=sizeof(WTSINFOW)?info->LastInputTime.QuadPart:0;
  WTSFreeMemory(bytes);FILETIME now{};GetSystemTimeAsFileTime(&now);
  if(last>0)return ticks(now)>=static_cast<std::uint64_t>(last)&&ticks(now)-static_cast<std::uint64_t>(last)<30000000;
 }
 // Unknown foreground state never masquerades as an idle desktop.
 return true;
}
struct Meter {
 PDH_HQUERY query{};PDH_HCOUNTER disk{},gpu{},gpu_memory{};std::uint64_t idle{},total{};bool cpu_seen{};
 explicit Meter(const std::wstring& volume){
  if(PdhOpenQueryW(nullptr,0,&query)!=ERROR_SUCCESS)return;
  const auto name=L"\\LogicalDisk("+volume+L")\\% Idle Time";
  if(PdhAddEnglishCounterW(query,name.c_str(),0,&disk)!=ERROR_SUCCESS)disk=nullptr;
  if(PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&gpu)!=ERROR_SUCCESS)gpu=nullptr;
  if(PdhAddEnglishCounterW(query,L"\\GPU Adapter Memory(*)\\Dedicated Usage",0,&gpu_memory)!=ERROR_SUCCESS)gpu_memory=nullptr;
 }
 ~Meter(){if(query)PdhCloseQuery(query);}
 RuntimePressure sample(){
  RuntimePressure out;out.host.foreground=foreground();
  MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);out.host.ram_percent=GlobalMemoryStatusEx(&memory)?std::optional<double>{static_cast<double>(memory.dwMemoryLoad)}:std::nullopt;
  FILETIME i{},k{},u{};if(GetSystemTimes(&i,&k,&u)){
   const auto current=ticks(k)+ticks(u),idle_now=ticks(i);
   if(cpu_seen&&current>total&&idle_now>=idle){out.host.cpu_percent=100.0*(1.0-static_cast<double>(idle_now-idle)/static_cast<double>(current-total));}
   total=current;idle=idle_now;cpu_seen=true;
  }
  if(!query||PdhCollectQueryData(query)!=ERROR_SUCCESS)return out;
  PDH_FMT_COUNTERVALUE value{};
  if(disk&&PdhGetFormattedCounterValue(disk,PDH_FMT_DOUBLE,nullptr,&value)==ERROR_SUCCESS&&value.CStatus==ERROR_SUCCESS)
   out.host.disk_percent=std::clamp(100.0-value.doubleValue,0.0,100.0);
  if(gpu_memory){
   DWORD size=0,items=0;
   if(PdhGetFormattedCounterArrayW(gpu_memory,PDH_FMT_LARGE,&size,&items,nullptr)==PDH_MORE_DATA&&size&&size<=16*1024*1024){
    std::vector<unsigned char> data(size);auto values=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data());
    if(PdhGetFormattedCounterArrayW(gpu_memory,PDH_FMT_LARGE,&size,&items,values)==ERROR_SUCCESS){
     static const std::wregex id(L"luid_0x([0-9a-fA-F]{8})_0x([0-9a-fA-F]{8})_phys_[0-9]+");
     std::set<std::string> invalid;
     for(DWORD n=0;n<items;++n){const auto& value=values[n].FmtValue;
      const std::wstring name=values[n].szName;std::wsmatch match;if(!std::regex_search(name,match,id))continue;
      std::ostringstream key;key<<std::hex<<std::setfill('0')<<std::setw(8)<<std::stoul(match[1].str(),nullptr,16)<<':'<<std::setw(8)<<std::stoul(match[2].str(),nullptr,16);
      if((value.CStatus!=PDH_CSTATUS_VALID_DATA&&value.CStatus!=PDH_CSTATUS_NEW_DATA)||value.largeValue<0){invalid.insert(key.str());continue;}
      out.dedicated_vram_bytes[key.str()]+=static_cast<std::uint64_t>(value.largeValue);
     }
     for(const auto& key:invalid)out.dedicated_vram_bytes.erase(key);
    }
   }
  }
  if(!gpu)return out;
  DWORD bytes=0,count=0;
  if(PdhGetFormattedCounterArrayW(gpu,PDH_FMT_DOUBLE,&bytes,&count,nullptr)!=PDH_MORE_DATA||!bytes||bytes>16*1024*1024)return out;
  std::vector<unsigned char> buffer(bytes);auto* entries=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
  if(PdhGetFormattedCounterArrayW(gpu,PDH_FMT_DOUBLE,&bytes,&count,entries)!=ERROR_SUCCESS)return out;
  std::map<std::pair<std::string,std::string>,double> engines;
  static const std::wregex pattern(L"luid_0x([0-9a-fA-F]{8})_0x([0-9a-fA-F]{8}).*_eng_([0-9]+)_");
  for(DWORD n=0;n<count;++n){
   if(entries[n].FmtValue.CStatus!=ERROR_SUCCESS)continue;
   const std::wstring name=entries[n].szName;std::wsmatch match;
   if(!std::regex_search(name,match,pattern))continue;
   std::ostringstream luid;luid<<std::hex<<std::setfill('0')<<std::setw(8)<<std::stoul(match[1].str(),nullptr,16)<<':'<<std::setw(8)<<std::stoul(match[2].str(),nullptr,16);
   engines[{luid.str(),utf8(match[3].str())}]+=std::max(0.0,entries[n].FmtValue.doubleValue);
  }
  for(const auto& [key,busy]:engines)out.gpu_busy[key.first]=std::max(out.gpu_busy[key.first],std::min(100.0,busy));
  return out;
 }
};
}
RuntimePressure sample_runtime_pressure(const std::filesystem::path& root){
 static std::mutex mutex;static std::map<std::pair<std::wstring,std::thread::id>,std::unique_ptr<Meter>> meters;
 std::lock_guard lock(mutex);auto volume=root.root_name().wstring();auto& meter=meters[{volume,std::this_thread::get_id()}];
 if(!meter)meter=std::make_unique<Meter>(volume);return meter->sample();
}
Priority desktop_priority(Priority requested){return (requested==Priority::maximum||requested==Priority::normal)&&foreground()?Priority::low:requested;}
void background_gpu_process(HANDLE process) noexcept{
 // WDDM scheduling priority is a hint, not a utilization/memory hard quota.
 using SetGpuPriority=LONG(WINAPI*)(HANDLE,int);
 const auto module=GetModuleHandleW(L"gdi32.dll");
 if(module)if(auto set=reinterpret_cast<SetGpuPriority>(GetProcAddress(module,"D3DKMTSetProcessSchedulingPriorityClass")))set(process,1);
}
}
