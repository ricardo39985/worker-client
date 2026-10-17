#pragma once
#include "ow/progress.hpp"
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ow {
enum class ConsoleColor { red, orange, green, blue, white };

// Exactly five semantic colours. White is the terminal's default foreground so
// neutral text stays legible on both light and dark Windows consoles. Dark
// yellow (the 16-colour palette's brown/orange slot) is the closest legible
// analogue of orange: unlike bright yellow it also has usable contrast on a
// light console background.
inline const char* ansi_color(ConsoleColor color) {
 switch(color){
  case ConsoleColor::red:return "\x1b[91m";
  case ConsoleColor::orange:return "\x1b[33m";
  case ConsoleColor::green:return "\x1b[92m";
  case ConsoleColor::blue:return "\x1b[94m";
  case ConsoleColor::white:return "\x1b[39m";
 }
 return "\x1b[39m";
}

struct ConsoleLine {std::string text;ConsoleColor color{ConsoleColor::white};};
struct ConsolePresentation {bool interactive{},color{},unicode{};};

inline std::size_t console_text_width(std::string_view text) {
 std::size_t width=0;
 for(unsigned char c:text)if((c&0xC0)!=0x80)++width;
 return width;
}

inline std::string fit_console_line(std::string_view text,std::size_t width=console_width) {
 if(console_text_width(text)<=width)return std::string(text);
 if(width==0)return {};
 const auto limit=width-1;std::size_t bytes=0,columns=0;
 while(bytes<text.size()&&columns<limit){
  auto c=static_cast<unsigned char>(text[bytes]);
  std::size_t length=c<0x80?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:4;
  if(bytes+length>text.size())break;
  bytes+=length;++columns;
 }
 auto prefix=std::string(text.substr(0,bytes));
 if(auto space=prefix.find_last_of(' ');space!=std::string::npos&&space>=prefix.size()/2){
  prefix.resize(space);
  while(!prefix.empty()&&prefix.back()==' ')prefix.pop_back();
 }
 return prefix+"…";
}

namespace console_detail {
inline std::string trim(std::string_view value) {
 auto begin=value.find_first_not_of(" \t\r\n");if(begin==std::string_view::npos)return {};
 auto end=value.find_last_not_of(" \t\r\n");return std::string(value.substr(begin,end-begin+1));
}
inline bool starts(std::string_view value,std::string_view prefix){return value.starts_with(prefix);}
inline std::string first_token(std::string_view value) {
 auto end=value.find_first_of(" \t\r\n");return std::string(value.substr(0,end));
}
inline std::string after_first_token(std::string_view value) {
 auto end=value.find_first_of(" \t\r\n");if(end==std::string_view::npos)return {};
 return trim(value.substr(end));
}
inline std::string upper_ascii(std::string value) {
 for(char& c:value)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
 return value;
}
inline std::string lower_ascii(std::string value) {
 for(char& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
 return value;
}
inline std::vector<std::string> fields(std::string_view value) {
 std::vector<std::string> out;std::size_t start=0;
 for(;;){auto end=value.find(" | ",start);if(end==std::string_view::npos){auto last=trim(value.substr(start));if(!last.empty())out.push_back(std::move(last));break;}
  auto field=trim(value.substr(start,end-start));if(!field.empty())out.push_back(std::move(field));start=end+3;}
 return out;
}
inline std::string strip_leading_pipe(std::string text) {
 text=trim(text);if(starts(text,"|"))text=trim(std::string_view(text).substr(1));return text;
}
inline std::int64_t days_from_civil(int year,unsigned month,unsigned day) {
 year-=month<=2;const auto era=(year>=0?year:year-399)/400;const auto yoe=static_cast<unsigned>(year-era*400);
 const auto adjusted_month=month>2?month-3:month+9;
 const auto doy=(153u*adjusted_month+2u)/5u+day-1u;
 const auto doe=yoe*365u+yoe/4u-yoe/100u+doy;return static_cast<std::int64_t>(era)*146097+static_cast<std::int64_t>(doe)-719468;
}
inline bool parse_utc_prefix(std::string_view line,std::int64_t& milliseconds,std::size_t& consumed) {
 if(line.size()<20)return false;
 for(std::size_t i:{0u,1u,2u,3u,5u,6u,8u,9u,11u,12u,14u,15u,17u,18u})if(line[i]<'0'||line[i]>'9')return false;
 if(line[4]!='-'||line[7]!='-'||(line[10]!=' '&&line[10]!='T')||line[13]!=':'||line[16]!=':')return false;
 auto number=[&](std::size_t at,std::size_t length){int value=0;std::from_chars(line.data()+at,line.data()+at+length,value);return value;};
 const int year=number(0,4),month=number(5,2),day=number(8,2),hour=number(11,2),minute=number(14,2),second=number(17,2);
 if(month<1||month>12||day<1||day>31||hour>23||minute>59||second>59)return false;
 std::size_t at=19;int millis=0;
 if(line[at]=='.'){
  if(line.size()<23)return false;
  for(std::size_t i=at+1;i<at+4;++i)if(line[i]<'0'||line[i]>'9')return false;
  millis=number(at+1,3);at+=4;
 }
 if(at>=line.size()||line[at]!='Z')return false;
 ++at;
 if(at<line.size()&&line[at]!=' '&&line[at]!='\t')return false;
 if(at<line.size())++at;
 const auto seconds=((days_from_civil(year,static_cast<unsigned>(month),static_cast<unsigned>(day))*24+hour)*60+minute)*60+second;
 milliseconds=seconds*1000+millis;consumed=at;return true;
}
struct SplitLine {std::string prefix,body;};
inline SplitLine split_timestamp(std::string_view line) {
 std::int64_t milliseconds{};std::size_t consumed{};
 if(parse_utc_prefix(line,milliseconds,consumed))return {format_local_timestamp(milliseconds)+" ",trim(line.substr(consumed))};
 return {"",trim(line)};
}
inline std::optional<std::string> field_value(std::string_view body,std::string_view name) {
 while(!body.empty()){
  auto token=first_token(body);
  if(token.starts_with(name)&&token.size()>name.size()&&token[name.size()]=='=')return token.substr(name.size()+1);
  auto end=body.find_first_of(" \t\r\n");if(end==std::string_view::npos)break;
  body.remove_prefix(end);auto next=body.find_first_not_of(" \t\r\n");if(next==std::string_view::npos)break;body.remove_prefix(next);
 }
 return {};
}
inline std::optional<bool> flag(std::string_view body,std::string_view name) {
 auto value=field_value(body,name);if(!value)return {};
 if(*value=="1"||*value=="true"||*value=="verified")return true;
 if(*value=="0"||*value=="false"||*value=="unavailable")return false;
 return {};
}
inline std::string capability_from_probe(std::string_view body) {
 if(trim(body)=="PROBE RESULT disabled")return "conversion off";
 auto video=flag(body,"app_video"),image=flag(body,"app_image");
 const bool app_fields=field_value(body,"app_video").has_value()||field_value(body,"app_image").has_value();
 bool diagnostic=false;
 if(!app_fields){video=flag(body,"video");image=flag(body,"image");diagnostic=video.has_value()||image.has_value();}
 if(!video&&!image)return "profiled: unknown";
 auto out=capability_summary(video.value_or(false),image.value_or(false),probe_encoders(body));
 if(diagnostic)out.insert(std::string("profiled: ").size(),"diagnostics ");
 if(!video||!image)out+="; other capability unknown";
 return out;
}
inline std::optional<unsigned> percent_value(std::string_view field) {
 if(field.empty()||field.back()!='%')return {};
 unsigned value{};auto text=field.substr(0,field.size()-1);
 auto result=std::from_chars(text.data(),text.data()+text.size(),value);
 if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return {};
 return value;
}
}

struct RenderedConsole {
 enum class Kind { hidden,event,progress,activity,clear_job } kind{Kind::hidden};
 std::string key,prefix,before,after,text;
 unsigned percent{};
 bool has_percent{},activity{},clear_job{};
 ConsoleColor color{ConsoleColor::white};
};

class ConsoleFormatter {
 JobLabels labels_;
 static RenderedConsole event(std::string prefix,std::string body,ConsoleColor color,std::string key={},bool clear=false) {
  RenderedConsole out;out.kind=RenderedConsole::Kind::event;out.prefix=std::move(prefix);out.key=std::move(key);
  out.text=fit_console_line(out.prefix+console_detail::trim(body));out.color=color;out.clear_job=clear;return out;
 }
 static RenderedConsole clear(std::string key) {
  RenderedConsole out;out.kind=RenderedConsole::Kind::clear_job;out.key=std::move(key);out.clear_job=true;return out;
 }
 RenderedConsole render_job(const std::string& prefix,const std::string& body) {
  using namespace console_detail;
  auto rest=trim(body.substr(4));auto id=first_token(rest);
  if(id.empty())return event(prefix,body,ConsoleColor::white);
  rest=strip_leading_pipe(rest.substr(id.size()));
  auto keyword_token=first_token(rest);auto keyword=upper_ascii(keyword_token);auto tail=trim(rest.substr(keyword_token.size()));
  if(keyword=="STAGE")return clear(id);
  if(keyword=="PROGRESS"||keyword=="WAITING"){
   auto content=strip_leading_pipe(tail);auto parts=fields(content);
   std::string stage,elapsed,remaining;std::optional<unsigned> percent;
   for(std::size_t i=0;i<parts.size();++i){
    if(starts(parts[i],"run ")){
     if(i>0)stage=parts[i-1];
    }else if(starts(parts[i],"elapsed ")){
     elapsed=compact_duration_text(std::string(parts[i]).substr(8));
     if(i>0&&stage.empty())stage=parts[i-1];
    }else if(starts(parts[i],"remaining ")){
     remaining=compact_duration_text(std::string(parts[i]).substr(10));
    }else if(auto value=percent_value(parts[i]))percent=value;
   }
   if(stage.empty()&&!parts.empty())stage=parts.back();
   if(auto marker=stage.find(" progress cannot be measured");marker!=std::string::npos)stage.resize(marker);
   if(stage.empty())stage="work";
   RenderedConsole out;out.kind=RenderedConsole::Kind::progress;out.key=id;
   out.prefix=prefix;out.before="job "+(labels_.live(id)?labels_.label(id):labels_.start(id))+" "+stage+" ";
   if(percent){
    out.has_percent=true;out.percent=*percent;out.after=std::to_string(*percent)+"%";
    if(!elapsed.empty())out.after+=" "+elapsed;
    if(!remaining.empty())out.after+=" eta "+remaining;
   }else{
    out.after=elapsed;
   }
   out.color=ConsoleColor::blue;return out;
  }
  if(keyword=="RETRY"){
   auto content=strip_leading_pipe(tail);auto parts=fields(content);auto stage=parts.empty()?"work":parts.front();
   labels_.start(id);return event(prefix,"job "+labels_.label(id)+" retrying "+stage,ConsoleColor::orange,id,true);
  }
  auto terminal=[&](const char* verb,ConsoleColor color){
   const bool reason_only=starts(trim(tail),"|");
   auto content=strip_leading_pipe(tail);auto parts=fields(content);std::string subject,reason;
   if(parts.size()>1){subject=parts.front();for(std::size_t i=1;i<parts.size();++i){if(!reason.empty())reason+=" | ";reason+=parts[i];}}
   else if(parts.size()==1){auto value=parts.front();if(reason_only)reason=value;else{auto colon=value.find(": ");if(colon!=std::string::npos){subject=value.substr(0,colon);reason=value.substr(colon+2);}else subject=value;}}
   labels_.finish(id);std::string text="job "+labels_.label(id)+" "+verb;
   if(!subject.empty())text+=" "+subject;
   if(!reason.empty())text+=": "+reason;
   return event(prefix,std::move(text),color,id,true);
  };
  if(keyword=="FAILED")return terminal("failed",ConsoleColor::red);
  if(keyword=="CANCELLED")return terminal("cancelled",ConsoleColor::orange);
  if(keyword=="DECLINED")return terminal("declined",ConsoleColor::orange);
  if(keyword=="SUCCEEDED")return terminal("done",ConsoleColor::green);
  if(keyword=="ACKNOWLEDGED"){
   labels_.finish(id);return event(prefix,"job "+labels_.label(id)+" acknowledged",ConsoleColor::green,id,true);
  }
  if(keyword=="STARTED"){
   auto content=strip_leading_pipe(tail);auto parts=fields(content);auto subject=parts.empty()?"":parts.front();
   labels_.start(id);std::string text="job "+labels_.label(id)+" started";
   if(!subject.empty())text+=" "+subject;
   return event(prefix,std::move(text),ConsoleColor::blue,id);
  }
  auto lowered=lower_ascii(rest);
  if(lowered.size()>=8&&lowered.ends_with(" started")){
   auto subject=trim(std::string_view(rest).substr(0,rest.size()-8));
   if(auto open=subject.find(" (");open!=std::string::npos)subject.resize(open);
   if(starts(subject,"convert "))subject=subject.substr(8);
   labels_.start(id);std::string text="job "+labels_.label(id)+" started";
   if(!subject.empty())text+=" "+subject;
   return event(prefix,std::move(text),ConsoleColor::blue,id);
  }
  return event(prefix,"job "+labels_.label(id)+" "+rest,ConsoleColor::white,id);
 }
public:
 RenderedConsole render(std::string_view line) {
  using namespace console_detail;
  auto split=split_timestamp(line);const auto& prefix=split.prefix;const auto& body=split.body;
  if(body.empty())return {};
  if(starts(body,"START ")){
   auto rest=trim(body.substr(6));auto id=first_token(rest);
   if(id.empty())return event(prefix,body,ConsoleColor::white);
   rest=trim(rest.substr(id.size()));auto capability=first_token(rest);auto subject=trim(rest.substr(capability.size()));
   labels_.start(id);std::string text="job "+labels_.label(id)+" started";
   if(!subject.empty())text+=" "+subject;
   else if(!capability.empty())text+=" "+(capability.find("video")!=std::string::npos?"video":capability.find("image")!=std::string::npos?"image":capability);
   return event(prefix,std::move(text),ConsoleColor::blue,id);
  }
  if(starts(body,"ACTIVITY")){
   RenderedConsole out;out.kind=RenderedConsole::Kind::activity;out.activity=true;out.prefix=prefix;out.color=ConsoleColor::white;
   auto rest=trim(body.substr(8));std::uint64_t active=0,pending=0;
   if(starts(rest,"|")){
    auto lower=lower_ascii(rest);if(lower.find("idle")==std::string::npos){
     auto active_at=lower.find("active jobs ");auto pending_at=lower.find("durable results awaiting ack ");
     if(active_at!=std::string::npos)std::from_chars(rest.data()+active_at+12,rest.data()+rest.size(),active);
     if(pending_at!=std::string::npos)std::from_chars(rest.data()+pending_at+29,rest.data()+rest.size(),pending);
    }
   }else{
    auto first=first_token(rest);auto second=first_token(after_first_token(rest));
    std::from_chars(first.data(),first.data()+first.size(),active);std::from_chars(second.data(),second.data()+second.size(),pending);
   }
   auto summary=connectionActivityLine(active,pending);out.text=summary.empty()?std::string{}:fit_console_line(prefix+summary);return out;
  }
  if(starts(body,"WORKER STARTED")||starts(body,"Worker starting."))return event(prefix,"worker started — tray to exit",ConsoleColor::white);
  if(starts(body,"PROBE START")||starts(body,"PROBE |"))return event(prefix,"probing converter…",ConsoleColor::white,"$probe",true);
  if(starts(body,"PROBE PROGRESS | ")){
   auto parts=fields(std::string_view(body).substr(17));
   RenderedConsole out;out.kind=RenderedConsole::Kind::progress;out.key="$probe";out.prefix=prefix;
   out.before="probing "+(parts.empty()?std::string("converter"):parts.front())+" ";
   if(parts.size()>1&&starts(parts[1],"elapsed "))out.after=parts[1].substr(8);
   out.color=ConsoleColor::white;return out;
  }
  if(starts(body,"PROBE RESULT"))return event(prefix,capability_from_probe(body),ConsoleColor::white,"$probe",true);
  if(starts(body,"PROBE SCOPE "))return event(prefix,body.substr(12),ConsoleColor::white);
  if(starts(body,"PROBE FALLBACK "))return event(prefix,"CPU fallback: "+body.substr(15),ConsoleColor::orange);
  if(starts(body,"PROBE DETAIL "))return {}; // Full evidence remains in the durable log.
  if(starts(body,"ENCODERS UPDATE")){
   auto tokens=probe_encoders(body);auto detail=field_value(body,"encoders");
   std::string text="encoders now: "+(detail&&*detail=="none"?"none":tokens.empty()?"unknown":tokens);
   if(auto failure=field_value(body,"failed"))text+="; "+*failure+" failed";
   if(field_value(body,"demoted")==std::optional<std::string>{"1"})text+="; demoted";
   if(auto fallback=field_value(body,"fallback"))text+="; retrying "+*fallback;
   return event(prefix,text,ConsoleColor::orange);
  }
  if(starts(body,"CPU conversion probes passed"))return event(prefix,capability_from_probe(body),ConsoleColor::white);
  if(starts(body,"PAIRING CODE")){
   auto rest=trim(body.substr(12));auto code=first_token(rest);auto seconds_text=first_token(after_first_token(rest));
   long long seconds=0;std::from_chars(seconds_text.data(),seconds_text.data()+seconds_text.size(),seconds);
   auto text="pairing code "+code+" · "+format_duration_compact(seconds*1000);
   return event(prefix,std::move(text),ConsoleColor::orange);
  }
  if(starts(body,"PAIRING APPROVED"))return event(prefix,"pairing approved",ConsoleColor::green);
  if(starts(body,"RESULT PENDING ACK ")){
   auto rest=trim(body.substr(19));auto id=first_token(rest);auto status=lower_ascii(first_token(after_first_token(rest)));
   auto color=status.find("fail")!=std::string::npos||status.find("cancel")!=std::string::npos?ConsoleColor::orange:ConsoleColor::green;
   return event(prefix,"job "+labels_.label(id)+" result queued",color,id);
  }
  if(starts(body,"RESULT ACKNOWLEDGED ")){
   auto id=first_token(trim(body.substr(20)));
   labels_.finish(id);return event(prefix,"job "+labels_.label(id)+" acknowledged",ConsoleColor::green,id,true);
  }
  if(starts(body,"JOB "))return render_job(prefix,body);
  if(starts(body,"END ")){
   auto rest=trim(body.substr(4));auto id=rest.substr(0,rest.find_first_of(": "));auto reason=rest.find(": ")==std::string::npos?std::string{}:rest.substr(rest.find(": ")+2);
   labels_.finish(id);auto text="job "+labels_.label(id)+" failed";if(!reason.empty())text+=": "+reason;
   return event(prefix,std::move(text),ConsoleColor::red,id,true);
  }
  if(starts(body,"STATE ")){
   auto rest=trim(body.substr(6));auto name=upper_ascii(first_token(rest));auto detail=after_first_token(rest);
   if(name=="CONNECTED")return event(prefix,"connected",ConsoleColor::white);
   if(name=="RECONNECTED")return event(prefix,"reconnected",ConsoleColor::white);
   if(name=="OFFLINE")return event(prefix,"offline"+(detail.empty()?std::string{}:": "+detail),ConsoleColor::orange);
   if(name=="ACTION")return event(prefix,"action required"+(detail.empty()?std::string{}:": "+detail),ConsoleColor::orange);
   if(name=="REVOKED")return event(prefix,"access revoked",ConsoleColor::orange);
   if(name=="UNCONFIGURED")return event(prefix,"not configured",ConsoleColor::orange);
   if(name=="PAUSED")return event(prefix,"paused",ConsoleColor::orange);
   if(name=="RESUMED")return event(prefix,"resumed",ConsoleColor::green);
   if(name=="DRAINING")return event(prefix,"draining",ConsoleColor::orange);
   if(name=="STOPPING")return event(prefix,"stopping jobs",ConsoleColor::orange);
   if(name=="PRIORITY")return event(prefix,"priority "+(detail.empty()?"unknown":detail),ConsoleColor::white);
   if(name=="PREFERENCE"){
    auto value=detail;std::string key="task";
    if(starts(value,"video=")){key="video";value=value.substr(6);}else if(starts(value,"image=")){key="image";value=value.substr(6);}
    if(value=="disabled")value="off";
    return event(prefix,key+" "+value,ConsoleColor::white);
   }
   if(name=="PROBE_FAILURE")return event(prefix,"probe failed"+(detail.empty()?std::string{}:": "+detail),ConsoleColor::red,"$probe",true);
   if(name=="JOURNAL_FAILURE")return event(prefix,"journal error — jobs paused",ConsoleColor::red);
   if(name=="WATCHDOG_FAILURE")return event(prefix,"watchdog error — jobs paused"+(detail.empty()?std::string{}:": "+detail),ConsoleColor::red);
   if(name=="EXITED")return event(prefix,"worker exited",ConsoleColor::white);
   return event(prefix,detail.empty()?lower_ascii(name):lower_ascii(name)+": "+detail,ConsoleColor::white);
  }
  auto lower=lower_ascii(body);
  if(starts(body,"CONNECTED"))return event(prefix,"connected",ConsoleColor::white);
  if(starts(body,"OFFLINE:"))return event(prefix,"offline:"+body.substr(8),ConsoleColor::orange);
  if(starts(body,"REVOKED:"))return event(prefix,"access revoked",ConsoleColor::orange);
  if(starts(body,"ACTION REQUIRED:"))return event(prefix,"action required:"+body.substr(16),ConsoleColor::orange);
  if(starts(body,"UNCONFIGURED:"))return event(prefix,"not configured",ConsoleColor::orange);
  if(starts(body,"Paused"))return event(prefix,"paused",ConsoleColor::orange);
  if(starts(body,"Resumed"))return event(prefix,"resumed",ConsoleColor::green);
  if(starts(body,"DRAINING"))return event(prefix,"draining",ConsoleColor::orange);
  if(starts(body,"EXITING"))return event(prefix,"stopping jobs",ConsoleColor::orange);
  if(starts(body,"JOURNAL FAILURE"))return event(prefix,"journal error — jobs paused",ConsoleColor::red);
  if(starts(body,"Watchdog check failed"))return event(prefix,"watchdog error — jobs paused",ConsoleColor::red);
  if(starts(body,"Rejected offer")||starts(body,"OFFER DECLINED"))return event(prefix,"offer declined",ConsoleColor::orange);
  if(starts(body,"Could not preserve child"))return event(prefix,"child log not saved",ConsoleColor::orange);
  if(starts(body,"Machine priority:"))return event(prefix,"priority "+trim(body.substr(17)),ConsoleColor::white);
  if(starts(body,"Task preference changed"))return event(prefix,"task preference changed",ConsoleColor::white);
  if(starts(body,"Worker exited"))return event(prefix,"worker exited",ConsoleColor::white);
  if(lower.find("failed")!=std::string::npos||lower.find("error")!=std::string::npos)return event(prefix,body,ConsoleColor::red);
  if(lower.find("cancel")!=std::string::npos||lower.find("retry")!=std::string::npos||lower.find("offline")!=std::string::npos)return event(prefix,body,ConsoleColor::orange);
  if(lower.find("acknowledged")!=std::string::npos||lower.find("succeeded")!=std::string::npos)return event(prefix,body,ConsoleColor::green);
  return event(prefix,body,ConsoleColor::white);
 }
};

struct LiveConsoleRow {
 std::string key,prefix,before,after,text;
 unsigned percent{};
 bool has_percent{},activity{};
 ConsoleColor color{ConsoleColor::blue};
};

struct ConsoleFeed {
 std::optional<ConsoleLine> event;
 std::optional<ConsoleLine> activity;
 bool redraw{};
};

class LiveConsoleView {
 std::vector<LiveConsoleRow> rows_;
 ConsoleFormatter formatter_;
 void erase(const std::string& key){rows_.erase(std::remove_if(rows_.begin(),rows_.end(),[&](const auto& row){return row.key==key;}),rows_.end());}
public:
 ConsoleFeed consume(const std::string& line) {
  auto rendered=formatter_.render(line);ConsoleFeed feed;
  switch(rendered.kind){
   case RenderedConsole::Kind::hidden:return feed;
   case RenderedConsole::Kind::clear_job:erase(rendered.key);feed.redraw=true;return feed;
   case RenderedConsole::Kind::event:
    if(rendered.clear_job)erase(rendered.key);
    feed.event=ConsoleLine{rendered.text,rendered.color};feed.redraw=true;return feed;
   case RenderedConsole::Kind::progress:{
    auto found=std::find_if(rows_.begin(),rows_.end(),[&](const auto& row){return row.key==rendered.key;});
    LiveConsoleRow row;row.key=rendered.key;row.prefix=rendered.prefix;row.before=rendered.before;row.after=rendered.after;
    row.percent=rendered.percent;row.has_percent=rendered.has_percent;row.color=rendered.color;
    if(rendered.key=="$probe")feed.activity=ConsoleLine{render_row(row,false),rendered.color};
    if(found==rows_.end())rows_.push_back(std::move(row));else *found=std::move(row);
    feed.redraw=true;return feed;
   }
   case RenderedConsole::Kind::activity:
    if(rendered.text.empty())erase("$activity");
    else{
     auto found=std::find_if(rows_.begin(),rows_.end(),[](const auto& row){return row.activity;});
     LiveConsoleRow row;row.key="$activity";row.text=rendered.text;row.activity=true;row.color=rendered.color;
     if(found==rows_.end())rows_.push_back(std::move(row));else *found=std::move(row);
     feed.activity=ConsoleLine{rendered.text,rendered.color};
    }
    feed.redraw=true;return feed;
  }
  return feed;
 }
 const std::vector<LiveConsoleRow>& rows() const{return rows_;}
 std::string render_row(const LiveConsoleRow& row,bool color) const {
  std::string line;
  if(row.activity)line=row.text;
  else{
   const auto fixed=console_text_width(row.prefix+row.before+row.after);
   const auto available=fixed<console_width?console_width-fixed:0;
   const auto bar_width=available>3?std::min<std::size_t>(12,available-3):0;
   auto before=row.before;
   if(row.key=="$probe"){
    const auto reserved=console_text_width(row.prefix+row.after)+1;
    before=fit_console_line(before,reserved<console_width?console_width-reserved:0)+" ";
   }
   line=row.prefix+before;
   if(row.has_percent){
    const auto filled=std::min<std::size_t>(bar_width,row.percent*bar_width/100);
    line+='[';line.append(filled,'#');line.append(bar_width-filled,'-');line+="] ";
   }
   line+=row.after;
  }
  line=fit_console_line(line);
  while(!line.empty()&&line.back()==' ')line.pop_back();
  if(color)return ansi_color(row.color)+line+"\x1b[0m";
  return line;
 }
 std::string render(ConsolePresentation mode) const {
  std::string out;
  for(const auto& row:rows_)out+=render_row(row,mode.interactive&&mode.color)+'\n';
  return out;
 }
};
}
