#pragma once
#include "worker.hpp"
namespace ow::win {
// Test/measurement port only. Not linked into tray entry points and never
// selected by configuration or a network message. Real media execution, hash,
// validation, budgets, cancellation/drain and journal code are unchanged.
class OfflineMediaHarness {
public:
 // Fake coordinator/media-transport boundaries; actual typed admission, children,
 // leases, durable outbox/ACK and owner IPC. No production state is accessed.
 static json::object control_acceptance(Config config,bool restart=false);
 static json::object run(Config config,const Probe& proof,const std::filesystem::path& source,
                         const std::map<adaptive::Codec,std::string>& pins,unsigned jobs,
                         unsigned concurrency,std::uint64_t minimum_ms,unsigned warmups=2);
};
}
