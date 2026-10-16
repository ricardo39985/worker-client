#include "test.hpp"
#include "ow/rendition_pipeline.hpp"
#include <atomic>
#include <chrono>
#include <future>
using namespace std::chrono_literals;
namespace {
void signalled(std::shared_future<void> signal) {
 if(signal.wait_for(2s)!=std::future_status::ready)throw std::runtime_error("pipeline failed to overlap encode and transfer");
 signal.get();
}
}
TEST("rendition pipeline returns ordered metadata for every artifact") {
 const std::vector<int> items{0,1,2};std::vector<int> prepared;
 auto results=ow::run_rendition_pipeline(items,[&](int i,std::stop_token){prepared.push_back(i);},
  [](int i,std::stop_token){return std::to_string(i);});
 CHECK(prepared==items);CHECK(results==std::vector<std::string>({"0","1","2"}));
}
TEST("next encode overlaps previous transfer") {
 std::promise<void> transfer_started,next_prepared;
 auto started=transfer_started.get_future().share(),prepared=next_prepared.get_future().share();
 auto results=ow::run_rendition_pipeline(std::vector<int>{0,1},[&](int i,std::stop_token){
  if(i==1){signalled(started);next_prepared.set_value();}
 },[&](int i,std::stop_token){if(i==0){transfer_started.set_value();signalled(prepared);}return i;});
 CHECK(results==std::vector<int>({0,1}));
}
TEST("empty rendition pipeline starts no stages") {
 auto unexpected=[](int,std::stop_token)->int{throw std::runtime_error("unexpected work");};
 CHECK(ow::run_rendition_pipeline(std::vector<int>{},unexpected,unexpected).empty());
}
TEST("transfer backpressure keeps one encoder and one transfer at a time") {
 std::atomic_int encoded{-1},delivered{-1},encoding{},transferring{};
 std::vector<int> items;for(int i=0;i<20;++i)items.push_back(i);
 auto results=ow::run_rendition_pipeline(items,[&](int i,std::stop_token){
  CHECK(encoding.fetch_add(1)==0);CHECK(i<=delivered.load()+2);
  encoded=i;CHECK(encoding.fetch_sub(1)==1);
 },[&](int i,std::stop_token){
  CHECK(transferring.fetch_add(1)==0);CHECK(encoding.load()<=1);
  CHECK(encoded.load()>=i);CHECK(delivered.load()==i-1);
  delivered=i;CHECK(transferring.fetch_sub(1)==1);return i;
 });
 CHECK(results==items);CHECK(encoding==0);CHECK(transferring==0);
}
TEST("final transfer must join before result is returned") {
 std::promise<void> transfer_started,release;
 auto started=transfer_started.get_future().share(),gate=release.get_future().share();
 auto run=std::async(std::launch::async,[&]{
  return ow::run_rendition_pipeline(std::vector<int>{7},[](int,std::stop_token){},
   [&](int i,std::stop_token){transfer_started.set_value();signalled(gate);return i;});
 });
 signalled(started);auto status=run.wait_for(0ms);release.set_value();
 CHECK(status!=std::future_status::ready);CHECK(run.get()==std::vector<int>({7}));
}
TEST("transfer failure cancels next encode and preserves original cause") {
 std::promise<void> encoding_started;
 auto encoding=encoding_started.get_future().share();std::atomic_bool encode_stopped{};
 std::string error;
 try{ow::run_rendition_pipeline(std::vector<int>{0,1},[&](int i,std::stop_token stop){
  if(i==1){encoding_started.set_value();auto until=std::chrono::steady_clock::now()+2s;
   while(!stop.stop_requested()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
   encode_stopped=stop.stop_requested();throw std::runtime_error("secondary encode cancellation");}
 },[&](int i,std::stop_token){if(i==0){signalled(encoding);throw std::runtime_error("upload rejected");}return i;});
 }catch(const std::exception& e){error=e.what();}
 CHECK(error=="upload rejected");CHECK(encode_stopped);
}
TEST("encode failure cancels and joins transfer before unwinding") {
 std::promise<void> transfer_started;auto started=transfer_started.get_future().share();
 std::atomic_bool transfer_stopped{};std::string error;
 try{ow::run_rendition_pipeline(std::vector<int>{0,1},[&](int i,std::stop_token){
  if(i==1){signalled(started);throw std::runtime_error("encoder failed");}
 },[&](int,std::stop_token stop){transfer_started.set_value();auto until=std::chrono::steady_clock::now()+2s;
  while(!stop.stop_requested()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
  transfer_stopped=stop.stop_requested();throw std::runtime_error("secondary upload cancellation");return 0;
 });}catch(const std::exception& e){error=e.what();}
 CHECK(error=="encoder failed");CHECK(transfer_stopped);
}
TEST("failed final checksum cannot return a successful partial result") {
 std::atomic_int completed{};std::string error;
 try{auto result=ow::run_rendition_pipeline(std::vector<int>{0,1,2},[](int,std::stop_token){},
  [&](int i,std::stop_token){if(i==2)throw std::runtime_error("checksum failed");++completed;return i;});
  (void)result;
 }catch(const std::exception& e){error=e.what();}
 CHECK(error=="checksum failed");CHECK(completed==2);
}
TEST("independent pipelines do not share cancellation or metadata") {
 auto good=std::async(std::launch::async,[]{return ow::run_rendition_pipeline(std::vector<int>{4,5},
  [](int,std::stop_token stop){CHECK(!stop.stop_requested());},[](int i,std::stop_token stop){CHECK(!stop.stop_requested());return i;});});
 THROWS(ow::run_rendition_pipeline(std::vector<int>{0},[](int,std::stop_token){throw std::runtime_error("cancelled job");},[](int i,std::stop_token){return i;}));
 CHECK(good.get()==std::vector<int>({4,5}));
}
