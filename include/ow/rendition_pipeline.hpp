#pragma once
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <vector>
namespace ow {
// Bounded preparation workers and one streaming delivery lane. Preparation
// callbacks must acquire their own atomic resource/encoder leases. A window
// bounds preparing + ready artifacts; no unbounded future-per-item backlog.
// Metadata retains role order even when a poster overtakes a slow rendition.
// Every callback joins (including cancellation/drain) before return or throw.
template<class Range,class Prepare,class Deliver>
auto run_rendition_pipeline(const Range& items,Prepare prepare,Deliver deliver,std::size_t parallelism=1) {
 using Item=typename Range::value_type;
 using Result=std::invoke_result_t<Deliver,const Item&,std::stop_token>;
 if(!parallelism||parallelism>64)throw std::invalid_argument("Invalid preparation concurrency");
 std::vector<const Item*> work;for(const auto& item:items)work.push_back(&item);
 std::vector<std::optional<Result>> completed(work.size());
 std::vector<Result> results;results.reserve(work.size());
 if(work.empty())return results;
 parallelism=std::min(parallelism,work.size());
 std::stop_source abort;std::mutex mutex;std::condition_variable changed;
 std::exception_ptr first_error;std::deque<std::size_t> ready;
 std::size_t next{},in_flight{},live{};
 auto fail=[&](std::exception_ptr error){
  {std::lock_guard lock(mutex);if(!first_error)first_error=error;}
  abort.request_stop();changed.notify_all();
 };
 std::vector<std::future<void>> workers;workers.reserve(parallelism);
 try{
  for(std::size_t n=0;n<parallelism;++n){
   {std::lock_guard lock(mutex);++live;}
   try{workers.push_back(std::async(std::launch::async,[&]{
    try{for(;;){
     std::size_t index;
     {std::unique_lock lock(mutex);changed.wait(lock,[&]{return abort.stop_requested()||next==work.size()||in_flight<parallelism;});
      if(abort.stop_requested()||next==work.size())break;
      index=next++;++in_flight;}
     prepare(*work[index],abort.get_token());
     {std::lock_guard lock(mutex);if(abort.stop_requested())break;ready.push_back(index);}
     changed.notify_all();
    }}catch(...){fail(std::current_exception());}
    {std::lock_guard lock(mutex);--live;}changed.notify_all();
   }));}catch(...){std::lock_guard lock(mutex);--live;throw;}
  }
  for(;;){
   std::size_t index;
   {std::unique_lock lock(mutex);changed.wait(lock,[&]{return abort.stop_requested()||!ready.empty()||live==0;});
    if(abort.stop_requested()||ready.empty())break;
    index=ready.front();ready.pop_front();--in_flight;}
   changed.notify_all();
   completed[index].emplace(deliver(*work[index],abort.get_token()));
  }
 }catch(...){fail(std::current_exception());}
 for(auto& worker:workers)try{worker.get();}catch(...){fail(std::current_exception());}
 if(first_error)std::rethrow_exception(first_error);
 for(auto& value:completed){if(!value)throw std::runtime_error("Incomplete rendition pipeline");results.push_back(std::move(*value));}
 return results;
}
}
