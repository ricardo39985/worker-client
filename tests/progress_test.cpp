#include "test.hpp"
#include "ow/console.hpp"
#include "ow/progress.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

namespace {
std::int64_t epoch_ms(int year,unsigned month,unsigned day,int hour,int minute,int second) {
 using namespace std::chrono;
 const auto point=sys_days{std::chrono::year{year}/std::chrono::month{month}/std::chrono::day{day}}
  +hours{hour}+minutes{minute}+seconds{second};
 return duration_cast<milliseconds>(point.time_since_epoch()).count();
}
struct TzGuard {
 std::optional<std::string> old;
 static void set(const char* value) {
#ifdef _WIN32
  _putenv_s("TZ",value?value:"");_tzset();
#else
  if(value)setenv("TZ",value,1);else unsetenv("TZ");
  tzset();
#endif
 }
 explicit TzGuard(const char* value) {
  if(const char* current=std::getenv("TZ"))old=current;
  set(value);
 }
 ~TzGuard(){
  set(old?old->c_str():nullptr);
 }
};
void check_frame(const std::string& frame) {
 std::size_t start=0;
 while(start<frame.size()){
  auto end=frame.find('\n',start);if(end==std::string::npos)end=frame.size();
  auto line=frame.substr(start,end-start);
  CHECK(ow::console_text_width(line)<=ow::console_width);
  start=end+1;
 }
}
std::string start_line(const std::string& id,const std::string& subject="feed.webp") {
 return "2026-10-17 15:01:28.424Z START "+id+" conversion.image.jpeg "+subject;
}
std::string progress_line(const std::string& id,unsigned percent,const std::string& stage="convert feed.webp") {
 return "2026-10-17 15:01:29.000Z JOB "+id+" PROGRESS | "+stage+" | run 1 | "+std::to_string(percent)+"% | elapsed 1m12s | remaining 1m28s";
}
}

TEST("idle connection emits no activity and active states stay terse") {
 for(unsigned tick=0;tick<100;++tick)CHECK(ow::connectionActivityLine(0,0).empty());
 CHECK(ow::connectionActivityLine(1,0)=="1 active · 0 unacked");
 CHECK(ow::connectionActivityLine(0,3)=="0 active · 3 unacked");
 CHECK(ow::connectionActivityRecord(0,0)=="ACTIVITY 0 0");
}

TEST("encoder timeline and byte accounting behavior are preserved") {
 ow::ProgressCounter p;
 p.begin("conversion",1000);
 auto s=p.snapshot(6000);CHECK(!s.percent&&s.completed==0&&s.elapsed_ms==5000&&!s.quiet);
 s=p.snapshot(32000);CHECK(s.quiet&&!s.percent&&s.completed==0);
 p.begin("download",40000,10);p.update(4,41000);s=p.snapshot(42000);
 CHECK(s.percent&&*s.percent==40&&s.completed==4&&s.elapsed_ms==2000&&!s.quiet);
 p.update(3,42000);s=p.snapshot(72000);CHECK(s.completed==4&&s.quiet);
 p.update(5,73000);s=p.snapshot(73000);CHECK(!s.quiet&&*s.percent==50);
 p.begin("next stage",74000);s=p.snapshot(74000);CHECK(s.completed==0&&!s.percent&&s.elapsed_ms==0);
 p.begin("download",75000,10);p.update(4,76000);p.finish(false,77000);s=p.snapshot(78000);
 CHECK(s.finished&&!s.succeeded&&s.completed==4&&*s.percent==40);
 p.update(10,79000);p.finish(true,79000);s=p.snapshot(80000);
 CHECK(!s.succeeded&&s.completed==4&&s.elapsed_ms==2000);
 p.begin("upload",80000,10);p.update(10,81000);p.finish(true,82000);s=p.snapshot(83000);
 CHECK(s.finished&&s.succeeded&&*s.percent==100&&s.elapsed_ms==2000);

 ow::EncoderProgress encoder;
 auto reporting=ow::encoder_progress_arguments({L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"input",L"output"});
 CHECK(std::find(reporting.begin(),reporting.end(),L"pipe:1")!=reporting.end());
 CHECK(std::find(reporting.begin(),reporting.end(),L"info")!=reporting.end());
 encoder.consume("ffmpeg version synthetic\n  Duration: 00:02:00.000, start: 0.000000\n");
 encoder.consume("total_size=90000000\nout_time_us=30000000\nprogress=continue\n");
 CHECK(encoder.duration_us()==120000000);CHECK(encoder.position_us()==30000000);
 p.begin("convert known.mp4",100000);
 p.set_total(encoder.duration_us(),100010);p.update(encoder.position_us(),130000);s=p.snapshot(130000);
 CHECK(s.percent&&*s.percent==25&&s.completed==30000000&&s.total==120000000);
 CHECK(s.remaining_ms&&*s.remaining_ms==90000);
 encoder.consume("out_time_us=6000");encoder.consume("0000\nprogress=continue\n");
 CHECK(encoder.position_us()==60000000);
 encoder.consume("out_time_us=not-a-number\nout_time_us=59000000\n");
 CHECK(encoder.position_us()==60000000);
 p.update(encoder.position_us(),160000);CHECK(*p.snapshot(160000).percent==50);
 p.update(59000000,170000);CHECK(*p.snapshot(170000).percent==50);
}

