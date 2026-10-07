#include "ow/admission.hpp"
#include <stdexcept>
#include <algorithm>
namespace ow {
namespace {
bool fits(Resources n,Resources b,Resources u={}){
 return u.ram_mb<=b.ram_mb && n.ram_mb<=b.ram_mb-u.ram_mb &&
        u.vram_mb<=b.vram_mb && n.vram_mb<=b.vram_mb-u.vram_mb &&
        u.scratch_mb<=b.scratch_mb && n.scratch_mb<=b.scratch_mb-u.scratch_mb &&
        u.cpu_threads<=b.cpu_threads && n.cpu_threads<=b.cpu_threads-u.cpu_threads;
}
}
bool safe_id(const std::string& s){
 return !s.empty() && s.size()<=128 && std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
Admission::Admission(Resources b,std::size_t m):budget_(b),max_jobs_(m){
 if(!m||m>64||!b.ram_mb||!b.cpu_threads)throw std::invalid_argument("invalid host resource budget");
}
void Admission::capability(std::string name,bool verified,Preference preference){
 std::lock_guard lock(mutex_);capabilities_.insert_or_assign(std::move(name),Capability{verified,preference});
}
void Admission::pause(bool value){std::lock_guard lock(mutex_);paused_=value;}
Decision Admission::offer(const Offer& o,Tick now,Resources available){
 std::lock_guard lock(mutex_);
 if(!safe_id(o.job_id)||!safe_id(o.attempt_id)||o.fingerprint.empty()||!o.resources.ram_mb||!o.resources.cpu_threads||o.reserve_until<=now||o.reserve_until-now>60000)return {false,"invalid_offer"};
 if(auto i=active_.find(o.attempt_id);i!=active_.end()){
   if(i->second.offer.fingerprint!=o.fingerprint||i->second.offer.job_id!=o.job_id||i->second.offer.capability!=o.capability||i->second.offer.resources!=o.resources)return {false,"attempt_conflict"};
   // Retransmission must not extend the original reservation or revive cancellation.
   return {i->second.phase!=Phase::cancelling && (i->second.phase==Phase::running?i->second.deadline>now:i->second.offer.reserve_until>now),"duplicate"};
 }
 if(paused_)return {false,"paused"};
 auto c=capabilities_.find(o.capability);
 if(c==capabilities_.end()||!c->second.verified)return {false,"unsupported"};
 if(c->second.preference==Preference::disabled)return {false,"disabled"};
 for(const auto& [_,a]:active_)if(a.offer.job_id==o.job_id)return {false,"job_already_active"};
 if(active_.size()>=max_jobs_||!fits(o.resources,budget_,used_)||!fits(o.resources,available,used_))return {false,"busy"};
 used_.ram_mb+=o.resources.ram_mb;used_.vram_mb+=o.resources.vram_mb;used_.scratch_mb+=o.resources.scratch_mb;used_.cpu_threads+=o.resources.cpu_threads;
 active_.emplace(o.attempt_id,Active{o,Phase::reserved,{},o.reserve_until,0});return {true,"ready"};
}
Start Admission::begin(const std::string& id,std::string token,Tick deadline,Tick now){
 std::lock_guard lock(mutex_);auto i=active_.find(id);
 if(i==active_.end()||token.empty()||token.size()>512||deadline<=now||deadline-now>300000)return Start::declined;
 auto& a=i->second;
 if(a.phase==Phase::running)return a.lease_token==token&&a.deadline>now?Start::already_running:Start::declined;
 auto cap=capabilities_.find(a.offer.capability);
 if(paused_||a.phase!=Phase::reserved||a.offer.reserve_until<=now||cap==capabilities_.end()||!cap->second.verified||cap->second.preference==Preference::disabled)return Start::declined;
 a.phase=Phase::running;a.lease_token=std::move(token);a.deadline=deadline;return Start::started;
}
bool Admission::renew(const std::string& id,const std::string& token,std::uint64_t seq,Tick deadline,Tick now){
 std::lock_guard lock(mutex_);auto i=active_.find(id);if(i==active_.end())return false;auto& a=i->second;
 if(a.phase!=Phase::running||a.lease_token!=token||a.deadline<=now||seq<=a.renewal_sequence||deadline<=a.deadline||deadline<=now||deadline-now>300000)return false;
 a.deadline=deadline;a.renewal_sequence=seq;return true;
}
void Admission::release(const Resources& r){used_.ram_mb-=r.ram_mb;used_.vram_mb-=r.vram_mb;used_.scratch_mb-=r.scratch_mb;used_.cpu_threads-=r.cpu_threads;}
bool Admission::cancel(const std::string& id){
 std::lock_guard lock(mutex_);auto i=active_.find(id);if(i==active_.end())return false;
 if(i->second.phase==Phase::reserved){release(i->second.offer.resources);active_.erase(i);}else i->second.phase=Phase::cancelling;return true;
}
bool Admission::finish(const std::string& id){
 std::lock_guard lock(mutex_);auto i=active_.find(id);if(i==active_.end())return false;
 release(i->second.offer.resources);active_.erase(i);return true;
}
std::vector<Expired> Admission::expire(Tick now){
 std::lock_guard lock(mutex_);std::vector<Expired> out;
 for(auto i=active_.begin();i!=active_.end();){auto& a=i->second;
  if(a.phase==Phase::reserved&&a.offer.reserve_until<=now){out.push_back({i->first,false});release(a.offer.resources);i=active_.erase(i);}
  else{if(a.phase==Phase::running&&a.deadline<=now){a.phase=Phase::cancelling;out.push_back({i->first,true});}++i;}}
 return out;
}
std::vector<Active> Admission::active() const{std::lock_guard lock(mutex_);std::vector<Active> out;out.reserve(active_.size());for(const auto& [_,a]:active_)out.push_back(a);return out;}
Resources Admission::used() const{std::lock_guard lock(mutex_);return used_;}
}
