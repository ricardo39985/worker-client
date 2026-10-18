#pragma once
#include "ow/adaptive_hardware.hpp"
#include <set>
namespace ow::adaptive {
// Windows device instance identifiers are case insensitive. Product IDs, DXGI
// names/LUIDs and CUDA UUIDs alone cannot establish a physical PCI card.
inline std::string canonical_instance_id(std::string id){
 for(auto& c:id)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
 return id;
}
inline bool pci_instance_id(const std::string& id){
 const auto key=canonical_instance_id(id);
 if(key.rfind("PCI\\",0)!=0)return false;
 const auto location=key.find('\\',4);
 return location!=std::string::npos&&location>4&&location+1<key.size()&&
  key.find_first_of("\r\n\t ")==std::string::npos;
}
// Keep excluded adapters for diagnostics, but never tune or schedule them.
inline void verify_device_identities(std::vector<Device>& devices){
 for(auto& d:devices)if(d.family!=Backend::cpu){
  d.physical_id=canonical_instance_id(d.physical_id);
  if(!pci_instance_id(d.physical_id)){
   d.exclusion_reason=d.physical_id.rfind("ROOT\\",0)==0?
    "software adapter (ROOT instance); excluded from capacity and scheduling":
    "unmappable adapter: no physical PCI instance; GPU profile withheld";
   d.backend_identity_verified=false;
  }else if(!d.backend_identity_verified||d.stable_id.empty()){
   d.exclusion_reason="unproven encoder mapping; GPU profile withheld";
   d.backend_identity_verified=false;
  }else d.exclusion_reason.clear();
 }
 const auto inventory=devices;
 for(auto& d:devices){
  if(d.family==Backend::cpu||!d.backend_identity_verified)continue;
  const auto matches=std::count_if(inventory.begin(),inventory.end(),[&](const auto& other){
   if(other.family==Backend::cpu||!other.backend_identity_verified)return false;
   return other.stable_id==d.stable_id||other.physical_id==d.physical_id||
    (other.family==d.family&&other.backend_index==d.backend_index);
  });
  if(matches!=1){d.backend_identity_verified=false;d.exclusion_reason="ambiguous PCI instance or encoder alias; GPU profile withheld";}
 }
}
inline std::size_t schedulable_device_count(const std::vector<Profile>& profiles){
 std::set<std::string> ids;
 for(const auto& p:profiles)if(p.backend!=Backend::cpu&&p.backend_identity_verified&&p.max_concurrency&&pci_instance_id(p.physical_id))
  ids.insert(canonical_instance_id(p.physical_id));
 return ids.size();
}
// At most one encoder session is reserved on a card, including all sequential
// codecs of a rendition job. Do not count those codecs as separate VRAM pools.
inline std::uint64_t profile_vram_capacity(const std::vector<Profile>& profiles){
 std::map<std::string,std::uint64_t> capacities;
 for(const auto& p:profiles)if(p.backend!=Backend::cpu&&p.backend_identity_verified&&p.max_concurrency&&pci_instance_id(p.physical_id))
  capacities[canonical_instance_id(p.physical_id)]=std::max(capacities[canonical_instance_id(p.physical_id)],p.vram_mb*p.proven_sessions);
 std::uint64_t total=0;for(const auto& [_,mb]:capacities)total+=mb;return total;
}
}
