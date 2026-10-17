#include "test.hpp"
#include "ow/adaptive_hardware.hpp"
using namespace ow::adaptive;
TEST("exact native fixture rejects a second reservation before child creation") {
 Profile gpu{"synthetic-luid","driver","hash",Backend::nvidia_nvenc,Codec::h264,1,256,512,128,1,50,0,0,true};
 Profile cpu{"cpu","","hash",Backend::cpu,Codec::h264,1,256,0,128,1,20,0,0,true};
 Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
 auto held=scheduler.reserve({Codec::h264});CHECK(held);CHECK(scheduler.active_jobs()==1);
 CHECK(!scheduler.reserve({Codec::h264}));
}
TEST("exact native fixture retains GPU capacity while admitting an independent CPU job") {
 Profile gpu{"synthetic-luid","driver","hash",Backend::nvidia_nvenc,Codec::h264,1,256,512,128,1,50,0,0,true};
 Profile cpu{"cpu","","hash",Backend::cpu,Codec::h264,1,256,0,128,1,20,0,0,true};
 Scheduler scheduler({2,1024,512,512,4},{gpu,cpu});
 auto held=scheduler.reserve({Codec::h264});CHECK(held);
 CHECK(held.profiles().front().backend==Backend::nvidia_nvenc);
 auto independent=scheduler.reserve({Codec::h264});CHECK(independent);
 CHECK(independent.profiles().front().backend==Backend::cpu);
 CHECK(scheduler.active_jobs()==2);CHECK(!scheduler.reserve({Codec::h264}));
 independent.release();CHECK(scheduler.active_jobs()==1);
 CHECK(held.profiles().front().backend==Backend::nvidia_nvenc);
 held.release();CHECK(scheduler.active_jobs()==0);
 auto next=scheduler.reserve({Codec::h264});CHECK(next);
 CHECK(next.profiles().front().backend==Backend::nvidia_nvenc);
}