TEST("unmeasurable stages say only stage and elapsed once") {
 ow::ProgressCounter p;p.begin("probing converter",200000);auto s=p.snapshot(212000);
 ow::ProgressLogPolicy policy;auto line=policy.line("abcdef12-abcd-4abc-8abc-abcdefabcdef","converter",s);
 CHECK(line.has_value());
 CHECK(line->find(" WAITING ")!=std::string::npos);
 CHECK(line->find("cannot be measured")==std::string::npos);
 CHECK(line->find("probing converter")!=std::string::npos);
 CHECK(line->find("elapsed 12s")!=std::string::npos);
 CHECK(!policy.line("abcdef12-abcd-4abc-8abc-abcdefabcdef","converter",p.snapshot(240000)));
 policy.forget("abcdef12-abcd-4abc-8abc-abcdefabcdef");
 CHECK(policy.line("abcdef12-abcd-4abc-8abc-abcdefabcdef","converter",p.snapshot(240000)).has_value());
 ow::LiveConsoleView view;auto feed=view.consume(*line);CHECK(feed.redraw);
 auto frame=view.render({false,false,false});
 CHECK(frame.find("job cdef")!=std::string::npos);
 CHECK(frame.find("probing converter 12s")!=std::string::npos);
 CHECK(frame.find("cannot be measured")==std::string::npos);
}

TEST("a repeated stage is a visible retry with one stable suffix") {
 const std::string id="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 ow::ProgressCounter p;p.begin("convert feed.webp",100000,120000000);p.update(30000000,130000);
 ow::ProgressLogPolicy policy;
 auto first=policy.line(id,"feed.webp",p.snapshot(130000));CHECK(first&&first->find(" PROGRESS ")!=std::string::npos);
 p.begin("convert feed.webp",180000,120000000);p.update(0,180000);
 auto retry=policy.line(id,"feed.webp",p.snapshot(180000));
 CHECK(retry&&retry->find(" RETRY ")!=std::string::npos&&retry->find("run 2")!=std::string::npos);
 auto next=policy.line(id,"feed.webp",p.snapshot(190000));
 CHECK(next&&next->find(" PROGRESS ")!=std::string::npos&&next->find("0%")!=std::string::npos);
 ow::LiveConsoleView view;
 view.consume("2026-10-17 15:01:28.424Z START "+id+" conversion.image.jpeg feed.webp");
 auto feed=view.consume(*retry);CHECK(feed.event.has_value());
 CHECK(feed.event->text.find("job 93c7 retrying convert feed.webp")!=std::string::npos);
 CHECK(feed.event->color==ow::ConsoleColor::orange);
 auto done=view.consume("2026-10-17 15:02:00.000Z RESULT PENDING ACK "+id+" succeeded");
 CHECK(done.event.has_value());
 CHECK(done.event->text.ends_with(" job 93c7 result queued"));
 CHECK(done.event->color==ow::ConsoleColor::green);
}

