#pragma once
#include <algorithm>
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
inline std::string connectionActivityLine(std::uint64_t active,std::uint64_t pending) {
 if(active==0 && pending==0)return {};
 return "ACTIVITY | active jobs "+std::to_string(active)+" | durable results awaiting ACK "+std::to_string(pending);
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

inline std::string short_job_id(std::string_view id) {
 return std::string(id.substr(0,std::min<std::size_t>(8,id.size())));
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

class ProgressLogPolicy {
 std::set<std::pair<std::string,std::uint64_t>> unknown_;
 std::map<std::pair<std::string,std::uint64_t>,std::pair<unsigned,bool>> percent_;
public:
 void forget(const std::string& job) {
  auto prefix=job+"|";
  std::erase_if(unknown_,[&](const auto& key){return key.first.starts_with(prefix);});
  std::erase_if(percent_,[&](const auto& entry){return entry.first.first.starts_with(prefix);});
 }
 std::optional<std::string> line(const std::string& job,const std::string& subject,const ProgressSnapshot& value) {
  auto key=std::make_pair(job+"|"+value.stage,value.run);
  if(!value.percent){if(!unknown_.insert(key).second)return {};
   return "JOB "+short_job_id(job)+" | WAITING | "+subject+" | "+value.stage+" progress cannot be measured";
  }
  auto found=percent_.find(key);
  if(found!=percent_.end()&&found->second.first==*value.percent&&(!value.quiet||found->second.second))return {};
  percent_[key]={*value.percent,value.quiet};
  std::string out="JOB "+short_job_id(job)+" | PROGRESS | "+subject+" | "+value.stage+" | run "+std::to_string(value.run)+" | "+std::to_string(*value.percent)+"% | elapsed "+format_duration(value.elapsed_ms);
  if(value.remaining_ms)out+=" | remaining "+format_duration(*value.remaining_ms);
  if(value.quiet)out+=" | no measured movement for "+format_duration(value.quiet_ms);
  return out;
 }
};

struct ConsolePresentation {bool interactive{},color{},unicode{};};
struct LiveConsoleRow {std::string key,text;unsigned percent{};bool activity{};};
class LiveConsoleView {
 std::vector<LiveConsoleRow> rows_;
 static std::string job_key(std::string_view line){auto begin=line.find("JOB ");if(begin==std::string_view::npos)return {};begin+=4;auto end=line.find(" |",begin);return end==std::string_view::npos?std::string{}:std::string(line.substr(begin,end-begin));}
 void erase(const std::string& key){rows_.erase(std::remove_if(rows_.begin(),rows_.end(),[&](const auto& row){return row.key==key;}),rows_.end());}
public:
 bool consume(const std::string& line) {
  auto key=job_key(line);const bool progress=line.find(" | PROGRESS | ")!=std::string::npos;
  const bool terminal=line.find(" | CANCELLED | ")!=std::string::npos||line.find(" | FAILED | ")!=std::string::npos||line.find(" | SUCCEEDED | ")!=std::string::npos||line.find(" | DECLINED | ")!=std::string::npos;
  if(terminal&&!key.empty()){erase(key);return false;}
  if(!key.empty()&&!progress&&(line.find(" | STAGE | ")!=std::string::npos||line.find(" | WAITING | ")!=std::string::npos)){erase(key);return false;}
  if(progress&&!key.empty()){
   auto marker=line.find("% | elapsed ");auto start=marker==std::string::npos?std::string::npos:line.rfind(" | ",marker);
   unsigned percent=0;if(start!=std::string::npos){auto number=std::string_view(line).substr(start+3,marker-(start+3));std::from_chars(number.data(),number.data()+number.size(),percent);}
   auto found=std::find_if(rows_.begin(),rows_.end(),[&](const auto& row){return row.key==key;});LiveConsoleRow row{key,line,percent,false};
   if(found==rows_.end())rows_.push_back(std::move(row));else *found=std::move(row);return true;
  }
  if(line.find("ACTIVITY | idle")!=std::string::npos){erase("$activity");return true;}
  if(line.find("ACTIVITY | ")!=std::string::npos){auto found=std::find_if(rows_.begin(),rows_.end(),[](const auto& row){return row.activity;});LiveConsoleRow row{"$activity",line,0,true};if(found==rows_.end())rows_.push_back(std::move(row));else *found=std::move(row);return true;}
  return false;
 }
 const std::vector<LiveConsoleRow>& rows() const{return rows_;}
 std::string render(ConsolePresentation mode) const {
  std::string out;for(const auto& row:rows_){std::string line=row.text;
   if(!row.activity){auto message=line.find("JOB ");if(message!=std::string::npos)line=line.substr(message);std::string bar="[";auto filled=std::min(40u,row.percent*40/100);bar.append(filled,'#');bar.append(40-filled,'-');bar+="] ";auto percent=line.find("% | elapsed ");auto before=percent==std::string::npos?std::string::npos:line.rfind(" | ",percent);if(before!=std::string::npos)line.insert(before+3,bar);}
   if(mode.interactive&&mode.color)out+="\x1b[36m"+line+"\x1b[0m\n";else out+=line+'\n';
  }return out;
 }
};
}
