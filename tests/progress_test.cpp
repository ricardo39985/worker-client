#include "ow/progress.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
void check(bool value){if(!value)throw std::runtime_error("Progress contract failed");}
int main(){
 try{
  for(unsigned tick=0;tick<100;++tick)
   if(!ow::connectionActivityLine(0,0).empty())throw std::runtime_error("Idle connection must not emit activity lines");
  auto active=ow::connectionActivityLine(2,0);
  check(active.find("active jobs 2")!=std::string::npos);
  auto waiting=ow::connectionActivityLine(0,3);
  check(waiting.find("durable results awaiting ACK 3")!=std::string::npos);
  check(ow::connectionActivityLine(0,0).empty()); // Backlog drained: console becomes quiet again.
  ow::ProgressCounter p;
  p.begin("conversion",1000);
  auto s=p.snapshot(6000);check(!s.percent&&s.completed==0&&s.elapsed_ms==5000&&!s.quiet);
  s=p.snapshot(32000);check(s.quiet&&!s.percent&&s.completed==0);
  p.begin("download",40000,10);p.update(4,41000);s=p.snapshot(42000);
  check(s.percent&&*s.percent==40&&s.completed==4&&s.elapsed_ms==2000&&!s.quiet);
  p.update(3,42000);s=p.snapshot(72000);check(s.completed==4&&s.quiet); // backward counter is not new activity
  p.update(5,73000);s=p.snapshot(73000);check(!s.quiet&&*s.percent==50);
  p.begin("next stage",74000);s=p.snapshot(74000);check(s.completed==0&&!s.percent&&s.elapsed_ms==0);
  p.begin("download",75000,10);p.update(4,76000);p.finish(false,77000);s=p.snapshot(78000);
  check(s.finished&&!s.succeeded&&s.completed==4&&*s.percent==40); // failure never becomes 100%
  p.update(10,79000);p.finish(true,79000);s=p.snapshot(80000);
  check(!s.succeeded&&s.completed==4&&s.elapsed_ms==2000); // duplicate completion cannot rewrite failure
  p.begin("upload",80000,10);p.update(10,81000);p.finish(true,82000);s=p.snapshot(83000);
  check(s.finished&&s.succeeded&&*s.percent==100&&s.elapsed_ms==2000);

  // Encoder position, not output size, is the conversion extent. The output is
  // deliberately already large while the precisely simulated timeline is 25%.
  ow::EncoderProgress encoder;
  auto reporting=ow::encoder_progress_arguments({L"-nostdin",L"-hide_banner",L"-loglevel",L"error",L"input",L"output"});
  check(std::find(reporting.begin(),reporting.end(),L"pipe:1")!=reporting.end());
  check(std::find(reporting.begin(),reporting.end(),L"info")!=reporting.end());
  encoder.consume("ffmpeg version synthetic\n  Duration: 00:02:00.000, start: 0.000000\n");
  encoder.consume("total_size=90000000\nout_time_us=30000000\nprogress=continue\n");
  check(encoder.duration_us()==120000000);
  check(encoder.position_us()==30000000);
  p.begin("convert known.mp4",100000);
  p.set_total(encoder.duration_us(),100010);p.update(encoder.position_us(),130000);s=p.snapshot(130000);
  check(s.percent&&*s.percent==25&&s.completed==30000000&&s.total==120000000);
  check(s.remaining_ms&&*s.remaining_ms==90000);

  // Split writes and malformed/backward reports are normal when tailing a
  // growing child log and must never make progress optimistic or regress it.
  encoder.consume("out_time_us=6000");encoder.consume("0000\nprogress=continue\n");
  check(encoder.position_us()==60000000);
  encoder.consume("out_time_us=not-a-number\nout_time_us=59000000\n");
  check(encoder.position_us()==60000000);
  p.update(encoder.position_us(),160000);check(*p.snapshot(160000).percent==50);
  p.update(59000000,170000);check(*p.snapshot(170000).percent==50);

  // Retrying the same named stage is a new visible run with an honest reset.
  auto first_run=p.snapshot(170000).run;
  p.begin("convert known.mp4",180000,120000000);s=p.snapshot(180000);
  check(s.run==first_run+1&&s.completed==0&&*s.percent==0);
  ow::ProgressLogPolicy retry_policy;auto retry=retry_policy.line("retry001","known.mp4",s);
  check(retry&&retry->find("run "+std::to_string(first_run+1))!=std::string::npos&&retry->find("0%")!=std::string::npos);

  check(ow::format_duration(0)=="0s");
  check(ow::format_duration(44000)=="44s");
  check(ow::format_duration(72000)=="1m 12s");
  check(ow::format_duration(2*60*60*1000+5*60*1000)=="2h 05m");
  check(ow::format_utc_timestamp(0)=="1970-01-01 00:00:00.000Z");

  // An unmeasurable stage is described once and then remains quiet.
  ow::ProgressLogPolicy policy;
  p.begin("capability probe",200000);s=p.snapshot(200000);
  auto unknown=policy.line("abcdef12","approved converter",s);
  check(unknown&&unknown->find("cannot be measured")!=std::string::npos);
  check(!policy.line("abcdef12","approved converter",p.snapshot(240000)));
  policy.forget("abcdef12");check(policy.line("abcdef12","approved converter",p.snapshot(240000)).has_value());

  // Fast stages never enter the live dashboard. Cancellation always removes a
  // previously visible bar, including at 0%, 99%, and between named substeps.
  ow::LiveConsoleView view;
  check(!view.consume("2026-10-17 12:00:00.000Z JOB a1b2c3d4 | STARTED | tiny.jpg | convert started"));
  check(view.rows().empty());
  for(auto percent:{0u,99u}){
   auto line="2026-10-17 12:00:01.000Z JOB cancel"+std::to_string(percent)+" | PROGRESS | clip.mp4 | convert | run 1 | "+std::to_string(percent)+"% | elapsed 1m 00s | remaining 1s";
   check(view.consume(line));check(view.rows().size()==1);
   check(!view.consume("2026-10-17 12:00:02.000Z JOB cancel"+std::to_string(percent)+" | CANCELLED | clip.mp4 | operator requested cancellation"));
   check(view.rows().empty());
  }
  check(view.consume("2026-10-17 12:00:02.000Z JOB between1 | PROGRESS | clip.mp4 | convert | run 1 | 50% | elapsed 2s | remaining 2s"));
  check(!view.consume("2026-10-17 12:00:02.100Z JOB between1 | STAGE | clip.mp4 | verify SHA-256 started"));
  check(view.rows().empty());
  check(!view.consume("2026-10-17 12:00:02.200Z JOB between1 | CANCELLED | clip.mp4 | cancelled between sub-steps"));

  // Four jobs plus system activity remain one stable row each. Redirected and
  // NO_COLOR modes are plain pass-through with no ANSI or cursor movement.
  for(unsigned i=0;i<4;++i){
   auto line="2026-10-17 12:00:03.000Z JOB job0000"+std::to_string(i)+" | PROGRESS | item"+std::to_string(i)+".mp4 | convert | run 1 | 5% | elapsed 2s | remaining 38s";
   check(view.consume(line));
  }
  check(view.consume("2026-10-17 12:00:03.000Z ACTIVITY | active jobs 4 | durable results awaiting ACK 1"));
  auto frame=view.render({true,true,false});
  check(view.rows().size()==5);
  check(std::count(frame.begin(),frame.end(),'\n')==5);
  check(frame.find("[##")!=std::string::npos);
  check(std::all_of(frame.begin(),frame.end(),[](unsigned char value){return value<128;}));
  auto redirected=view.render({false,true,true});
  check(redirected.find('\x1b')==std::string::npos&&redirected.find('\r')==std::string::npos);
  auto no_color=view.render({true,false,false});
  check(no_color.find('\x1b')==std::string::npos&&no_color.find('\r')==std::string::npos);
  check(view.consume("2026-10-17 12:00:04.000Z ACTIVITY | idle"));
  check(view.rows().size()==4);

  // A multi-hour ETA stays human-readable; a completed millisecond job has no
  // invented intermediate bar and the durable terminal event remains English.
  p.begin("long conversion",300000,1000);p.update(1,307200);s=p.snapshot(307200);
  check(s.remaining_ms&&ow::format_duration(*s.remaining_ms)=="1h 59m");
  check(!view.consume("2026-10-17 12:00:04.000Z JOB fast0001 | SUCCEEDED | photo.jpg | completed in 8ms; result awaits acknowledgment"));
  p.begin("concurrent byte accounting",90000,400);
  std::vector<std::thread> writers;
  for(unsigned n=0;n<4;++n)writers.emplace_back([&,n]{for(unsigned i=1;i<=100;++i)p.update(n*100+i,90001+i);});
  for(auto& t:writers){t.join();}
  check(p.snapshot(92000).completed==400);
  std::cout<<"Progress and console contracts passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