TEST("local timestamp format is exact through midnight month and zone changes") {
 CHECK(ow::format_duration(0)=="0s");
 CHECK(ow::format_duration(44000)=="44s");
 CHECK(ow::format_duration(72000)=="1m 12s");
 CHECK(ow::format_duration(2*60*60*1000+5*60*1000)=="2h 05m");
 CHECK(ow::format_duration_compact(44000)=="44s");
 CHECK(ow::format_duration_compact(72000)=="1m12s");
 CHECK(ow::format_utc_timestamp(0)=="1970-01-01 00:00:00.000Z");
 {
  TzGuard zone{"UTC0"};
  CHECK(ow::format_local_timestamp(epoch_ms(2026,10,7,9,1,28))=="07/10 09:01:28");
  auto midnight=epoch_ms(2026,4,8,23,59,59);
  CHECK(ow::format_local_timestamp(midnight)=="08/04 23:59:59");
  CHECK(ow::format_local_timestamp(midnight+1000)=="09/04 00:00:00");
  auto month_end=epoch_ms(2026,4,30,23,59,59);
  CHECK(ow::format_local_timestamp(month_end)=="30/04 23:59:59");
  CHECK(ow::format_local_timestamp(month_end+1000)=="01/05 00:00:00");
  auto last= epoch_ms(2026,12,31,23,59,59);
  CHECK(ow::format_local_timestamp(last)=="31/12 23:59:59");
  CHECK(ow::format_local_timestamp(last+1000)=="01/01 00:00:00");
  CHECK(ow::format_local_timestamp(last+424)=="31/12 23:59:59");
  CHECK(ow::format_utc_timestamp(last+424)=="2026-12-31 23:59:59.424Z");
  CHECK(ow::format_utc_timestamp(last+1000)=="2027-01-01 00:00:00.000Z");
 }
 {
  TzGuard zone{"EST4"};
  CHECK(ow::format_local_timestamp(epoch_ms(2026,10,17,15,1,28))=="17/10 11:01:28");
  ow::LiveConsoleView view;auto feed=view.consume("2026-10-17 15:01:28.424Z WORKER STARTED");
  CHECK(feed.event.has_value());CHECK(feed.event->text.starts_with("17/10 11:01:28 "));
 }
 {
#ifdef _WIN32
  // The CRT uses US transition rules for this TZ spelling.
  TzGuard zone{"EST5EDT"};
#else
  TzGuard zone{"EST5EDT,M3.2.0/2,M11.1.0/2"};
#endif
  CHECK(ow::format_local_timestamp(epoch_ms(2026,7,1,12,0,0))=="01/07 08:00:00");
  CHECK(ow::format_local_timestamp(epoch_ms(2026,1,1,12,0,0))=="01/01 07:00:00");
  CHECK(ow::format_local_timestamp(epoch_ms(2026,3,8,6,59,59))=="08/03 01:59:59");
  CHECK(ow::format_local_timestamp(epoch_ms(2026,3,8,7,0,0))=="08/03 03:00:00");
  CHECK(ow::format_local_timestamp(epoch_ms(2026,11,1,5,59,59))=="01/11 01:59:59");
  CHECK(ow::format_local_timestamp(epoch_ms(2026,11,1,6,0,0))=="01/11 01:00:00");
  CHECK(ow::format_utc_timestamp(epoch_ms(2026,11,1,6,0,0)+424)=="2026-11-01 06:00:00.424Z");
 }
}

TEST("job suffixes are lowercase last four and stable for a job life") {
 const std::string id="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 CHECK(ow::job_suffix(id)=="93c7");
 CHECK(ow::job_suffix("ABC-DEF")=="cdef");
 ow::JobLabels labels;CHECK(labels.start(id)=="93c7");CHECK(labels.start(id)=="93c7");
 CHECK(labels.start("2c5195c3-6c0b-4cd3-b1de-cab91af048c7")=="48c7");
 CHECK(labels.live(id));labels.finish(id);CHECK(!labels.live(id));CHECK(labels.label(id)=="93c7");
}

