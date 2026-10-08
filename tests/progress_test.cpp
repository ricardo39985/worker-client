#include "ow/progress.hpp"
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
  p.begin("concurrent byte accounting",90000,400);
  std::vector<std::thread> writers;
  for(unsigned n=0;n<4;++n)writers.emplace_back([&,n]{for(unsigned i=1;i<=100;++i)p.update(n*100+i,90001+i);});
  for(auto& t:writers){t.join();}
  check(p.snapshot(92000).completed==400);
  std::cout<<"Progress and activity contracts passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
