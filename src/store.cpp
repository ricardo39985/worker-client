#include "ow/store.hpp"
#ifdef _WIN32
#if __has_include(<winsqlite/winsqlite3.h>)
#include <winsqlite/winsqlite3.h>
#else
#include <winsqlite3.h>
#endif
#else
#include <sqlite3.h>
#endif
#include <stdexcept>
namespace ow {
namespace {
class Statement {
 sqlite3_stmt* p_{};sqlite3* db_;
public:
 Statement(sqlite3* db,const char* sql):db_(db){if(sqlite3_prepare_v2(db,sql,-1,&p_,nullptr)!=SQLITE_OK)throw std::runtime_error(sqlite3_errmsg(db));}
 ~Statement(){sqlite3_finalize(p_);}
 void bind(int pos,const std::string& value){if(sqlite3_bind_text(p_,pos,value.data(),static_cast<int>(value.size()),SQLITE_TRANSIENT)!=SQLITE_OK)throw std::runtime_error("SQLite bind failed");}
 void integer(int pos,std::int64_t value){if(sqlite3_bind_int64(p_,pos,value)!=SQLITE_OK)throw std::runtime_error("SQLite integer bind failed");}
 bool step(){int r=sqlite3_step(p_);if(r!=SQLITE_ROW&&r!=SQLITE_DONE)throw std::runtime_error(sqlite3_errmsg(db_));return r==SQLITE_ROW;}
 std::string string(int c){const auto* p=sqlite3_column_text(p_,c);return p?std::string(reinterpret_cast<const char*>(p),static_cast<std::size_t>(sqlite3_column_bytes(p_,c))):std::string{};}
 std::int64_t integer(int c){return sqlite3_column_int64(p_,c);}
};
}
void Store::exec(const char* sql){char* err=nullptr;if(sqlite3_exec(db_,sql,nullptr,nullptr,&err)!=SQLITE_OK){std::string message=err?err:"SQLite failure";sqlite3_free(err);throw std::runtime_error(message);}}
Store::Store(const std::filesystem::path& path){
 auto u=path.u8string();if(sqlite3_open_v2(reinterpret_cast<const char*>(u.c_str()),&db_,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,nullptr)!=SQLITE_OK){if(db_)sqlite3_close(db_);db_=nullptr;throw std::runtime_error("Cannot open local journal; check permissions and disk");}
 try{
 sqlite3_busy_timeout(db_,5000);
 exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON;");
 Statement version(db_,"PRAGMA user_version");version.step();if(version.integer(0)>1)throw std::runtime_error("Journal was created by a newer worker; refusing downgrade");
 exec("CREATE TABLE IF NOT EXISTS attempts(id TEXT PRIMARY KEY,job TEXT NOT NULL,payload TEXT NOT NULL,state TEXT NOT NULL,result TEXT);"
      "CREATE TABLE IF NOT EXISTS outbox(sequence INTEGER PRIMARY KEY AUTOINCREMENT,attempt_id TEXT NOT NULL UNIQUE REFERENCES attempts(id),body TEXT NOT NULL);"
      "CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY,value TEXT NOT NULL); PRAGMA user_version=1;");
 }catch(...){sqlite3_close(db_);db_=nullptr;throw;}
}
Store::~Store(){if(db_)sqlite3_close(db_);}
bool Store::reserve(const std::string& id,const std::string& job,const std::string& payload){
 std::lock_guard lock(mutex_);Statement s(db_,"INSERT OR IGNORE INTO attempts(id,job,payload,state) VALUES(?,?,?,'reserved')");s.bind(1,id);s.bind(2,job);s.bind(3,payload);s.step();if(sqlite3_changes(db_))return true;
 Statement q(db_,"SELECT job,payload,state FROM attempts WHERE id=?");q.bind(1,id);q.step();return q.string(0)==job&&q.string(1)==payload&&q.string(2)=="reserved";
}
bool Store::start(const std::string& id){std::lock_guard lock(mutex_);Statement s(db_,"UPDATE attempts SET state='running' WHERE id=? AND state='reserved'");s.bind(1,id);s.step();return sqlite3_changes(db_)==1;}
std::optional<std::string> Store::state(const std::string& id){std::lock_guard lock(mutex_);Statement s(db_,"SELECT state FROM attempts WHERE id=?");s.bind(1,id);if(s.step())return s.string(0);return {};}
void Store::result(const std::string& id,const std::string& body){
 std::lock_guard lock(mutex_);exec("BEGIN IMMEDIATE");try{
 Statement q(db_,"SELECT state,result FROM attempts WHERE id=?");q.bind(1,id);if(!q.step())throw std::runtime_error("result has no persisted attempt");
 if(q.string(0)=="reported"||q.string(0)=="pending"){if(q.string(1)!=body)throw std::runtime_error("conflicting result for same attempt");exec("COMMIT");return;}
 Statement s(db_,"UPDATE attempts SET state='pending',result=? WHERE id=?");s.bind(1,body);s.bind(2,id);s.step();
 Statement o(db_,"INSERT INTO outbox(attempt_id,body) VALUES(?,?)");o.bind(1,id);o.bind(2,body);o.step();exec("COMMIT");
 }catch(...){exec("ROLLBACK");throw;}
}
std::uint64_t Store::pending_count(){std::lock_guard lock(mutex_);Statement s(db_,"SELECT COUNT(*) FROM outbox");s.step();return static_cast<std::uint64_t>(s.integer(0));}
std::vector<Pending> Store::pending(){std::lock_guard lock(mutex_);Statement s(db_,"SELECT sequence,attempt_id,body FROM outbox ORDER BY sequence LIMIT 128");std::vector<Pending> out;while(s.step())out.push_back({s.integer(0),s.string(1),s.string(2)});return out;}
bool Store::acknowledge(std::int64_t seq,const std::string& id){
 std::lock_guard lock(mutex_);exec("BEGIN IMMEDIATE");try{
 Statement d(db_,"DELETE FROM outbox WHERE sequence=? AND attempt_id=?");d.integer(1,seq);d.bind(2,id);d.step();bool found=sqlite3_changes(db_)==1;
 if(found){Statement u(db_,"UPDATE attempts SET state='reported' WHERE id=? AND state='pending'");u.bind(1,id);u.step();}exec("COMMIT");return found;
 }catch(...){exec("ROLLBACK");throw;}
}
std::vector<std::string> Store::interrupted(){std::lock_guard lock(mutex_);Statement s(db_,"SELECT id FROM attempts WHERE state IN ('reserved','running')");std::vector<std::string> ids;while(s.step())ids.push_back(s.string(0));return ids;}
void Store::set(const std::string& key,const std::string& value){std::lock_guard lock(mutex_);Statement s(db_,"INSERT OR REPLACE INTO settings(key,value) VALUES(?,?)");s.bind(1,key);s.bind(2,value);s.step();}
std::optional<std::string> Store::get(const std::string& key){std::lock_guard lock(mutex_);Statement s(db_,"SELECT value FROM settings WHERE key=?");s.bind(1,key);if(s.step())return s.string(0);return {};}
}
