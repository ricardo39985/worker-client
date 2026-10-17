#pragma once
#include "ow/adaptive_hardware.hpp"
#include <set>
namespace ow::adaptive {
// Retain raw inventory for diagnostics. Only unique, proven backend/physical
// identities may feed tuning or placement. Never deduplicate by product alone.
inline void verify_device_identities(std::vector<Device>& devices){
 const auto inventory=devices;
 for(auto& d:devices){
  if(d.family==Backend::cpu)continue;
  if(!d.backend_identity_verified||d.stable_id.empty()||
     (d.family==Backend::nvidia_nvenc&&d.physical_id.empty())){d.backend_identity_verified=false;continue;}
  const auto product=d.stable_id.substr(0,d.stable_id.find(":luid:"));
  const auto matches=std::count_if(inventory.begin(),inventory.end(),[&](const auto& other){
   if(other.family==Backend::cpu||!other.backend_identity_verified)return false;
   if(other.stable_id==d.stable_id)return true;
   if(!d.physical_id.empty()&&other.physical_id==d.physical_id)return true;
   if(other.family!=d.family)return false;
   if(other.backend_index==d.backend_index)return true;
   // Non-CUDA mappings lack a physical UUID. With repeated PCI products,
   // fail closed rather than invent a second card from another DXGI LUID.
   return d.family!=Backend::nvidia_nvenc&&d.stable_id.find(":luid:")!=std::string::npos&&
          other.stable_id.substr(0,other.stable_id.find(":luid:"))==product;
  });
  if(matches!=1)d.backend_identity_verified=false;
 }
}
inline std::size_t schedulable_device_count(const std::vector<Profile>& profiles){
 std::set<std::string> ids;
 for(const auto& p:profiles)if(p.backend!=Backend::cpu&&p.backend_identity_verified&&p.max_concurrency)
  ids.insert(p.physical_id.empty()?p.stable_id:p.physical_id);
 return ids.size();
}
// At most one encoder session is reserved on a card, including all sequential
// codecs of a rendition job. Do not count those codecs as separate VRAM pools.
inline std::uint64_t profile_vram_capacity(const std::vector<Profile>& profiles){
 std::map<std::string,std::uint64_t> capacities;
 for(const auto& p:profiles)if(p.backend!=Backend::cpu&&p.backend_identity_verified&&p.max_concurrency)
  capacities[p.stable_id]=std::max(capacities[p.stable_id],p.vram_mb);
 std::uint64_t total=0;for(const auto& [_,mb]:capacities)total+=mb;return total;
}
}
