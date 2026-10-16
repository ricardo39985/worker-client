#pragma once
#include <exception>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <vector>
namespace ow {
// One prepare (native encode) and one delivery (streaming hash/upload) may overlap.
// At most one completed artifact waits for delivery. All work joins before return
// or throw, so callers may then publish results, delete files or release budgets.
template<class Range,class Prepare,class Deliver>
auto run_rendition_pipeline(const Range& items,Prepare prepare,Deliver deliver) {
 using Result=std::invoke_result_t<Deliver,const typename Range::value_type&,std::stop_token>;
 std::vector<Result> results;results.reserve(items.size());
 std::stop_source abort;std::mutex error_mutex;std::exception_ptr first_error;
 auto fail=[&](std::exception_ptr error){
  {std::lock_guard lock(error_mutex);if(!first_error)first_error=error;}
  abort.request_stop();
 };
 std::future<Result> pending;
 try {
  for(const auto& item:items){
   if(abort.stop_requested())throw std::runtime_error("Rendition pipeline aborted");
   prepare(item,abort.get_token());
   // Backpressure: never launch another delivery until the prior one has joined.
   if(pending.valid())results.push_back(pending.get());
   if(abort.stop_requested())throw std::runtime_error("Rendition pipeline aborted");
   pending=std::async(std::launch::async,[&,item]{
    try{return deliver(item,abort.get_token());}
    catch(...){fail(std::current_exception());throw;}
   });
  }
  if(pending.valid())results.push_back(pending.get());
 }catch(...){
  // Signal before joining: the other stage may be waiting on cancellation.
  fail(std::current_exception());
  if(pending.valid()){try{pending.get();}catch(...){}}
  std::rethrow_exception(first_error);
 }
 return results;
}
}