TEST("two live jobs with the same four characters stay distinguishable") {
 const std::string first="11111111-1111-4111-8111-0000000093c7";
 const std::string second="22222222-2222-4222-8222-0000000193c7";
 ow::JobLabels labels;auto short_first=labels.start(first);auto short_second=labels.start(second);
 CHECK(short_first=="93c7");CHECK(short_second!="93c7");CHECK(short_second.ends_with("93c7"));
 CHECK(short_second.size()>short_first.size());CHECK(labels.label(first)==short_first);
 ow::LiveConsoleView view;
 view.consume(start_line(first,"a.jpg"));view.consume(start_line(second,"b.jpg"));
 view.consume(progress_line(first,10,"convert a.jpg"));view.consume(progress_line(second,20,"convert b.jpg"));
 auto frame=view.render({false,false,false});
 CHECK(frame.find("job "+short_first)!=std::string::npos);
 CHECK(frame.find("job "+short_second)!=std::string::npos);
 CHECK(frame.find("job "+short_first+" convert a.jpg")!=std::string::npos);
 CHECK(frame.find("job "+short_second+" convert b.jpg")!=std::string::npos);
 CHECK(view.rows().size()==2);check_frame(frame);
 std::string durable=start_line(first,"a.jpg")+"\r\n"+start_line(second,"b.jpg")+"\r\n";
 CHECK(ow::resolve_job_suffix(durable,short_second)==second);
 CHECK(durable.find(first)!=std::string::npos&&durable.find(second)!=std::string::npos);
 labels.finish(first);
 CHECK(labels.start("33333333-3333-4333-8333-0000000093c7")=="93c7");
}

TEST("a shown suffix resolves to exactly one full attempt id in the durable log") {
 const std::string id="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 std::string durable="2026-10-17 15:01:28.424Z "+start_line(id).substr(25)+"\r\n";
 durable+="2026-10-17 15:02:00.000Z RESULT PENDING ACK "+id+" succeeded\r\n";
 auto resolved=ow::resolve_job_suffix(durable,"93c7");
 CHECK(resolved.has_value());CHECK(*resolved==id);
 CHECK(durable.find(*resolved)!=std::string::npos);
 CHECK(durable.find("\x1b")==std::string::npos);
 const std::string collision="11111111-1111-4111-8111-0000000093c7 22222222-2222-4222-8222-0000000193c7";
 CHECK(!ow::resolve_job_suffix(collision,"93c7").has_value());
}

TEST("restart needs no counter and never implies continuation") {
 const std::string before="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 const std::string after="2c5195c3-6c0b-4cd3-b1de-cab91af048c7";
 ow::JobLabels first_session;CHECK(first_session.start(before)=="93c7");
 ow::JobLabels after_restart;CHECK(after_restart.start(before)=="93c7");
 CHECK(after_restart.start(after)=="48c7");
 ow::LiveConsoleView view;auto feed=view.consume(start_line(after,"new.webp"));
 CHECK(feed.event.has_value());
 CHECK(feed.event->text.find("job 48c7 started new.webp")!=std::string::npos);
 CHECK(feed.event->text.find("continued")==std::string::npos);
 CHECK(feed.event->text.find("job 1 ")==std::string::npos);
}

TEST("every rendered line fits eighty columns including an impossible message") {
 auto repeated=[](const std::string& prefix,char value,std::size_t count){std::string out=prefix;out.append(count,value);return out;};
 std::vector<std::string> lines{
  start_line("7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7",std::string(240,'x')),
  repeated("2026-10-17 15:02:00.000Z JOB 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7 FAILED feed.webp | ",'r',300),
  repeated("2026-10-17 15:02:00.000Z JOB 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7 PROGRESS | ",'s',240)+" | run 1 | 99% | elapsed 1m12s | remaining 9h59m",
  "2026-10-17 15:02:00.000Z PAIRING CODE 123456 900",
  "2026-10-17 15:02:00.000Z RESULT PENDING ACK 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7 failed"
 };
 ow::LiveConsoleView view;
 for(const auto& line:lines){
  auto feed=view.consume(line);if(feed.event)CHECK(ow::console_text_width(feed.event->text)<=ow::console_width);
  auto frame=view.render({false,false,false});check_frame(frame);
 }
 auto long_failure=lines[1];ow::LiveConsoleView failure_view;auto feed=failure_view.consume(long_failure);
 CHECK(feed.event.has_value());CHECK(feed.event->text.ends_with("…"));
 CHECK(feed.event->text.find("failed feed.webp")!=std::string::npos);
 CHECK(feed.event->text.find('\n')==std::string::npos);
 const std::string token(400,'z');ow::LiveConsoleView token_view;auto token_feed=token_view.consume("2026-10-17 15:02:00.000Z "+token);
 CHECK(token_feed.event.has_value());CHECK(ow::console_text_width(token_feed.event->text)<=ow::console_width);
}

