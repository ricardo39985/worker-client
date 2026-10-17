#include "hardware.hpp"
#include "ow/device_identity.hpp"
#include <windows.h>
#include <dxgi1_2.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <iomanip>
#include <memory>
#include <map>
#include <sstream>
#include <cwchar>
#include <regex>
#include <cstring>
#include <type_traits>
namespace ow::win {
namespace {
struct ComRelease { template<class T> void operator()(T* p) const { if(p)p->Release(); } };
struct ModuleRelease { void operator()(HMODULE p) const { if(p)FreeLibrary(p); } };
adaptive::Backend backend_for(UINT vendor){
 if(vendor==0x10de)return adaptive::Backend::nvidia_nvenc;
 if(vendor==0x8086)return adaptive::Backend::intel_qsv;
 if(vendor==0x1002)return adaptive::Backend::amd_amf;
 return adaptive::Backend::cpu;
}
std::string adapter_id(const DXGI_ADAPTER_DESC1& desc){
 std::ostringstream id;id<<"pci:"<<std::hex<<std::setfill('0')<<std::setw(4)<<desc.VendorId<<":"
   <<std::setw(4)<<desc.DeviceId<<":"<<std::setw(8)<<desc.SubSysId<<":"<<std::setw(2)<<desc.Revision
   <<":luid:"<<std::setw(8)<<static_cast<unsigned long>(desc.AdapterLuid.HighPart)<<":"
   <<std::setw(8)<<desc.AdapterLuid.LowPart;return id.str();
}
struct CudaMapping { std::vector<adaptive::BackendIdentity> devices;std::map<std::string,std::string> physical_ids; };
CudaMapping cuda_mappings(){
 CudaMapping result;
 std::unique_ptr<std::remove_pointer_t<HMODULE>,ModuleRelease> module(
   LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32));
 if(!module)return result;
 using Init=int (WINAPI*)(unsigned);using Count=int (WINAPI*)(int*);using Get=int (WINAPI*)(int*,int);using Luid=int (WINAPI*)(char*,unsigned*,int);
 auto init=reinterpret_cast<Init>(GetProcAddress(module.get(),"cuInit"));
 auto count=reinterpret_cast<Count>(GetProcAddress(module.get(),"cuDeviceGetCount"));
 auto get=reinterpret_cast<Get>(GetProcAddress(module.get(),"cuDeviceGet"));
 struct CudaUuid {char bytes[16];};using Uuid=int (WINAPI*)(CudaUuid*,int);
 auto uuid=reinterpret_cast<Uuid>(GetProcAddress(module.get(),"cuDeviceGetUuid"));
 auto luid=reinterpret_cast<Luid>(GetProcAddress(module.get(),"cuDeviceGetLuid"));
 if(!init||!count||!get||!luid||!uuid||init(0)!=0)return result;
 int devices=0;if(count(&devices)!=0||devices<=0||devices>64)return result;
 for(int ordinal=0;ordinal<devices;++ordinal){
  int device{};char bytes[8]{};unsigned node_mask{};CudaUuid physical{};
  if(get(&device,ordinal)!=0||luid(bytes,&node_mask,device)!=0||!node_mask||uuid(&physical,device)!=0)continue;
  if(std::all_of(physical.bytes,physical.bytes+16,[](char b){return b==0;}))continue;
  DWORD low{};LONG high{};std::memcpy(&low,bytes,sizeof(low));std::memcpy(&high,bytes+sizeof(low),sizeof(high));
  std::ostringstream key;key<<std::hex<<std::setfill('0')<<std::setw(8)<<static_cast<unsigned long>(high)<<":"<<std::setw(8)<<low;
  result.devices.push_back({key.str(),static_cast<std::uint32_t>(ordinal),node_mask});
  std::ostringstream physical_key;physical_key<<"cuda-uuid:"<<std::hex<<std::setfill('0');
  for(unsigned char byte:physical.bytes)physical_key<<std::setw(2)<<static_cast<unsigned>(byte);
  result.physical_ids[key.str()]=physical_key.str();
 }
 return result;
}
std::string luid_key(const std::string& stable_id){
 const auto at=stable_id.find(":luid:");if(at==std::string::npos)return {};
 return stable_id.substr(at+6);
}
bool instance_for_adapter(const std::wstring& name,const std::string& key,std::uint32_t& pid,std::uint32_t& node){
 const int size=WideCharToMultiByte(CP_UTF8,0,name.data(),static_cast<int>(name.size()),nullptr,0,nullptr,nullptr);
 if(size<=0)return false;std::string instance(static_cast<std::size_t>(size),0);
 if(!WideCharToMultiByte(CP_UTF8,0,name.data(),static_cast<int>(name.size()),instance.data(),size,nullptr,nullptr))return false;
 std::smatch match;
 static const std::regex parts(R"(pid_([0-9]+)_luid_0x([0-9a-fA-F]{8})_0x([0-9a-fA-F]{8})_phys_([0-9]+))",std::regex::icase);
 if(!std::regex_search(instance,match,parts))return false;
 const auto colon=key.find(':');if(colon==std::string::npos)return false;
 std::string high=key.substr(0,colon),low=key.substr(colon+1);
 std::transform(high.begin(),high.end(),high.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 std::transform(low.begin(),low.end(),low.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 const std::string seen=match[2].str()+":"+match[3].str();
 std::string normalized=seen;std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 if(normalized!=high+":"+low)return false;
 try{pid=static_cast<std::uint32_t>(std::stoul(match[1].str()));node=static_cast<std::uint32_t>(std::stoul(match[4].str()));}
 catch(...){return false;}
 return true;
}
}
std::vector<adaptive::Device> enumerate_video_devices(bool resolve_encoder_identity){
 IDXGIFactory1* raw_factory=nullptr;
 auto hr=CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&raw_factory));
 if(FAILED(hr))throw std::runtime_error("DirectX adapter inventory unavailable");
 std::unique_ptr<IDXGIFactory1,ComRelease> factory(raw_factory);
 std::vector<adaptive::Device> out;
 const auto cuda=resolve_encoder_identity?cuda_mappings():CudaMapping{};std::map<adaptive::Backend,std::uint32_t> backend_indices;
 for(UINT index=0;;++index){
  IDXGIAdapter1* raw_adapter=nullptr;hr=factory->EnumAdapters1(index,&raw_adapter);
  if(hr==DXGI_ERROR_NOT_FOUND)break;
  if(FAILED(hr))throw std::runtime_error("DirectX adapter enumeration failed");
  std::unique_ptr<IDXGIAdapter1,ComRelease> adapter(raw_adapter);DXGI_ADAPTER_DESC1 desc{};
  if(FAILED(adapter->GetDesc1(&desc))||(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;
  const auto family=backend_for(desc.VendorId);if(family==adaptive::Backend::cpu)continue;
  LARGE_INTEGER driver{};const auto driver_hr=adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver);
  const auto id=adapter_id(desc);
  std::string driver_version;
  if(SUCCEEDED(driver_hr)){std::ostringstream value;value<<std::hex<<static_cast<unsigned long>(driver.HighPart)<<"."<<driver.LowPart;driver_version=value.str();}
  const auto vram=static_cast<std::uint64_t>(desc.DedicatedVideoMemory)/(1024*1024);
  const std::wstring description(desc.Description);
  int utf8_length=WideCharToMultiByte(CP_UTF8,0,description.data(),static_cast<int>(description.size()),nullptr,0,nullptr,nullptr);
  std::string name(utf8_length>0?static_cast<std::size_t>(utf8_length):0,0);
  if(utf8_length>0)WideCharToMultiByte(CP_UTF8,0,description.data(),static_cast<int>(description.size()),name.data(),utf8_length,nullptr,nullptr);
  std::uint32_t backend_index=index;bool mapped=false;std::string physical_id;
  if(family==adaptive::Backend::nvidia_nvenc){
   if(auto ordinal=adaptive::resolve_unique_backend_ordinal(cuda.devices,luid_key(id))){backend_index=*ordinal;physical_id=cuda.physical_ids.at(luid_key(id));mapped=true;}
  }else if(resolve_encoder_identity) { backend_index=backend_indices[family]++;mapped=true; }
  out.push_back({id,family,driver_version,vram,SUCCEEDED(driver_hr)&&vram>0,index,std::move(name),backend_index,mapped,std::move(physical_id)});
 }
 adaptive::verify_device_identities(out);
 return out;
}
std::optional<GpuMemorySample> sample_gpu_memory(std::uint32_t process_id,const adaptive::Device& device){
 if(device.family==adaptive::Backend::cpu||!device.backend_identity_verified)return {};
 const auto key=luid_key(device.stable_id);if(key.empty())return {};
 PDH_HQUERY query{};if(PdhOpenQueryW(nullptr,0,&query)!=ERROR_SUCCESS)return {};
 struct QueryClose{PDH_HQUERY q;~QueryClose(){if(q)PdhCloseQuery(q);}} close{query};
 PDH_HCOUNTER counter{};
 if(PdhAddEnglishCounterW(query,L"\\GPU Process Memory(*)\\Dedicated Usage",0,&counter)!=ERROR_SUCCESS)return {};
 if(PdhCollectQueryData(query)!=ERROR_SUCCESS)return {};
 DWORD bytes=0,count=0;auto status=PdhGetFormattedCounterArrayW(counter,PDH_FMT_LARGE,&bytes,&count,nullptr);
 if(status!=PDH_MORE_DATA||!bytes||bytes>16*1024*1024)return {};
 std::vector<unsigned char> storage(bytes);auto* items=reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_W>(storage.data());
 status=PdhGetFormattedCounterArrayW(counter,PDH_FMT_LARGE,&bytes,&count,items);if(status!=ERROR_SUCCESS)return {};
 std::vector<adaptive::GpuMemoryRecord> records;
 for(DWORD i=0;i<count;++i){
  std::uint32_t owner{},node{};if(!instance_for_adapter(items[i].szName,key,owner,node))continue;
  if((items[i].FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA&&items[i].FmtValue.CStatus!=PDH_CSTATUS_NEW_DATA)||items[i].FmtValue.largeValue<=0)return {};
  records.push_back({owner,node,static_cast<std::uint64_t>(items[i].FmtValue.largeValue),device.stable_id,true});
 }
 auto totals=adaptive::gpu_memory_totals(records,process_id,device.stable_id);if(!totals)return {};
 return GpuMemorySample{totals->child_bytes,totals->adapter_bytes};
}
}
