#include "test.hpp"
#include "ow/admission.hpp"
#include <thread>
#include <atomic>
#include <limits>
using namespace ow;
namespace {
constexpr Resources budget{4000,2000,10000,6};
Offer task(std::string id="a",Resources need={1000,0,100,2}){
 return {"job-"+id,id,"conversion.video.h264","fingerprint-"+id,need,1000};
}
void ready(Admission& a){a.capability("conversion.video.h264",true,Preference::preferred);}
}
TEST("verified capabilities accept independent jobs concurrently") {
 Admission a(budget,8);ready(a);
 CHECK(a.offer(task("a"),0,budget).accepted);CHECK(a.offer(task("b"),0,budget).accepted);
 CHECK(a.begin("a","token-a",5000,100)==Start::started);
 CHECK(a.begin("b","token-b",5000,100)==Start::started);CHECK(a.active().size()==2);
}
TEST("missing capability fails closed"){Admission a(budget,8);CHECK(!a.offer(task(),0,budget).accepted);}
TEST("installed but unverified does not mean capable"){Admission a(budget,8);a.capability("conversion.video.h264",false,Preference::preferred);CHECK(!a.offer(task(),0,budget).accepted);}
TEST("local disabled vetoes a preferred server offer"){Admission a(budget,8);a.capability("conversion.video.h264",true,Preference::disabled);CHECK(!a.offer(task(),0,budget).accepted);}
TEST("reservation consumes capacity before execution"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task("a",{3000,0,100,4}),0,budget).accepted);
 CHECK(!a.offer(task("b",{2000,0,100,3}),0,budget).accepted);CHECK(a.used().ram_mb==3000);
}
TEST("dynamic low memory rejects despite configured budget"){
 Admission a(budget,8);ready(a);CHECK(!a.offer(task(),0,{500,2000,10000,6}).accepted);
}
TEST("duplicate offer is idempotent"){
 Admission a(budget,8);ready(a);auto o=task();CHECK(a.offer(o,0,budget).accepted);
 CHECK(a.offer(o,1,budget).accepted);CHECK(a.active().size()==1);CHECK(a.used()==o.resources);
}
TEST("changed duplicate payload is rejected"){
 Admission a(budget,8);ready(a);auto o=task();CHECK(a.offer(o,0,budget).accepted);
 o.fingerprint="changed";CHECK(!a.offer(o,1,budget).accepted);
}
TEST("same content job cannot run in two attempts on one host"){
 Admission a(budget,8);ready(a);auto o=task();CHECK(a.offer(o,0,budget).accepted);
 o.attempt_id="new-attempt";CHECK(!a.offer(o,1,budget).accepted);
}
TEST("job guardrail is independent of resource budget"){
 Admission a(budget,1);ready(a);CHECK(a.offer(task("a"),0,budget).accepted);CHECK(!a.offer(task("b"),0,budget).accepted);
}
TEST("oversized resource numbers cannot overflow accounting"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);
 CHECK(!a.offer(task("huge",{std::numeric_limits<std::uint64_t>::max(),0,1,1}),0,budget).accepted);
 CHECK(a.used().ram_mb==1000);
}
TEST("zero CPU or memory declaration is rejected"){
 Admission a(budget,8);ready(a);CHECK(!a.offer(task("a",{0,0,1,1}),0,budget).accepted);CHECK(!a.offer(task("b",{1,0,1,0}),0,budget).accepted);
}
TEST("invalid IDs cannot become filesystem paths"){
 CHECK(!safe_id("../escape"));CHECK(!safe_id("a\\b"));CHECK(!safe_id("."));CHECK(!safe_id(""));CHECK(!safe_id(std::string(129,'a')));CHECK(safe_id("7e11_a-b"));
}
TEST("expired reservation cannot be started"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",3000,1000)==Start::declined);
}
TEST("start without lease is rejected"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","",3000,1)==Start::declined);
}
TEST("replayed begin does not start a second process"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);
 CHECK(a.begin("a","token",3000,1)==Start::started);CHECK(a.begin("a","token",3000,2)==Start::already_running);
 CHECK(a.begin("a","other-token",3000,3)==Start::declined);
}
TEST("pause refuses new work but preserves active execution"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",5000,1)==Start::started);
 a.pause(true);CHECK(!a.offer(task("b"),2,budget).accepted);CHECK(a.active()[0].phase==Phase::running);
 a.pause(false);CHECK(a.offer(task("b"),3,budget).accepted);
}
TEST("pause prevents an unstarted reservation from beginning"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);a.pause(true);CHECK(a.begin("a","token",3000,1)==Start::declined);
}
TEST("only a valid fresh renewal extends the lease"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",2000,1)==Start::started);
 CHECK(!a.renew("a","wrong",1,6000,100));CHECK(a.renew("a","token",1,4000,100));
 CHECK(!a.renew("a","token",1,8000,200));CHECK(!a.renew("a","token",0,8000,200));
 CHECK(a.active()[0].deadline==4000);
}
TEST("expired running lease cannot be revived by late renewal"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",2000,1)==Start::started);
 CHECK(!a.renew("a","token",1,5000,2000));
}
TEST("expiry returns unused reservations to the budget"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);auto e=a.expire(1000);
 CHECK(e.size()==1);CHECK(!e[0].stop_process);CHECK(a.used()==Resources{});
}
TEST("expired running job retains resources until child shutdown"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",2000,1)==Start::started);
 auto e=a.expire(2000);CHECK(e.size()==1);CHECK(e[0].stop_process);CHECK(a.used().ram_mb==1000);
 CHECK(a.expire(2001).empty());CHECK(a.finish("a"));CHECK(a.used()==Resources{});
}
TEST("cancelling one job preserves the other"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task("a"),0,budget).accepted);CHECK(a.offer(task("b"),0,budget).accepted);
 CHECK(a.begin("a","ta",3000,1)==Start::started);CHECK(a.begin("b","tb",3000,1)==Start::started);
 CHECK(a.cancel("a"));CHECK(a.used().ram_mb==2000);CHECK(a.finish("a"));CHECK(a.active().size()==1);CHECK(a.active()[0].offer.attempt_id=="b");
}
TEST("duplicate finish cannot release resources twice"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","token",3000,1)==Start::started);
 CHECK(a.finish("a"));CHECK(!a.finish("a"));CHECK(a.used()==Resources{});
}
TEST("simultaneous offers never oversubscribe"){
 Admission a({2000,0,1000,2},8);ready(a);std::atomic_int accepted=0;
 std::vector<std::jthread> threads;for(int i=0;i<20;++i)threads.emplace_back([&,i]{if(a.offer(task(std::to_string(i),{1000,0,1,1}),0,budget).accepted)++accepted;});
 threads.clear();CHECK(accepted==2);CHECK(a.used().ram_mb==2000);
}
TEST("two outstanding offers cannot both reserve the same live free memory"){
 Admission a(budget,8);ready(a);Resources live{1500,0,1000,6};
 CHECK(a.offer(task("a"),0,live).accepted);
 CHECK(!a.offer(task("b"),0,live).accepted);
}
TEST("disabling a capability prevents a reserved job from starting"){
 Admission a(budget,8);ready(a);CHECK(a.offer(task(),0,budget).accepted);
 a.capability("conversion.video.h264",true,Preference::disabled);
 CHECK(a.begin("a","token",3000,1)==Start::declined);
}
TEST("a duplicate offer cannot extend a reservation indefinitely"){
 Admission a(budget,8);ready(a);auto o=task();CHECK(a.offer(o,0,budget).accepted);
 o.reserve_until=5000;CHECK(a.offer(o,900,budget).accepted);
 CHECK(a.begin("a","token",6000,1000)==Start::declined);
}
