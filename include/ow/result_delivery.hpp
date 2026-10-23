#pragma once
#include "ow/admission.hpp"
namespace ow {
// New durable completions bypass the retry timer; retransmissions stay bounded.
class ResultDelivery {
 Tick retry_at_{};
public:
 bool due(Tick now,bool committed){
  if(!committed&&now<retry_at_)return false;
  retry_at_=now+2000;return true;
 }
};
}