TEST("old durable lines still render plainly without breaking the viewer") {
 ow::LiveConsoleView view;
 auto worker=view.consume("2026-10-17 15:01:28.424Z Worker starting. Closing the console only closes the viewer; use the tray to exit.");
 CHECK(worker.event&&worker.event->text.ends_with(" worker started — tray to exit"));
 CHECK(worker.event&&worker.event->text.size()>=15&&worker.event->text[2]=='/'&&worker.event->text[5]==' ');
 auto probe=view.consume("2026-10-17 15:01:29.000Z PROBE | checking approved converter and real samples; this check has no measurable progress");
 CHECK(probe.event&&probe.event->text.ends_with(" probing converter…"));
 auto old=view.consume("2026-10-17 15:01:28.424Z JOB 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7 | convert feed.webp (output bytes; percentage unavailable) started");
 CHECK(old.event.has_value());CHECK(old.event->text.find("job 93c7 started feed.webp")!=std::string::npos);
 CHECK(old.event->text.find("output bytes")==std::string::npos);
 auto activity=view.consume("2026-10-17 15:01:30.000Z ACTIVITY | active jobs 1 | durable results awaiting ACK 0");
 CHECK(activity.activity.has_value());CHECK(activity.activity->text.find("1 active · 0 unacked")!=std::string::npos);
 auto backlog=view.consume("2026-10-17 15:01:30.100Z ACTIVITY | active jobs 0 | durable results awaiting ACK 3");
 CHECK(backlog.activity.has_value());CHECK(backlog.activity->text.find("0 active · 3 unacked")!=std::string::npos);
 auto pending=view.consume("2026-10-17 15:02:00.000Z RESULT PENDING ACK 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7");
 CHECK(pending.event&&pending.event->text.find("job 93c7 result queued")!=std::string::npos);
 auto old_progress=view.consume("2026-10-17 15:01:31.000Z JOB 7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7 | PROGRESS | feed.webp | convert | run 1 | 50% | elapsed 1m 00s | remaining 1m 00s");
 CHECK(old_progress.redraw);CHECK(view.rows().size()>=1);
 auto frame=view.render({false,false,false});
 CHECK(frame.find('\x1b')==std::string::npos&&frame.find('\r')==std::string::npos);check_frame(frame);
}

TEST("exactly five colours carry one consistent meaning") {
 const std::string id="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 const std::array<std::pair<const char*,ow::ConsoleColor>,5> palette{{
  {"\x1b[91m",ow::ConsoleColor::red},{"\x1b[33m",ow::ConsoleColor::orange},
  {"\x1b[92m",ow::ConsoleColor::green},{"\x1b[94m",ow::ConsoleColor::blue},{"\x1b[39m",ow::ConsoleColor::white}}};
 for(const auto& [code,color]:palette)CHECK(std::string(ow::ansi_color(color))==code);
 ow::ConsoleFormatter formatter;
 CHECK(formatter.render(start_line(id)).color==ow::ConsoleColor::blue);
 CHECK(formatter.render(progress_line(id,20)).color==ow::ConsoleColor::blue);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z JOB "+id+" FAILED feed.webp | wrong bytes").color==ow::ConsoleColor::red);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z JOB "+id+" CANCELLED feed.webp | operator").color==ow::ConsoleColor::orange);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z RESULT ACKNOWLEDGED "+id).color==ow::ConsoleColor::green);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z PROBE START").color==ow::ConsoleColor::white);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z JOB "+id+" FAILED feed.webp | wrong bytes").text.find("failed")!=std::string::npos);
 CHECK(formatter.render("2026-10-17 15:02:00.000Z JOB "+id+" CANCELLED feed.webp | operator").text.find("cancelled")!=std::string::npos);
 auto declined=formatter.render("2026-10-17 15:02:00.000Z JOB "+id+" DECLINED | invalid offer");
 CHECK(declined.color==ow::ConsoleColor::orange);CHECK(declined.text.find("declined: invalid offer")!=std::string::npos);
}

TEST("progress rows keep a bounded bar and vanish on terminal events") {
 const std::string id="7b1511a3-e2b6-4a84-94cc-d7bc7eea93c7";
 ow::LiveConsoleView view;
 auto zero=view.consume(progress_line(id,0));CHECK(zero.redraw);CHECK(view.rows().size()==1);
 auto frame=view.render({true,true,false});CHECK(frame.find("[------------]")!=std::string::npos);
 CHECK(frame.find('\x1b')!=std::string::npos);
 check_frame(view.render({false,false,false}));
 auto plain=view.render({false,true,true});CHECK(plain.find('\x1b')==std::string::npos);
 auto no_color=view.render({true,false,false});CHECK(no_color.find('\x1b')==std::string::npos);
 auto cancelled=view.consume("2026-10-17 15:02:00.000Z JOB "+id+" CANCELLED feed.webp | operator requested cancellation");
 CHECK(cancelled.event.has_value());CHECK(cancelled.event->text.find("cancelled")!=std::string::npos);
 CHECK(view.rows().empty());
}

