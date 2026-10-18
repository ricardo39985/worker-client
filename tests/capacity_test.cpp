#include "test.hpp"
#include "ow/capacity_control.hpp"
using namespace ow;
TEST("capacity grows only after a sustained measured window with validated completions") {
 CapacityControl c(8,10000);Pressure p{10,10,false};
 c.observe(p,0);CHECK(c.limit()==1);c.observe(p,9999);CHECK(c.limit()==1);
 c.activity(1);c.validated();c.validated();c.observe(p,10000);CHECK(c.limit()==2);CHECK(c.qualified()==1);
 c.observe(p,20000);CHECK(c.limit()==2); // no completed work at this trial
 c.activity(1);c.validated();c.validated();c.observe(p,20001);CHECK(c.limit()==2);CHECK(c.qualified()==1); // the two-stage trial was not exercised
 c.activity(2);c.validated();c.validated();c.observe(p,30001);CHECK(c.limit()>2&&c.limit()<=8);CHECK(c.qualified()==2);
}
TEST("binding CPU or disk pressure backs off without releasing running reservations") {
 CapacityControl c(8,10);c.observe({10,10,false},0);c.activity(1);c.validated();c.validated();c.observe({10,10,false},10);CHECK(c.limit()==2);
 c.observe({95,10,false},11);CHECK(c.limit()==1);CHECK(!c.admit(1,11).accepted);
 CHECK(c.admit(0,11).accepted);CHECK(!c.admit_offer(11).accepted);
 CHECK(c.reason()=="cpu_pressure");
}
TEST("foreground use defers new jobs while running work can drain at conservative concurrency") {
 CapacityControl c(8,10);c.observe({10,10,true},0);
 CHECK(!c.admit_offer(0).accepted);CHECK(c.reason()=="foreground_activity");
 CHECK(c.admit(0,0).accepted);CHECK(!c.admit(1,0).accepted);
}
TEST("missing stale and malformed telemetry cannot manufacture measured capacity") {
 CapacityControl c(8,10);CHECK(c.limit()==1);CHECK(!c.admit_offer(0).accepted);
 c.observe({10,10,false},0);CHECK(!c.admit_offer(6000).accepted);
 c.observe({101,10,false},6001);CHECK(c.limit()==1);CHECK(c.qualified()==0);
 CHECK(c.reason()=="telemetry_unavailable");
}
TEST("capacity ceiling remains binding even on an otherwise idle host") {
 CapacityControl c(2,10);c.observe({10,10,false},0);
 for(int i=1;i<=10;++i){c.activity(c.limit());c.validated();c.validated();c.observe({10,10,false},i*10);}
 CHECK(c.limit()==2);CHECK(c.qualified()==2);CHECK(!c.admit(2,101).accepted);
}
