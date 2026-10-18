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
TEST("pressure defers reserved BEGIN as well as new offers without cancelling running work") {
 Admission a(budget,8);ready(a);CHECK(a.offer(task("a"),0,budget).accepted);CHECK(a.offer(task("b"),0,budget).accepted);
 CHECK(a.begin("a","live",5000,1)==Start::started);const auto before=a.used();
 a.defer_new_starts("foreground_activity");CHECK(!a.offer(task("new"),2,budget).accepted);
 CHECK(a.begin("b","not-started",5000,2)==Start::declined);CHECK(a.used()==before);
 CHECK(a.begin("a","live",5000,2)==Start::already_running);CHECK(a.renew("a","live",1,6000,2));
 CHECK(a.offer(task("a"),2,budget).accepted);CHECK(a.finish("a"));
 a.defer_new_starts("");CHECK(a.begin("b","not-started",5000,3)==Start::started);
}
TEST("verified capabilities accept independent jobs concurrently") {
 Admission a(budget,8);ready(a);
 CHECK(a.offer(task("a"),0,budget).accepted);CHECK(a.offer(task("b"),0,budget).accepted);
 CHECK(a.begin("a","token-a",5000,100)==Start::started);
 CHECK(a.begin("b","token-b",5000,100)==Start::started);CHECK(a.active().size()==2);
}
TEST("four single-CPU app jobs reserve and begin independently within configured limits") {
 const Resources capacity{2667,0,8192,4};Admission admission(capacity,4);
 admission.capability("media.video.renditions.v1",true,Preference::allowed);
 for(int i=0;i<4;++i){auto offer=task(std::to_string(i),{512,0,1024,1});offer.capability="media.video.renditions.v1";
  CHECK(admission.offer(offer,0,capacity).accepted);
  CHECK(admission.begin(offer.attempt_id,"token-"+offer.attempt_id,5000,1)==Start::started);}
 auto fifth=task("fifth",{512,0,1024,1});fifth.capability="media.video.renditions.v1";
 CHECK(!admission.offer(fifth,0,capacity).accepted);CHECK(admission.active().size()==4);
 CHECK(admission.used()==Resources{2048,0,4096,4});
 CHECK(admission.cancel("0"));CHECK(!admission.offer(fifth,0,capacity).accepted);
 CHECK(admission.finish("0"));CHECK(admission.offer(fifth,0,capacity).accepted);
}
TEST("drained input resources hand off to atomic concurrent stage claims") {
 Admission a({2048,0,1000,3},4);ready(a);CHECK(a.offer(task("a",{512,0,200,1}),0,budget).accepted);
 CHECK(a.begin("a","token",5000,1)==Start::started);CHECK(a.release_compute("a"));
 CHECK(a.used()==Resources{0,0,200,0});
 CHECK(a.stage("a","feed",{768,0,0,1},2,budget).accepted);
 CHECK(a.stage("a","optimized",{768,0,0,1},2,budget).accepted);
 CHECK(a.stage("a","poster",{512,0,0,1},2,budget).accepted);
 CHECK(a.used()==Resources{2048,0,200,3});
 auto denied=a.stage("a","extra",{512,0,0,1},2,budget);CHECK(!denied.accepted);CHECK(!denied.reason.empty());
 CHECK(!a.finish("a"));CHECK(a.finish_stage("a","feed"));
 CHECK(a.used()==Resources{1280,0,200,2});
 CHECK(a.finish_stage("a","optimized"));CHECK(a.finish_stage("a","poster"));CHECK(a.finish("a"));CHECK(a.used()==Resources{});
}
TEST("cancellation keeps stage claims until contained children drain") {
 Admission a(budget,4);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","t",5000,1)==Start::started);
 CHECK(a.release_compute("a"));CHECK(a.stage("a","feed",{1000,0,0,2},2,budget).accepted);
 CHECK(a.cancel("a"));CHECK(a.used().ram_mb==1000);CHECK(!a.finish("a"));
 CHECK(!a.stage("a","poster",{256,0,0,1},3,budget).accepted);
 CHECK(a.finish_stage("a","feed"));CHECK(a.finish("a"));CHECK(a.used()==Resources{});
}
TEST("stage retries cannot double charge or silently change a resource promise") {
 Admission a(budget,4);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","t",5000,1)==Start::started);
 CHECK(a.release_compute("a"));CHECK(!a.release_compute("a"));
 CHECK(a.stage("a","feed",{1000,0,0,2},2,budget).accepted);CHECK(a.stage("a","feed",{1000,0,0,2},3,budget).accepted);
 CHECK(!a.stage("a","feed",{2000,0,0,2},3,budget).accepted);CHECK(a.used().ram_mb==1000);
 CHECK(a.finish_stage("a","feed"));CHECK(!a.finish_stage("a","feed"));CHECK(a.used().ram_mb==0);
}
TEST("stage costs compete atomically across independent jobs without altering offer identity") {
 Admission a({1024,0,1000,2},4);ready(a);
 for(const auto id:{"a","b"}){CHECK(a.offer(task(id,{512,0,100,1}),0,budget).accepted);CHECK(a.begin(id,"t",5000,1)==Start::started);CHECK(a.release_compute(id));}
 std::atomic_int accepted{};std::thread x([&]{if(a.stage("a","feed",{768,0,0,1},2,budget).accepted)++accepted;}),y([&]{if(a.stage("b","feed",{768,0,0,1},2,budget).accepted)++accepted;});x.join();y.join();
 CHECK(accepted==1);CHECK(a.used().ram_mb==768);
 for(const auto& active:a.active())CHECK(active.offer.resources==Resources{512,0,100,1});
}
TEST("live stage RAM headroom is not charged twice against already allocated children") {
 Admission a(budget,4);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","t",5000,1)==Start::started);CHECK(a.release_compute("a"));
 CHECK(a.stage("a","feed",{1000,0,0,1},2,budget).accepted);
 CHECK(a.stage("a","poster",{400,0,0,1},3,{500,0,10000,6}).accepted);
 CHECK(a.used().ram_mb==1400);CHECK(!a.stage("a","extra",{600,0,0,1},3,{500,0,10000,6}).accepted);
}
TEST("freed source and uploaded output scratch can be reclaimed before the remaining job completes") {
 Admission a(budget,4);ready(a);CHECK(a.offer(task(),0,budget).accepted);CHECK(a.begin("a","t",5000,1)==Start::started);
 CHECK(a.release_scratch("a",30));CHECK(a.used().scratch_mb==70);CHECK(!a.release_scratch("a",71));
 CHECK(a.active()[0].offer.resources.scratch_mb==100);CHECK(a.finish("a"));CHECK(a.used()==Resources{});
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

TEST("a recovery probe cannot overlap reserved running or cancelling work"){
 Admission a(budget,8);ready(a);CHECK(a.begin_probe());
 CHECK(!a.offer(task(),0,budget).accepted);a.pause(false);
 CHECK(!a.offer(task(),0,budget).accepted);CHECK(!a.begin_probe());a.end_probe();
 CHECK(a.offer(task(),0,budget).accepted);CHECK(!a.begin_probe());
 CHECK(a.begin("a","token",3000,1)==Start::started);CHECK(!a.begin_probe());
 CHECK(a.cancel("a"));CHECK(!a.begin_probe());CHECK(a.used().ram_mb==1000);
 CHECK(a.finish("a"));CHECK(a.begin_probe());a.pause(true);a.end_probe();
 CHECK(!a.offer(task("b"),0,budget).accepted);
}
TEST("concurrent probe and offer admission have exactly one owner"){
 for(int n=0;n<100;++n){Admission a(budget,8);ready(a);bool probe=false,offer=false;
  std::jthread x([&]{probe=a.begin_probe();});std::jthread y([&]{offer=a.offer(task(),0,budget).accepted;});
  x.join();y.join();CHECK(probe!=offer);
 }
}

TEST("local child RAM topups are atomic preserve offer identity and last through cancellation"){
 const Resources cap{2048,0,1000,4};Admission a(cap,4);ready(a);
 auto first=task("gpu",{512,0,100,1}),second=task("other",{512,0,100,1});
 CHECK(a.offer(first,0,cap).accepted);CHECK(a.offer(second,0,cap).accepted);
 CHECK(!a.reserve_ram("gpu",768,cap,1));
 CHECK(a.begin("gpu","token",5000,1)==Start::started);
 CHECK(!a.reserve_ram("gpu",1792,cap,2));CHECK(a.used().ram_mb==1024);
 CHECK(!a.reserve_ram("gpu",768,Resources{1024,0,1000,4},2));
 CHECK(a.reserve_ram("gpu",768,cap,2));CHECK(a.used().ram_mb==1280);
 CHECK(a.offer(first,2,cap).accepted);CHECK(a.used().ram_mb==1280);
 CHECK(a.reserve_ram("gpu",768,cap,2));CHECK(a.used().ram_mb==1280);
 CHECK(a.cancel("gpu"));CHECK(a.used().ram_mb==1280);
 CHECK(!a.reserve_ram("gpu",1024,cap,3));
 CHECK(a.finish("gpu"));CHECK(a.used().ram_mb==512);CHECK(a.finish("other"));CHECK(a.used().ram_mb==0);
}
TEST("competing local RAM increases cannot oversubscribe host capacity"){
 const Resources cap{1536,0,1000,4};Admission a(cap,4);ready(a);
 CHECK(a.offer(task("a",{512,0,100,1}),0,cap).accepted);
 CHECK(a.offer(task("b",{512,0,100,1}),0,cap).accepted);
 CHECK(a.begin("a","token",5000,1)==Start::started);CHECK(a.begin("b","token",5000,1)==Start::started);
 std::atomic_uint accepted{};
 std::thread x([&]{if(a.reserve_ram("a",1024,cap,2))++accepted;});
 std::thread y([&]{if(a.reserve_ram("b",1024,cap,2))++accepted;});x.join();y.join();
 CHECK(accepted==1);CHECK(a.used().ram_mb==1536);
 a.expire(6000);CHECK(a.used().ram_mb==1536);
 CHECK(!a.reserve_ram("a",1100,cap,6001));CHECK(a.finish("a"));CHECK(a.finish("b"));CHECK(a.used().ram_mb==0);
}