TEST("four concurrent rows and activity stay readable and cancellation clears at any percent") {
 ow::LiveConsoleView view;
 for(unsigned i=0;i<4;++i){
  const auto id="0000000"+std::to_string(i)+"-1111-4111-8111-00000000000"+std::to_string(i);
  view.consume(start_line(id,"item"+std::to_string(i)+".mp4"));
  view.consume("2026-10-17 15:01:29.000Z JOB "+id+" PROGRESS | convert item"+std::to_string(i)+".mp4 | run 1 | 5% | elapsed 2s | remaining 38s");
 }
 auto activity=view.consume("2026-10-17 15:01:30.000Z ACTIVITY 4 1");
 CHECK(activity.activity.has_value());
 auto frame=view.render({false,false,false});
 CHECK(view.rows().size()==5);CHECK(std::count(frame.begin(),frame.end(),'\n')==5);
 CHECK(frame.find("1 active · 0 unacked")==std::string::npos);
 CHECK(frame.find("4 active · 1 unacked")!=std::string::npos);
 check_frame(frame);
 for(auto percent:{0u,99u}){
  const auto id="99999999-9999-4999-8999-0000000000"+std::to_string(percent);
  ow::LiveConsoleView one;
  one.consume("2026-10-17 15:01:29.000Z JOB "+id+" PROGRESS | convert clip.mp4 | run 1 | "+std::to_string(percent)+"% | elapsed 1m00s | remaining 1s");
  CHECK(one.rows().size()==1);
  auto cancel=one.consume("2026-10-17 15:02:00.000Z JOB "+id+" CANCELLED clip.mp4 | operator requested cancellation");
  CHECK(cancel.event.has_value());CHECK(one.rows().empty());
 }
}

TEST("many jobs keep suffix-to-id resolvability and concurrent accounting") {
 std::string durable;
 std::vector<std::string> ids;
 for(unsigned i=0;i<200;++i){
  std::ostringstream id;id<<"00000000-0000-4000-8000-"<<"00000000"<<std::hex<<std::setw(4)<<std::setfill('0')<<i;
  ids.push_back(id.str());
  durable+="2026-10-17 15:00:00.000Z START "+ids.back()+" conversion.image.jpeg f"+std::to_string(i)+".webp\r\n";
 }
 for(const auto& id:ids){
  auto suffix=ow::job_suffix(id);auto found=ow::resolve_job_suffix(durable,suffix);
  CHECK(found.has_value());CHECK(*found==id);CHECK(durable.find(*found)!=std::string::npos);
 }
 ow::ProgressCounter counter;counter.begin("concurrent byte accounting",90000,400);
 std::vector<std::thread> writers;
 for(unsigned n=0;n<4;++n)writers.emplace_back([&,n]{for(unsigned i=1;i<=100;++i)counter.update(n*100+i,90001+i);});
 for(auto& thread:writers)thread.join();
 CHECK(counter.snapshot(92000).completed==400);
}

TEST("the ready line names the encoders the worker actually reported") {
 const std::string body="PROBE RESULT app_video=1 app_image=1 video=1 image=1 encoders=cpu,qsv,amf";
 CHECK(ow::console_detail::capability_from_probe(body)=="ready: video+image (cpu, qsv, amf)");
}

TEST("a probe record without encoder detail claims no backend") {
 const std::string body="PROBE RESULT app_video=1 app_image=1 video=1 image=1";
 CHECK(ow::console_detail::capability_from_probe(body)=="ready: video+image");
 CHECK(ow::console_detail::capability_from_probe(body).find("cpu")==std::string::npos);
}

TEST("a malformed encoder list cannot invent a backend") {
 const std::string body="PROBE RESULT app_video=1 app_image=0 encoders=<script>";
 CHECK(ow::console_detail::capability_from_probe(body)=="ready: video");
 CHECK(ow::console_detail::capability_from_probe(body).find("script")==std::string::npos);
}
