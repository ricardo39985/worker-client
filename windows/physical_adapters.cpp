#include "physical_adapters.hpp"
#include "platform.hpp"
#include <setupapi.h>
#include <vector>
#include <algorithm>
namespace ow::win {
namespace {
constexpr GUID display_class{0x4d36e968,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
struct Close{HDEVINFO value;~Close(){SetupDiDestroyDeviceInfoList(value);}};
}
std::vector<DisplayInstance> present_display_instances(){
 HDEVINFO devices=SetupDiGetClassDevsW(&display_class,nullptr,nullptr,DIGCF_PRESENT);
 if(devices==INVALID_HANDLE_VALUE)throw std::runtime_error("Windows Display device inventory unavailable");
 Close close{devices};std::vector<DisplayInstance> out;
 for(DWORD index=0;;++index){
  SP_DEVINFO_DATA entry{};entry.cbSize=sizeof(entry);
  if(!SetupDiEnumDeviceInfo(devices,index,&entry)){
   if(GetLastError()!=ERROR_NO_MORE_ITEMS)throw std::runtime_error("Windows Display device enumeration failed");
   break;
  }
  wchar_t id[4096]{},name[4096]{};
  if(!SetupDiGetDeviceInstanceIdW(devices,&entry,id,4096,nullptr))continue;
  DWORD type{};
  if(!SetupDiGetDeviceRegistryPropertyW(devices,&entry,SPDRP_FRIENDLYNAME,&type,reinterpret_cast<PBYTE>(name),sizeof(name),nullptr))
   SetupDiGetDeviceRegistryPropertyW(devices,&entry,SPDRP_DEVICEDESC,&type,reinterpret_cast<PBYTE>(name),sizeof(name),nullptr);
  name[4095]=0;out.push_back({utf8(id),utf8(name)});
 }
 return out;
}

std::string display_instance_id(LUID luid){
 DISPLAYCONFIG_ADAPTER_NAME name{};
 name.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_ADAPTER_NAME;
 name.header.size=sizeof(name);name.header.adapterId=luid;
 if(DisplayConfigGetDeviceInfo(&name.header)!=ERROR_SUCCESS)return {};
 HDEVINFO devices=SetupDiGetClassDevsW(&display_class,nullptr,nullptr,DIGCF_PRESENT);
 if(devices==INVALID_HANDLE_VALUE)return {};
 Close close{devices};
 std::vector<std::wstring> present;
 for(DWORD index=0;;++index){
  SP_DEVINFO_DATA entry{};entry.cbSize=sizeof(entry);
  if(!SetupDiEnumDeviceInfo(devices,index,&entry)){
   if(GetLastError()!=ERROR_NO_MORE_ITEMS)return {};
   break;
  }
  wchar_t id[4096]{};
  if(SetupDiGetDeviceInstanceIdW(devices,&entry,id,4096,nullptr))present.emplace_back(id);
 }
 SP_DEVICE_INTERFACE_DATA interface_data{};interface_data.cbSize=sizeof(interface_data);
 if(!SetupDiOpenDeviceInterfaceW(devices,name.adapterDevicePath,0,&interface_data))return {};
 SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);DWORD needed{};
 // SetupAPI supplies the associated device even when only the detail buffer
 // size is requested. Allocate it and require successful detail retrieval.
 SetupDiGetDeviceInterfaceDetailW(devices,&interface_data,nullptr,0,&needed,nullptr);
 if(!needed||needed>65536)return {};
 std::vector<unsigned char> buffer(needed);
 auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
 detail->cbSize=sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
 if(!SetupDiGetDeviceInterfaceDetailW(devices,&interface_data,detail,needed,nullptr,&device))return {};
 // Display setup class, including ROOT software displays. This prevents an
 // interface resolving to an unrelated device from establishing GPU capacity.
 if(!IsEqualGUID(device.ClassGuid,display_class))return {};
 wchar_t instance[4096]{};
 if(!SetupDiGetDeviceInstanceIdW(devices,&device,instance,4096,nullptr))return {};
 if(std::none_of(present.begin(),present.end(),[&](const auto& id){return _wcsicmp(id.c_str(),instance)==0;}))return {};
 return utf8(instance);
}
}
