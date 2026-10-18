#pragma once
#include "ow/console.hpp"
#include <deque>

namespace ow {
struct StatusCard {std::string id,name;};
struct StatusReadings {
 std::optional<std::uint64_t> queue,ram_bytes,scratch_bytes;
 std::optional<std::string> encoders;
 std::optional<std::vector<StatusCard>> cards;
};
enum class JobOutcome {succeeded,failed,cancelled};
inline std::string outcome_word(JobOutcome o){return o==JobOutcome::succeeded?"succeeded":o==JobOutcome::failed?"failed":"cancelled";}
inline ConsoleColor outcome_color(JobOutcome o){return o==JobOutcome::succeeded?ConsoleColor::green:o==JobOutcome::failed?ConsoleColor::red:ConsoleColor::orange;}
namespace status_detail {
inline std::string safe(std::string value){for(auto& c:value)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>=127)c='?';return value;}
inline std::string truncate(std::string value,std::size_t width){value=safe(std::move(value));if(value.size()>width)value=value.substr(0,width-1)+"~";return value;}
inline std::string cell(std::string value,std::size_t width){value=truncate(std::move(value),width);value.resize(width,' ');return value;}
inline std::string hms(std::int64_t ms){auto seconds=std::max<std::int64_t>(0,ms)/1000;std::ostringstream s;s<<std::setfill('0')<<std::setw(2)<<seconds/3600<<':'<<std::setw(2)<<seconds/60%60<<':'<<std::setw(2)<<seconds%60;return s.str();}
inline std::string time(std::int64_t wall){auto stamp=format_local_timestamp(wall);return stamp.substr(6);}
inline std::string count(std::optional<std::uint64_t> n){return n?std::to_string(*n):"unknown";}
inline std::string gb(std::optional<std::uint64_t> n,bool decimal){if(!n)return "unknown";std::ostringstream s;s<<std::fixed<<std::setprecision(decimal?1:0)<<static_cast<double>(*n)/1000000000.0<<" GB";return s.str();}
inline std::string card_name(std::string name){for(std::string_view prefix:{"NVIDIA GeForce ","AMD Radeon ","Intel(R) "})if(name.starts_with(prefix)){name.erase(0,prefix.size());break;}return name;}
inline std::vector<StatusCard> cards(const StatusReadings& r){std::vector<StatusCard> out;std::set<std::string> seen;if(r.cards)for(const auto& c:*r.cards)if(!c.id.empty()&&c.id!="cpu"&&seen.insert(c.id).second)out.push_back({c.id,card_name(c.name)});std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.id<b.id;});return out;}
inline ConsoleLine decorate(std::string text){
 ConsoleLine line{std::move(text),ConsoleColor::white};
 if(auto job=line.text.find(" job ");job!=std::string::npos){
  auto body=console_detail::after_first_token(std::string_view(line.text).substr(job+5));auto outcome=console_detail::first_token(body);
  if(outcome=="failed")line.color=ConsoleColor::red;else if(outcome=="cancelled")line.color=ConsoleColor::orange;else if(outcome=="succeeded")line.color=ConsoleColor::green;
 }
 if(line.text.starts_with("  state      ")){
  const auto start=std::size_t{13},end=line.text.find(' ',start);auto name=line.text.substr(start,end-start);
  if(name=="accepting"||name=="paused"||name=="stopping")line.spans.push_back({start,name.size(),name=="accepting"?ConsoleColor::green:ConsoleColor::orange});
 }
 if(line.text.starts_with("  totals     ")){
  auto failed=line.text.find(" failed");if(failed!=std::string::npos){auto start=line.text.find_last_of(' ',failed-1);start=start==std::string::npos?0:start+1;auto n=line.text.substr(start,failed-start);if(n!="0"&&n!="unknown")line.spans.push_back({start,failed-start+7,ConsoleColor::red});}
 }
 return line;
}
}
inline std::vector<ConsoleLine> parse_status_frame(std::string_view text){std::vector<ConsoleLine> out;std::size_t at=0;while(at<text.size()){auto end=text.find('\n',at);if(end==std::string_view::npos)return {};out.push_back(status_detail::decorate(std::string(text.substr(at,end-at))));at=end+1;}return out;}
inline std::string status_frame(const std::vector<ConsoleLine>& lines){std::string out;for(const auto& l:lines)out+=l.text+'\n';return out;}

