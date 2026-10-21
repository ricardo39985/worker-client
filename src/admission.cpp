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
void Admission::defer_new_starts(std::string reason){std::lock_guard lock(mutex_);deferred_reason_=std::move(reason);}
Decision Admission::start_gate() const{
 std::lock_guard lock(mutex_);
 if(paused_)return {false,"paused"};if(probing_)return {false,"gpu_probe"};if(!deferred_reason_.empty())return {false,deferred_reason_};
 if(active_.size()>=max_jobs_)return {false,"busy"};
 if(std::none_of(capabilities_.begin(),capabilities_.end(),[](const auto& entry){return entry.second.verified&&entry.second.preference!=Preference::disabled;}))return {false,"unsupported_or_disabled"};
 return {true,"ready_subject_to_offer_resources"};
}
void Admission::pause(bool value){std::lock_guard lock(mutex_);paused_=value;}
bool Admission::begin_probe(){
 std::lock_guard lock(mutex_);if(probing_||!active_.empty())return false;probing_=true;return true;
}
void Admission::end_probe(){std::lock_guard lock(mutex_);probing_=false;}
Decision Admission::offer(const Offer& o,Tick now,Resources available){
 std::lock_guard lock(mutex_);
 if(!safe_id(o.job_id)||!safe_id(o.attempt_id)||o.fingerprint.empty()||!o.resources.ram_mb||!o.resources.cpu_threads||o.reserve_until<=now||o.reserve_until-now>60000)return {false,"invalid_offer"};
 if(auto i=active_.find(o.attempt_id);i!=active_.end()){
   if(i->second.offer.fingerprint!=o.fingerprint||i->second.offer.job_id!=o.job_id||i->second.offer.capability!=o.capability||i->second.offer.resources!=o.resources)return {false,"attempt_conflict"};
   // Retransmission must not extend the original reservation or revive cancellation.
   return {i->second.phase!=Phase::cancelling && (i->second.phase==Phase::running?i->second.deadline>now:i->second.offer.reserve_until>now),"duplicate"};
 }
 if(paused_)return {false,"paused"};
 if(!deferred_reason_.empty())return {false,deferred_reason_};
 if(probing_)return {false,"gpu_probe"};
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
 if(paused_||probing_||!deferred_reason_.empty()||a.phase!=Phase::reserved||a.offer.reserve_until<=now||cap==capabilities_.end()||!cap->second.verified||cap->second.preference==Preference::disabled)return Start::declined;
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
bool Admission::reserve_ram(const std::string& id,std::uint64_t total,Resources available,Tick now){
 std::lock_guard lock(mutex_);auto i=active_.find(id);
 if(i==active_.end()||i->second.phase!=Phase::running||i->second.deadline<=now||i->second.compute_released)return false;
 auto& a=i->second;const auto current=a.offer.resources.ram_mb+a.extra_ram_mb;
 if(total<=current)return true;
 const Resources delta{total-current,0,0,0};
 if(!fits(delta,budget_,used_)||!fits(delta,available,used_))return false;
 used_.ram_mb+=delta.ram_mb;a.extra_ram_mb+=delta.ram_mb;return true;
}
bool Admission::release_compute(const std::string& id){
 std::lock_guard lock(mutex_);auto i=active_.find(id);
 if(i==active_.end()||i->second.phase!=Phase::running||i->second.compute_released)return false;
 auto& a=i->second;used_.ram_mb-=a.offer.resources.ram_mb+a.extra_ram_mb;used_.cpu_threads-=a.offer.resources.cpu_threads;
 a.extra_ram_mb=0;a.compute_released=true;return true;
}
Decision Admission::stage(const std::string& id,const std::string& name,Resources cost,Tick now,Resources available,unsigned concurrent_limit){
 std::lock_guard lock(mutex_);auto i=active_.find(id);
 if(i==active_.end()||i->second.phase!=Phase::running||i->second.deadline<=now||!i->second.compute_released||
    !safe_id(name)||!cost.ram_mb||!cost.cpu_threads||cost.scratch_mb||cost.vram_mb)return {false,"stage_unavailable"};
 auto& stages=i->second.stages;
 if(auto existing=stages.find(name);existing!=stages.end())return {existing->second==cost,"duplicate_stage"};
 std::string reason;std::size_t concurrent=0;for(const auto& [_,active]:active_)concurrent+=active.stages.size();
 if(concurrent>=concurrent_limit)reason="measured_stage_limit";
 else if(used_.cpu_threads>budget_.cpu_threads||cost.cpu_threads>budget_.cpu_threads-used_.cpu_threads)reason="cpu_budget";
 else if(used_.ram_mb>budget_.ram_mb||cost.ram_mb>budget_.ram_mb-used_.ram_mb)reason="ram_budget";
 // Physical free RAM already excludes resident running children. The atomic
 // sum of their full child caps is independently bounded by budget_ above.
 // This live check is sampled headroom, not an OS-wide physical RAM quota.
 else if(!fits(cost,available))reason="live_headroom";
 if(!reason.empty()){waiting_[{id,name}]=reason;return {false,reason};}
 waiting_.erase({id,name});stages.emplace(name,cost);used_.ram_mb+=cost.ram_mb;used_.cpu_threads+=cost.cpu_threads;
 return {true,"stage_ready"};
}
void Admission::waiting(const std::string& id,const std::string& name,const std::string& reason){
 std::lock_guard lock(mutex_);if(active_.contains(id)&&safe_id(name))waiting_[{id,name}]=reason.substr(0,128);
}
std::string Admission::queue_reason(const std::string& id,const std::string& stage) const{
 std::lock_guard lock(mutex_);auto i=waiting_.find({id,stage});return i==waiting_.end()?"encoder_capacity":i->second;
}
unsigned Admission::stage_count() const{
 std::lock_guard lock(mutex_);std::size_t n=0;for(const auto& [_,a]:active_)n+=a.stages.size();return static_cast<unsigned>(n);
}
bool Admission::finish_stage(const std::string& id,const std::string& name){
 std::lock_guard lock(mutex_);auto i=active_.find(id);if(i==active_.end())return false;
 auto s=i->second.stages.find(name);if(s==i->second.stages.end())return false;
 release(s->second);i->second.stages.erase(s);waiting_.erase({id,name});return true;
}
bool Admission::release_scratch(const std::string& id,std::uint64_t mb){
 std::lock_guard lock(mutex_);auto i=active_.find(id);
 if(i==active_.end()||i->second.phase==Phase::reserved||mb>i->second.offer.resources.scratch_mb-i->second.released_scratch_mb)return false;
 i->second.released_scratch_mb+=mb;used_.scratch_mb-=mb;return true;
}
std::vector<std::string> Admission::capacity_lines() const{
 std::lock_guard lock(mutex_);
 std::vector<std::string> out{
  "capacity CPU "+std::to_string(used_.cpu_threads)+"/"+std::to_string(budget_.cpu_threads)+" reserved threads",
  "capacity RAM "+std::to_string(used_.ram_mb)+"/"+std::to_string(budget_.ram_mb)+" MiB reserved",
  "capacity scratch "+std::to_string(used_.scratch_mb)+"/"+std::to_string(budget_.scratch_mb)+" MiB reserved",
  "local stage queue "+std::to_string(waiting_.size())+"; remote queue unknown"};
 for(const auto& [key,reason]:waiting_)out.push_back("queued "+key.first.substr(key.first.size()>4?key.first.size()-4:0)+" "+key.second+": "+reason);
 return out;
}
bool Admission::finish(const std::string& id){
 std::lock_guard lock(mutex_);auto i=active_.find(id);if(i==active_.end()||!i->second.stages.empty())return false;
 auto resources=i->second.offer.resources;resources.scratch_mb-=i->second.released_scratch_mb;if(i->second.compute_released){resources.ram_mb=0;resources.cpu_threads=0;}
 used_.ram_mb-=i->second.extra_ram_mb;release(resources);active_.erase(i);
 std::erase_if(waiting_,[&](const auto& entry){return entry.first.first==id;});return true;
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
