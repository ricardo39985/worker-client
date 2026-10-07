#include "test.hpp"
#include "ow/store.hpp"
#include "ow/pairing.hpp"
#include <chrono>
using ow::Store;
struct Temp {
 std::filesystem::path path=std::filesystem::temp_directory_path()/("ow-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Temp(){std::filesystem::create_directory(path);}
 ~Temp(){std::error_code e;std::filesystem::remove_all(path,e);}
};
TEST("accepted attempt and pending result survive journal reopen"){
 Temp t;{Store s(t.path/"journal.db");CHECK(s.reserve("a","job-a","payload"));CHECK(s.start("a"));s.result("a","result");}
 Store s(t.path/"journal.db");auto p=s.pending();CHECK(p.size()==1);CHECK(p[0].body=="result");CHECK(!s.reserve("a","job-a","payload"));
 CHECK(s.acknowledge(p[0].sequence,"a"));CHECK(s.pending().empty());CHECK(s.state("a")=="reported");
}
TEST("outbox cannot be acknowledged with wrong attempt or sequence"){
 Store s(":memory:");CHECK(s.reserve("a","j","p"));s.result("a","r");auto p=s.pending()[0];
 CHECK(!s.acknowledge(p.sequence,"b"));CHECK(!s.acknowledge(p.sequence+1,"a"));CHECK(s.pending().size()==1);
}
TEST("repeated result does not create duplicate outbox entry"){
 Store s(":memory:");CHECK(s.reserve("a","j","p"));s.result("a","r");s.result("a","r");CHECK(s.pending().size()==1);THROWS(s.result("a","different"));CHECK(s.pending().size()==1);
}
TEST("attempt collision cannot change persisted payload"){
 Store s(":memory:");CHECK(s.reserve("a","j","p"));CHECK(s.reserve("a","j","p"));CHECK(!s.reserve("a","j","changed"));CHECK(!s.reserve("a","different-job","p"));
}
TEST("restart exposes interrupted work without automatically rerunning it"){
 Temp t;{Store s(t.path/"journal.db");CHECK(s.reserve("a","j","p"));CHECK(s.start("a"));}
 Store s(t.path/"journal.db");CHECK(s.interrupted()==std::vector<std::string>{"a"});s.result("a","abandoned");CHECK(s.interrupted().empty());CHECK(s.pending().size()==1);
}
TEST("priority settings survive restart"){
 Temp t;{Store s(t.path/"j.db");s.set("priority","low");}Store s(t.path/"j.db");CHECK(s.get("priority")=="low");CHECK(!s.get("absent"));
}
TEST("result without accepted job fails instead of making orphan output"){
 Store s(":memory:");THROWS(s.result("unknown","result"));CHECK(s.pending().empty());
}
TEST("pending counter follows durable results and acknowledgements"){
 Store s(":memory:");CHECK(s.pending_count()==0);
 CHECK(s.reserve("a","ja","p"));s.result("a","r");CHECK(s.pending_count()==1);
 auto row=s.pending()[0];CHECK(s.acknowledge(row.sequence,"a"));CHECK(s.pending_count()==0);
}

TEST("journal coordinator binding survives reopen and rejects a different server"){
 Temp t;{Store s(t.path/"j.db");ow::bind_journal_origin(s,"https://first.test");}
 Store reopened(t.path/"j.db");ow::bind_journal_origin(reopened,"https://FIRST.test:443/");
 THROWS(ow::bind_journal_origin(reopened,"https://second.test"));
}
TEST("legacy pending results cannot be adopted without endpoint provenance"){
 Store s(":memory:");CHECK(s.reserve("a","job-a","p"));s.result("a","private-result-token");
 THROWS(ow::bind_journal_origin(s,"https://second.test"));
 CHECK(s.pending()[0].body=="private-result-token");
 ow::bind_journal_origin(s,"https://first.test",std::string("https://first.test"));
 THROWS(ow::bind_journal_origin(s,"https://second.test",std::string("https://second.test")));
 CHECK(s.pending_count()==1);
}
TEST("legacy interrupted attempt needs the same protected coordinator origin"){
 Store s(":memory:");CHECK(s.reserve("a","job-a","p"));
 THROWS(ow::bind_journal_origin(s,"https://first.test",std::string("https://other.test")));
 CHECK(s.interrupted()==std::vector<std::string>{"a"});
 ow::bind_journal_origin(s,"https://first.test",std::string("https://first.test"));
}