// Reporting only: no admission, scheduling, persistence or wire-result decisions.
// Caller serializes mutation and frame publication. All elapsed values use the
// supplied monotonic clock; wall clock is used solely for local timestamps.
class WorkerStatus {
 struct Active {std::string subject,device;std::int64_t wall,mono;};
 struct Last {std::string id,subject,reason;JobOutcome outcome;std::int64_t wall,duration;};
 std::int64_t started_wall_,started_,since_wall_,idle_since_,next_live_{},next_durable_{};
 std::string state_{"paused"},reason_{"starting"},priority_{"unknown"};
 std::uint64_t ok_{},failed_{},cancelled_{};
 std::map<std::string,Active> active_;
 std::deque<Last> history_;
 bool dirty_{true};
 static std::string totals(std::uint64_t ok,std::uint64_t failed,std::uint64_t cancelled,std::string sep){return std::to_string(ok)+" ok"+sep+std::to_string(failed)+" failed"+sep+std::to_string(cancelled)+" cancelled";}
public:
 struct Emission {bool live{},durable{},summary{};};
 WorkerStatus(std::int64_t wall,std::int64_t mono):started_wall_(wall),started_(mono),since_wall_(wall),idle_since_(mono){}
 std::string startup() const{return format_local_timestamp(started_wall_)+" worker started; counts since this process";}
 void state(std::string name,std::string reason,std::int64_t wall,std::int64_t /*mono*/){if(name==state_&&reason==reason_)return;state_=std::move(name);reason_=std::move(reason);since_wall_=wall;dirty_=true;}
 void priority(std::string p){if(p!=priority_){priority_=p.empty()?"unknown":std::move(p);dirty_=true;}}
 bool accepting() const{return state_=="accepting";}
 std::string state_line() const{return state_+(reason_.empty()?"":": "+reason_)+" (since "+status_detail::time(since_wall_)+")";}
 void start(const std::string& id,std::string subject,std::int64_t wall,std::int64_t mono){active_.try_emplace(id,Active{std::move(subject),{},wall,mono});dirty_=true;}
 void placement(const std::string& id,std::string device){if(auto it=active_.find(id);it!=active_.end())it->second.device=std::move(device);}
 bool finish(const std::string& id,JobOutcome outcome,std::string reason,std::int64_t wall,std::int64_t mono){
  auto it=active_.find(id);if(it==active_.end())return false;
  if(outcome==JobOutcome::succeeded)++ok_;else if(outcome==JobOutcome::failed)++failed_;else ++cancelled_;
  history_.push_back({id,it->second.subject,std::move(reason),outcome,wall,std::max<std::int64_t>(0,mono-it->second.mono)});if(history_.size()>10)history_.pop_front();
  active_.erase(it);if(active_.empty()){idle_since_=mono;next_durable_=mono+300000;}dirty_=true;return true;
 }
 std::string completion() const{if(history_.empty())return {};const auto& h=history_.back();return "JOB "+h.id+" "+outcome_word(h.outcome)+" | done in "+format_duration_compact(h.duration)+" ("+h.subject+")"+(h.reason.empty()?"":" | "+status_detail::safe(h.reason));}
 std::vector<ConsoleLine> history() const{std::vector<ConsoleLine> lines;for(const auto& h:history_)lines.push_back({format_local_timestamp(h.wall)+" job "+h.id.substr(h.id.size()>4?h.id.size()-4:0)+" "+outcome_word(h.outcome)+" in "+format_duration_compact(h.duration)+" ("+h.subject+")"+(h.reason.empty()?"":" | "+status_detail::safe(h.reason)),outcome_color(h.outcome)});return lines;}
 std::string summary(std::int64_t wall,std::int64_t mono,const StatusReadings& r) const{
  std::string last="unknown";if(!history_.empty()){const auto& h=history_.back();auto subject=h.subject.substr(0,h.subject.find(' '));last=status_detail::time(h.wall).substr(0,5)+" ("+format_duration_compact(h.duration)+", "+subject+")";}
  return format_local_timestamp(wall)+" stats: "+totals(ok_,failed_,cancelled_," ")+" | last job "+last+" | queue "+status_detail::count(r.queue)+" | idle "+(active_.empty()?status_detail::hms(mono-idle_since_):"00:00:00")+" | "+machine(r)+" | priority "+priority_+" | "+state_line();
 }
 std::string machine(const StatusReadings& r) const{
  std::string text="running "+std::to_string(active_.size());auto cards=status_detail::cards(r);
  for(const auto& [id,a]:active_){std::string name=a.device.empty()?"unknown":a.device=="cpu"?"cpu":"unknown";for(const auto& c:cards)if(c.id==a.device)name=c.name;text+=" | job "+id+"="+status_detail::safe(name);}
  return text+" | ram free "+status_detail::gb(r.ram_bytes,true)+" | scratch free "+status_detail::gb(r.scratch_bytes,false);
 }
 std::vector<ConsoleLine> table(std::int64_t wall,std::int64_t mono,const StatusReadings& r) const{
  if(!active_.empty())return {};
  using namespace status_detail;std::vector<ConsoleLine> out;
  auto add=[&](std::string label,std::string value){out.push_back(decorate("  "+cell(std::move(label),11)+std::move(value)));};
  out.push_back({format_local_timestamp(wall)+"  status",ConsoleColor::white});
  add("state",cell(state_,25)+"since "+time(since_wall_)+(reason_.empty()||state_=="accepting"?"":" ("+truncate(reason_,24)+")"));
  add("priority",truncate(priority_,67));add("jobs","running 0   queue "+count(r.queue));
  auto pad_count=[](std::string value,std::size_t width){if(value.size()<width)value.append(width-value.size(),' ');return value;};
  add("totals",pad_count(std::to_string(ok_)+" ok",8)+pad_count(std::to_string(failed_)+" failed",11)+std::to_string(cancelled_)+" cancelled");
  if(history_.empty())add("last job","unknown");else{const auto& h=history_.back();add("last job",time(h.wall)+"  "+cell(h.subject,26)+format_duration_compact(h.duration));}
  auto cards_now=cards(r);cards_now.push_back({"cpu","cpu"});std::vector<std::string> entries;if(!r.cards)entries.push_back("unknown");for(const auto& c:cards_now)entries.push_back(truncate(c.name,12)+" idle");
  for(std::size_t i=0;i<entries.size();i+=3){
   std::string row;const auto end=std::min(i+3,entries.size());
   for(std::size_t j=i;j<end;++j){row+=entries[j];if(j+1<end)row.append(22-entries[j].size(),' ');}
   add(i==0?"cards":"",row);
  }
  add("memory","ram free "+gb(r.ram_bytes,true)+"   scratch free "+gb(r.scratch_bytes,false));add("encoders",r.encoders?truncate(*r.encoders,67):"unknown");add("uptime",hms(mono-started_));return out;
 }
 Emission poll(std::int64_t mono){
  Emission e;e.summary=dirty_;if(!active_.empty()){dirty_=false;return e;}
  e.live=dirty_||mono>=next_live_;e.durable=mono>=next_durable_;e.summary=e.summary||e.live;
  if(e.live)next_live_=mono+30000;
  if(e.durable)next_durable_=mono+300000;
  dirty_=false;return e;
 }
};
}
