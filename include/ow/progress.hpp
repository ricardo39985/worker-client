#pragma once
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ow {
inline constexpr std::size_t console_width = 80;

inline std::string connectionActivityLine(std::uint64_t active,std::uint64_t pending) {
 if(active==0 && pending==0)return {};
 return std::to_string(active)+" active · "+std::to_string(pending)+" unacked";
}

// Durable activity is still written when the last job drains so the viewer can
// remove its stale summary row. The console formatter hides the zero/zero record.
inline std::string connectionActivityRecord(std::uint64_t active,std::uint64_t pending) {
 return "ACTIVITY "+std::to_string(active)+" "+std::to_string(pending);
}

inline std::string format_duration(std::int64_t milliseconds) {
 milliseconds=std::max<std::int64_t>(0,milliseconds);
 if(milliseconds>0&&milliseconds<1000)return std::to_string(milliseconds)+"ms";
 auto seconds=milliseconds/1000;
 if(seconds<60)return std::to_string(seconds)+"s";
 auto minutes=seconds/60;
 if(minutes<60)return std::to_string(minutes)+"m "+(seconds%60<10?"0":"")+std::to_string(seconds%60)+"s";
 auto hours=minutes/60;
 return std::to_string(hours)+"h "+(minutes%60<10?"0":"")+std::to_string(minutes%60)+"m";
}

// Console durations are deliberately tighter than the human setup/report form.
inline std::string format_duration_compact(std::int64_t milliseconds) {
 milliseconds=std::max<std::int64_t>(0,milliseconds);
 if(milliseconds>0&&milliseconds<1000)return std::to_string(milliseconds)+"ms";
 auto seconds=milliseconds/1000;
 if(seconds<60)return std::to_string(seconds)+"s";
 auto minutes=seconds/60;
 if(minutes<60)return std::to_string(minutes)+"m"+(seconds%60<10?"0":"")+std::to_string(seconds%60)+"s";
 auto hours=minutes/60;
 return std::to_string(hours)+"h"+(minutes%60<10?"0":"")+std::to_string(minutes%60)+"m";
}

inline std::string compact_duration_text(std::string text) {
 std::erase(text,' ');
 return text;
}

// The durable file keeps an unambiguous UTC, millisecond, zone-marked value.
inline std::string format_utc_timestamp(std::int64_t milliseconds) {
 const auto seconds=static_cast<std::time_t>(milliseconds/1000);std::tm value{};
#ifdef _WIN32
 gmtime_s(&value,&seconds);
#else
 gmtime_r(&seconds,&value);
#endif
 std::ostringstream out;out<<std::put_time(&value,"%Y-%m-%d %H:%M:%S")<<'.'
  <<std::setw(3)<<std::setfill('0')<<(milliseconds%1000+1000)%1000<<'Z';return out.str();
}

// The console uses the operator's local clock. No year, milliseconds or zone
// letter is added: the durable log remains the timezone-complete evidence.
inline std::string format_local_timestamp(std::int64_t milliseconds) {
 auto seconds=milliseconds/1000;
 if(milliseconds<0&&milliseconds%1000) --seconds;
 const auto value_seconds=static_cast<std::time_t>(seconds);std::tm value{};
#ifdef _WIN32
 localtime_s(&value,&value_seconds);
#else
 localtime_r(&value_seconds,&value);
#endif
 std::ostringstream out;out<<std::put_time(&value,"%d/%m %H:%M:%S");return out.str();
}

inline std::string normalized_attempt_id(std::string_view id) {
 std::string out;
 for(char c:id)if(c>='0'&&c<='9')out+=c;else if(c>='a'&&c<='z')out+=c;else if(c>='A'&&c<='Z')out+=static_cast<char>(c-'A'+'a');
 return out;
}

inline std::string job_suffix(std::string_view id,std::size_t length=4) {
 auto normalized=normalized_attempt_id(id);
 if(normalized.empty())return "????";
 if(length==0)length=4;
 if(normalized.size()<=length)return normalized;
 return normalized.substr(normalized.size()-length);
}

// Live-label allocation is deterministic from the attempt id. A newly started
// job keeps the documented four-character suffix. If that collides with a live
// job it grows by one character at a time; earlier live labels never shrink or
// change, so every line in that job's life keeps the same label. Finished labels
// leave the live set and may be reused.
class JobLabels {
 std::map<std::string,std::string> labels_;
 std::set<std::string> live_;
public:
 std::string start(const std::string& id) {
  if(auto found=labels_.find(id);found!=labels_.end()){live_.insert(id);return found->second;}
  const auto normalized=normalized_attempt_id(id);
  std::string chosen=job_suffix(id);
  for(std::size_t length=4;length<=normalized.size();++length){
   auto candidate=normalized.size()<=length?normalized:normalized.substr(normalized.size()-length);
   bool used=false;
   for(const auto& other:live_)if(other!=id&&labels_.at(other)==candidate){used=true;break;}
   if(!used){chosen=std::move(candidate);break;}
  }
  labels_[id]=chosen;live_.insert(id);return chosen;
 }
 std::string label(const std::string& id) const {
  if(auto found=labels_.find(id);found!=labels_.end())return found->second;
  return job_suffix(id);
 }
 void finish(const std::string& id){live_.erase(id);}
 bool live(const std::string& id) const{return live_.contains(id);}
};

// Resolve a displayed lowercase suffix by finding the complete attempt ids in
// the plain durable log. A unique match proves the viewer did not invent a
// second identifier namespace. Ambiguous historical collisions stay ambiguous.
inline std::vector<std::string> attempt_ids_in_log(std::string_view text) {
 std::vector<std::string> ids;
 auto hex=[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');};
 for(std::size_t i=0;i+36<=text.size();++i){
  auto token=text.substr(i,36);
  bool shape=token[8]=='-'&&token[13]=='-'&&token[18]=='-'&&token[23]=='-';
  if(!shape)continue;
  for(std::size_t j=0;j<token.size()&&shape;++j){
   if(j==8||j==13||j==18||j==23)continue;
   shape=hex(token[j]);
  }
  if(!shape)continue;
  if(i&&(hex(text[i-1])||text[i-1]=='-'))continue;
  if(i+36<text.size()&&(hex(text[i+36])||text[i+36]=='-'))continue;
  ids.emplace_back(token);i+=35;
 }
 return ids;
}

inline std::optional<std::string> resolve_job_suffix(std::string_view durable_log,std::string_view suffix) {
 const auto wanted=normalized_attempt_id(suffix);if(wanted.empty())return {};
 std::set<std::string> matches;
 for(const auto& id:attempt_ids_in_log(durable_log)){
  const auto normalized=normalized_attempt_id(id);
  if(normalized.size()>=wanted.size()&&normalized.ends_with(wanted))matches.insert(id);
 }
 if(matches.size()!=1)return {};
 return *matches.begin();
}

inline std::string capability_summary(bool app_video,bool app_image,std::string_view encoders={}) {
 std::string out="ready: ";
 if(app_video&&app_image)out+="video+image";
 else if(app_video)out+="video";
 else if(app_image)out+="image";
 else out+="none";
 // Name the encoders the worker actually reported. An absent field is not proof of CPU:
 // older records omit it, so claim nothing rather than reporting a backend we cannot see.
 if(!encoders.empty())out+=" ("+std::string(encoders)+")";
 return out;
}

// Extracts the encoder list a worker reported on its probe line, e.g. "encoders=cpu,qsv,amf".
// Only known backend names survive, and an absent or unrecognised field yields nothing, so the
// console never invents a backend it cannot verify.
inline std::string probe_encoders(std::string_view body) {
 static constexpr std::string_view known[]={"cpu","qsv","amf","nvenc","vaapi","videotoolbox"};
 constexpr std::string_view key="encoders=";
 auto at=body.find(key);
 if(at==std::string_view::npos)return {};
 at+=key.size();
 auto end=body.find_first_of(" \t\r\n",at);
 const auto value=body.substr(at,end==std::string_view::npos?std::string_view::npos:end-at);
 std::string out;
 std::size_t start=0;
 while(start<=value.size()){
  const auto comma=value.find(',',start);
  const auto token=value.substr(start,comma==std::string_view::npos?std::string_view::npos:comma-start);
  for(std::string_view k:known)if(token==k&&out.find(k)==std::string_view::npos){
   if(!out.empty())out+=", ";
   out+=k;
  }
  if(comma==std::string_view::npos)break;
  start=comma+1;
 }
 return out;
}

inline std::vector<std::wstring> encoder_progress_arguments(std::vector<std::wstring> arguments) {
 for(std::size_t i=0;i+1<arguments.size();++i)if(arguments[i]==L"-loglevel")arguments[i+1]=L"info";
 auto at=std::find(arguments.begin(),arguments.end(),L"-hide_banner");
 arguments.insert(at==arguments.end()?arguments.begin():at+1,{L"-progress",L"pipe:1",L"-stats_period",L"0.5",L"-nostats"});
 return arguments;
}

// Parses FFmpeg's own timeline report. Byte counters are deliberately absent:
// encoded file size has no stable relationship to position in variable-rate media.
class EncoderProgress {
 std::string pending_;std::uint64_t duration_us_{},position_us_{};
 static std::optional<std::uint64_t> unsigned_value(std::string_view text) {
  std::uint64_t value{};auto result=std::from_chars(text.data(),text.data()+text.size(),value);
  if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return {};
  return value;
 }
 static std::optional<std::uint64_t> duration_value(std::string_view line) {
  auto at=line.find("Duration: ");if(at==std::string_view::npos)return {};at+=10;
  if(line.substr(at,3)=="N/A"||at+8>line.size())return {};
  unsigned hours{},minutes{};long double seconds{};char first{},second{};
  std::istringstream in{std::string(line.substr(at))};
  if(!(in>>hours>>first>>minutes>>second>>seconds)||first!=':'||second!=':'||minutes>=60||seconds<0||seconds>=60)return {};
  return static_cast<std::uint64_t>((static_cast<long double>(hours*3600+minutes*60)+seconds)*1000000.0L);
 }
 void line(std::string_view value) {
  if(auto duration=duration_value(value);duration&&*duration>duration_us_)duration_us_=*duration;
  constexpr std::string_view key="out_time_us=";auto at=value.find(key);
  if(at!=std::string_view::npos){auto end=value.find_first_of("\r ",at+key.size());auto number=value.substr(at+key.size(),end-(at+key.size()));
   if(auto position=unsigned_value(number);position)position_us_=std::max(position_us_,*position);
  }
 }
public:
 void consume(std::string_view bytes) {
  pending_.append(bytes);std::size_t start=0;
  for(;;){auto end=pending_.find('\n',start);if(end==std::string::npos)break;line(std::string_view(pending_).substr(start,end-start));start=end+1;}
  if(start)pending_.erase(0,start);
  if(pending_.size()>8192)pending_.erase(0,pending_.size()-8192);
 }
 std::uint64_t duration_us() const{return duration_us_;}
 std::uint64_t position_us() const{return position_us_;}
};

struct ProgressSnapshot {
 std::string stage;std::uint64_t completed{},total{},run{};std::optional<unsigned> percent;
 std::optional<std::int64_t> remaining_ms;
 std::int64_t elapsed_ms{},quiet_ms{};bool quiet{},finished{},succeeded{};
};

class ProgressCounter {
 mutable std::mutex mutex_;std::string stage_;std::uint64_t completed_{},total_{},run_{};
 std::int64_t started_{},activity_{},finished_at_{};bool finished_{},succeeded_{};
public:
 void begin(std::string stage,std::int64_t now,std::uint64_t total=0){
  std::lock_guard lock(mutex_);stage_=std::move(stage);started_=activity_=now;completed_=0;total_=total;++run_;finished_=succeeded_=false;
 }
 void set_total(std::uint64_t total,std::int64_t now){
  std::lock_guard lock(mutex_);if(!finished_&&total&&!total_){total_=total;activity_=std::max(activity_,now);}
 }
 void update(std::uint64_t completed,std::int64_t now){
  std::lock_guard lock(mutex_);if(!finished_&&completed>completed_){completed_=completed;activity_=std::max(activity_,now);}
 }
 void finish(bool succeeded,std::int64_t now){std::lock_guard lock(mutex_);if(finished_)return;finished_=true;succeeded_=succeeded;finished_at_=now;}
 ProgressSnapshot snapshot(std::int64_t now) const {
  std::lock_guard lock(mutex_);if(finished_)now=finished_at_;
  ProgressSnapshot s;s.stage=stage_;s.completed=completed_;s.total=total_;s.run=run_;s.finished=finished_;s.succeeded=succeeded_;
  s.elapsed_ms=std::max<std::int64_t>(0,now-started_);s.quiet_ms=std::max<std::int64_t>(0,now-activity_);s.quiet=!finished_&&s.quiet_ms>=30000;
  if(total_){s.percent=static_cast<unsigned>(100.0L*std::min(completed_,total_)/total_);
   if(completed_&&completed_<total_&&s.elapsed_ms>0){auto estimate=static_cast<long double>(s.elapsed_ms)*(total_-completed_)/completed_;s.remaining_ms=static_cast<std::int64_t>(estimate);}
  }
  return s;
 }
};

// Durable records keep the full attempt id for grep/evidence. The console
// renderer is the only layer that shortens them and inserts a progress bar.
class ProgressLogPolicy {
 std::set<std::pair<std::string,std::uint64_t>> unknown_;
 std::map<std::pair<std::string,std::uint64_t>,std::pair<unsigned,bool>> percent_;
 std::map<std::string,std::uint64_t> last_stage_run_;
public:
 void forget(const std::string& job) {
  auto prefix=job+"|";
  std::erase_if(unknown_,[&](const auto& key){return key.first.starts_with(prefix);});
  std::erase_if(percent_,[&](const auto& entry){return entry.first.first.starts_with(prefix);});
  std::erase_if(last_stage_run_,[&](const auto& entry){return entry.first.starts_with(prefix);});
 }
 std::optional<std::string> line(const std::string& job,const std::string& subject,const ProgressSnapshot& value) {
  (void)subject;
  const auto stage_key=job+"|"+value.stage;
  auto previous_run=last_stage_run_.find(stage_key);
  const bool retry=previous_run!=last_stage_run_.end()&&value.run>previous_run->second;
  if(previous_run==last_stage_run_.end()||value.run>previous_run->second)last_stage_run_[stage_key]=value.run;
  if(retry)return "JOB "+job+" RETRY | "+value.stage+" | run "+std::to_string(value.run);
  const auto key=std::make_pair(job+"|"+value.stage,value.run);
  if(!value.percent){if(!unknown_.insert(key).second)return {};
   return "JOB "+job+" WAITING | "+value.stage+" | elapsed "+format_duration_compact(value.elapsed_ms);
  }
  auto found=percent_.find(key);
  if(found!=percent_.end()&&found->second.first==*value.percent&&(!value.quiet||found->second.second))return {};
  percent_[key]={*value.percent,value.quiet};
  std::string out="JOB "+job+" PROGRESS | "+value.stage+" | run "+std::to_string(value.run)+" | "+std::to_string(*value.percent)+"% | elapsed "+format_duration_compact(value.elapsed_ms);
  if(value.remaining_ms)out+=" | remaining "+format_duration_compact(*value.remaining_ms);
  return out;
 }
};
}
