#include "test.hpp"
#include "ow/adaptive_hardware.hpp"
using namespace ow::adaptive;
namespace {
std::vector<Profile> profiles(){
 Profile cpu;cpu.stable_id="cpu";cpu.backend=Backend::cpu;cpu.codec=Codec::h264;
 cpu.cpu_threads=1;cpu.ram_mb=512;cpu.scratch_mb=128;cpu.max_concurrency=1;cpu.measured_frames_per_second=10;
 auto gpu=cpu;gpu.stable_id="gpu";gpu.backend=Backend::nvidia_nvenc;gpu.backend_identity_verified=true;
 gpu.vram_mb=256;gpu.max_concurrency=2;gpu.measured_frames_per_second=50;
 return {cpu,gpu};
}
}
TEST("active CPU fallback must block a new primary CPU encode at concurrency one"){
 auto p=profiles();p[1].max_concurrency=1;
 Scheduler scheduler({3,2048,1024,1024,3},p);
 auto first=scheduler.reserve({Codec::h264});CHECK(first);
 CHECK(first.acquire_cpu_fallback([]{return false;},0));
 auto second=scheduler.reserve({Codec::h264});
 CHECK(!second); // GPU is full, CPU slot is already executing the fallback.
}
TEST("releasing a non-owner must not clear another job's CPU fallback slot"){
 Scheduler scheduler({4,4096,2048,2048,4},profiles());
 auto owner=scheduler.reserve({Codec::h264}),waiter=scheduler.reserve({Codec::h264});CHECK(owner&&waiter);
 CHECK(owner.acquire_cpu_fallback([]{return false;},0));
 CHECK(!waiter.acquire_cpu_fallback([]{return false;},0));
 waiter.release();
 auto third=scheduler.reserve({Codec::h264});CHECK(third);
 CHECK(!third.acquire_cpu_fallback([]{return false;},0));
}
