// Opt-in comparison against raw current WDDM counters. Requires physical GPUs;
// never registered as an automatic test, and never performs an encode.
#include "hardware.hpp"
#include "platform.hpp"
#include <pdh.h>
#include <pdhmsg.h>
#include <iostream>
using namespace ow;using namespace ow::win;
int wmain(){try{
 unsigned checked=0,failures=0;
 for(const auto& device:enumerate_video_devices()){
  if(!device.backend_identity_verified)continue;
  const auto at=device.stable_id.find(":luid:");if(at==std::string::npos)continue;
  const auto key=device.stable_id.substr(at+6);const auto colon=key.find(':');
  auto needle=wide("luid_0x"+key.substr(0,colon)+"_0x"+key.substr(colon+1)+"_phys_");
  PDH_HQUERY query{};if(PdhOpenQueryW(nullptr,0,&query))continue;
  struct Close{PDH_HQUERY q;~Close(){PdhCloseQuery(q);}}close{query};PDH_HCOUNTER counter{};
  if(PdhAddEnglishCounterW(query,L"\\GPU Process Memory(*)\\Dedicated Usage",0,&counter)||PdhCollectQueryData(query))continue;
  DWORD bytes=0,count=0;if(PdhGetFormattedCounterArrayW(counter,PDH_FMT_LARGE,&bytes,&count,nullptr)!=PDH_MORE_DATA||bytes>16*1024*1024)continue;
  std::vector<unsigned char> data(bytes);auto values=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(data.data());
  if(PdhGetFormattedCounterArrayW(counter,PDH_FMT_LARGE,&bytes,&count,values))continue;
  std::uint64_t positive=0;unsigned zero=0,invalid=0;
  for(DWORD n=0;n<count;++n){const std::wstring name=values[n].szName;if(name.find(needle)==std::wstring::npos)continue;
   const auto& value=values[n].FmtValue;
   if((value.CStatus!=PDH_CSTATUS_VALID_DATA&&value.CStatus!=PDH_CSTATUS_NEW_DATA)||value.largeValue<0){++invalid;continue;}
   if(!value.largeValue)++zero;else positive+=static_cast<std::uint64_t>(value.largeValue);
  }
  const auto measured=sample_gpu_memory(0,device);
  std::cout<<json::serialize(json::object{{"device",device.name},{"raw_positive_bytes",positive},{"raw_zero_records",zero},{"raw_invalid_records",invalid},{"worker_known",static_cast<bool>(measured)},{"worker_bytes",measured?measured->adapter_bytes:0}})<<std::endl;
  if(positive&&!invalid){++checked;if(!measured||!measured->adapter_bytes)++failures;}
 }
 std::cout<<"RESOURCE_CHECKED="<<checked<<" RESOURCE_FAILURES="<<failures<<std::endl;
 return !checked?2:failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 3;}}
