#include "test.hpp"
#include "ow/worker_status.hpp"
#include <cstdlib>
namespace {
constexpr std::int64_t wall=1792249802000; // 17/10/2026 15:10:02 UTC
ow::StatusReadings readings(){ow::StatusReadings r;r.queue=0;r.ram_bytes=41200000000ULL;r.scratch_bytes=402000000000ULL;r.encoders="cpu, nvenc";r.cards=std::vector<ow::StatusCard>{{"pci-a","3070"},{"pci-b","3050"}};return r;}
std::string plain(const std::vector<ow::ConsoleLine>& lines){std::string s;for(const auto& l:lines)s+=l.text+'\n';return s;}
void timezone_utc(){
#ifdef _WIN32
 _putenv_s("TZ","UTC");_tzset();
#else
 setenv("TZ","UTC",1);tzset();
#endif
}
}
TEST("outcomes increment once per executed attempt and survive pause resume"){
 ow::WorkerStatus s(wall,0);s.start("a","video renditions",wall,0);CHECK(s.finish("a",ow::JobOutcome::succeeded,"",wall+19000,19000));
 CHECK(!s.finish("a",ow::JobOutcome::failed,"late",wall+20000,20000));
 s.state("paused","operator",wall+20000,20000);s.state("accepting","resumed",wall+21000,21000);
 s.start("b","image renditions",wall+22000,22000);s.finish("b",ow::JobOutcome::failed,"digest mismatch",wall+23000,23000);
 s.start("c","video renditions",wall+24000,24000);s.finish("c",ow::JobOutcome::cancelled,"operator",wall+25000,25000);
 auto summary=s.summary(wall+26000,26000,readings());CHECK(summary.find("1 ok 1 failed 1 cancelled")!=std::string::npos);
 auto history=s.history();CHECK(history.size()==3);CHECK(history[1].text.find("failed")!=std::string::npos);CHECK(history[1].text.find("digest mismatch")!=std::string::npos);
}
TEST("fixed clock summary has exact finish time duration queue and computed idle"){
 timezone_utc();ow::WorkerStatus s(wall,0);s.start("a","video renditions",wall,0);s.finish("a",ow::JobOutcome::succeeded,"",wall+19000,19000);
 CHECK(s.summary(wall+211000,211000,readings())=="17/10 15:13:33 stats: 1 ok 0 failed 0 cancelled | last job 15:10 (19s, video) | queue 0 | idle 00:03:12 | running 0 | ram free 41.2 GB | scratch free 402 GB | priority unknown | paused: starting (since 15:10:02)");
}
TEST("idle table is fixed width deterministic and derives since and uptime"){
 timezone_utc();ow::WorkerStatus s(wall,0);s.state("accepting","resumed",wall,0);s.priority("high");s.start("a","video renditions",wall,0);s.finish("a",ow::JobOutcome::succeeded,"",wall+19000,19000);
 auto lines=s.table(wall+60000,60000,readings());CHECK(plain(lines)==plain(s.table(wall+60000,60000,readings())));
 CHECK(lines[0].text=="17/10 15:11:02  status");
 CHECK(lines[1].text=="  state      accepting                since 15:10:02");
 CHECK(lines[2].text=="  priority   high");CHECK(lines[3].text=="  jobs       running 0   queue 0");
 CHECK(lines[4].text=="  totals     1 ok    0 failed   0 cancelled");
 CHECK(lines[5].text=="  last job   15:10:21  video renditions          19s");
 CHECK(lines[6].text=="  cards      3070 idle             3050 idle             cpu idle");
 CHECK(lines[7].text=="  memory     ram free 41.2 GB   scratch free 402 GB");
 CHECK(lines[8].text=="  encoders   cpu, nvenc");CHECK(lines[9].text=="  uptime     00:01:00");
 CHECK(lines[1].spans.size()==1);CHECK(lines[1].spans[0].color==ow::ConsoleColor::green);CHECK(lines[4].spans.empty());
}
TEST("missing readings and last job stay unknown never numeric zero"){
 timezone_utc();ow::WorkerStatus s(wall,0);auto text=plain(s.table(wall,0,{}));
 CHECK(text.find("priority   unknown")!=std::string::npos);CHECK(text.find("queue unknown")!=std::string::npos);CHECK(text.find("ram free unknown")!=std::string::npos);CHECK(text.find("scratch free unknown")!=std::string::npos);CHECK(text.find("last job   unknown")!=std::string::npos);CHECK(text.find("cards      unknown")!=std::string::npos);CHECK(text.find("cpu idle")!=std::string::npos);CHECK(text.find("encoders   unknown")!=std::string::npos);
}
TEST("idle live and durable cadence are independent and no table runs during work"){
 ow::WorkerStatus s(wall,0);auto first=s.poll(0);CHECK(first.live&&first.durable&&first.summary);CHECK(!s.poll(29999).live);
 auto thirty=s.poll(30000);CHECK(thirty.live&&!thirty.durable&&thirty.summary);CHECK(!s.poll(299999).durable);CHECK(s.poll(300000).durable);
 s.start("a","video",wall+301000,301000);auto busy=s.poll(900000);CHECK(!busy.live&&!busy.durable);CHECK(s.table(wall+900000,900000,readings()).empty());
 s.finish("a",ow::JobOutcome::succeeded,"",wall+901000,901000);auto done=s.poll(901000);CHECK(done.live&&done.summary);CHECK(!done.durable);CHECK(!s.poll(901001).live);
}
TEST("physical cards deduplicate by identity truncate names and wrap fixed cells"){
 ow::WorkerStatus s(wall,0);auto r=readings();r.cards->push_back({"pci-a","alias must not appear"});r.cards->push_back({"pci-c","an enormously long adapter name"});
 auto lines=s.table(wall,0,r);auto text=plain(lines);CHECK(text.find("alias must")==std::string::npos);CHECK(text.find("an enormous~ idle")!=std::string::npos);
 CHECK(text.find("  cards      3070 idle             3050 idle             an enormous~ idle\n             cpu idle\n")!=std::string::npos);
 for(const auto& line:lines)CHECK(ow::console_text_width(line.text)<=80);
}
TEST("outcome and state colours have only nonzero failures red"){
 ow::WorkerStatus s(wall,0);s.state("paused","operator",wall,0);s.start("a","video",wall,0);s.finish("a",ow::JobOutcome::failed,"bad input",wall+1000,1000);
 auto lines=s.table(wall+1000,1000,readings());CHECK(lines[1].spans[0].color==ow::ConsoleColor::orange);CHECK(lines[4].color==ow::ConsoleColor::white);CHECK(lines[4].spans.size()==1);CHECK(lines[4].spans[0].color==ow::ConsoleColor::red);
 CHECK(s.history()[0].color==ow::ConsoleColor::red);
}
TEST("placement describes actual running encoder and clears on drain"){
 ow::WorkerStatus s(wall,0);s.start("a","video",wall,0);CHECK(s.machine(readings()).find("a=unknown")!=std::string::npos);s.placement("a","pci-b");CHECK(s.machine(readings()).find("a=3050")!=std::string::npos);s.placement("a","cpu");CHECK(s.machine(readings()).find("a=cpu")!=std::string::npos);s.finish("a",ow::JobOutcome::cancelled,"operator",wall+1000,1000);CHECK(s.machine(readings()).find("running 0")!=std::string::npos);
}
TEST("status sidecar rows roundtrip colours and durable blocks stay out of live scrollback"){
 ow::WorkerStatus s(wall,0);s.state("accepting","resumed",wall,0);auto lines=s.table(wall,0,readings());auto decoded=ow::parse_status_frame(plain(lines));CHECK(plain(decoded)==plain(lines));CHECK(decoded[1].spans[0].color==ow::ConsoleColor::green);
 ow::LiveConsoleView view;for(const auto& line:lines){auto f=view.consume("2026-10-17 15:10:02.000Z "+line.text);CHECK(!f.event);}CHECK(view.consume("2026-10-17 15:10:03.000Z STATE CONNECTED").event.has_value());
}
TEST("live idle frame is suppressed until the last concurrent job terminates"){
 ow::LiveConsoleView view;CHECK(view.jobs_running()==0);
 view.consume("START attempt-abcd media.video.renditions.v1 video renditions");view.consume("START attempt-efgh media.image.renditions.v1 image renditions");CHECK(view.jobs_running()==2);
 view.consume("JOB attempt-abcd STAGE | convert feed.mp4 | run 1");CHECK(view.jobs_running()==2);
 view.consume("JOB attempt-abcd succeeded | done in 19s (video renditions)");CHECK(view.jobs_running()==1);
 view.consume("JOB attempt-efgh cancelled | done in 2s (image renditions) | operator");CHECK(view.jobs_running()==0);
 view.consume("JOB attempt-ijkl PROGRESS | convert feed.mp4 | run 1 | 5% | elapsed 2s");CHECK(view.jobs_running()==1);
 view.consume("WORKER STARTED");CHECK(view.jobs_running()==0);CHECK(view.rows().empty());
}
TEST("bounded history survives new jobs and derives duration across wall clock correction"){
 ow::WorkerStatus s(wall,1000);for(unsigned i=0;i<12;++i){auto id="attempt-"+std::to_string(1000+i);s.start(id,"video renditions",wall+1000,2000+i*1000);s.finish(id,ow::JobOutcome::succeeded,"",wall-10000,2500+i*1000);}
 CHECK(s.history().size()==10);CHECK(s.history().front().text.find("1002")!=std::string::npos);CHECK(s.history().back().text.find("500ms")!=std::string::npos);
 CHECK(s.summary(wall,20000,readings()).find("12 ok")!=std::string::npos);CHECK(s.completion().find("succeeded | done in 500ms (video renditions)")!=std::string::npos);
 CHECK(s.startup().find("counts since this process")!=std::string::npos);
}
TEST("asset stages retries completions and full summaries keep observable words"){
 ow::ConsoleFormatter f;auto stage=f.render("JOB attempt-abcd STAGE | verify poster.webp sha256 | run 1");CHECK(stage.text=="job abcd verify poster.webp sha256");CHECK(stage.color==ow::ConsoleColor::blue);
 auto retry=f.render("JOB attempt-abcd RETRY | convert feed.mp4 | GPU encode or validation failed; retrying CPU");CHECK(retry.text.find("GPU encode or validation failed")!=std::string::npos);CHECK(retry.color==ow::ConsoleColor::orange);
 auto done=f.render("JOB attempt-abcd succeeded | done in 19s (video renditions)");CHECK(done.text=="job abcd succeeded done in 19s (video renditions)");CHECK(done.color==ow::ConsoleColor::green);
 ow::WorkerStatus s(wall,0);auto summary=s.summary(wall,0,{});CHECK(f.render("2026-10-17 15:10:02.000Z "+summary).text==summary);
}
TEST("idle ticks do not reset state since and nonzero failed span roundtrips"){
 ow::WorkerStatus s(wall,0);s.state("paused","operator",wall,0);s.start("attempt-abcd","video",wall,0);s.finish("attempt-abcd",ow::JobOutcome::failed,"input integrity",wall+1000,1000);
 auto first=s.table(wall+1000,1000,readings());s.poll(1000);s.poll(31000);s.state("paused","operator",wall+31000,31000);auto next=s.table(wall+31000,31000,readings());CHECK(first[1].text==next[1].text);
 auto parsed=ow::parse_status_frame(plain(next));CHECK(parsed[4].spans[0].offset==next[4].spans[0].offset);CHECK(parsed[4].spans[0].length==next[4].spans[0].length);CHECK(parsed[4].spans[0].color==ow::ConsoleColor::red);
 CHECK(ow::parse_status_frame(plain(s.history()))[0].color==ow::ConsoleColor::red);
}
TEST("redirected durable table keeps the same fixed width plain block"){
 ow::WorkerStatus s(wall,0);auto table=s.table(wall,0,readings());ow::LiveConsoleView view;std::vector<ow::ConsoleLine> forwarded;
 for(const auto& row:table){auto feed=view.consume("2026-10-17 15:10:02.000Z "+row.text);CHECK(feed.durable_status.has_value());forwarded.push_back(*feed.durable_status);CHECK(!feed.event);}
 CHECK(plain(forwarded)==plain(table));CHECK(plain(forwarded).find('\x1b')==std::string::npos);
}
TEST("summary carries running placement free resources priority and accepting state"){
 ow::WorkerStatus s(wall,0);s.state("accepting","resumed",wall,0);s.priority("maximum");s.start("attempt-abcd","video",wall,0);s.placement("attempt-abcd","pci-b");auto summary=s.summary(wall+1000,1000,readings());
 CHECK(summary.find("running 1 | job attempt-abcd=3050")!=std::string::npos);CHECK(summary.find("priority maximum | accepting: resumed (since ")!=std::string::npos);
 ow::ConsoleFormatter formatter;auto rendered=formatter.render("2026-10-17 15:10:03.000Z "+summary);CHECK(rendered.text.find("job abcd=3050")!=std::string::npos);CHECK(rendered.text.find("attempt-abcd")==std::string::npos);CHECK(rendered.text.find("scratch free 402 GB")!=std::string::npos);CHECK(rendered.color==ow::ConsoleColor::blue);
 auto cards=readings();cards.cards=std::vector<ow::StatusCard>{{"pci-a","NVIDIA GeForce RTX 3070"},{"pci-b","NVIDIA GeForce RTX 3050"}};ow::WorkerStatus idle(wall,0);auto text=plain(idle.table(wall,0,cards));CHECK(text.find("RTX 3070 idle")!=std::string::npos);CHECK(text.find("RTX 3050 idle")!=std::string::npos);
}
TEST("totals keep outcome columns at fixed positions as counts grow"){
 ow::WorkerStatus s(wall,0);auto initial=s.table(wall,0,readings())[4].text;
 for(unsigned i=0;i<14;++i){auto id=std::to_string(i);s.start(id,"video",wall,0);s.finish(id,ow::JobOutcome::succeeded,"",wall+1000,1000);}
 auto later=s.table(wall+1000,1000,readings())[4].text;
 CHECK(initial.find("0 failed")==later.find("0 failed"));CHECK(initial.find("0 cancelled")==later.find("0 cancelled"));CHECK(later.find("14 ok")!=std::string::npos);
}
TEST("encoder and failure records retain asset names and failure colour"){
 ow::ConsoleFormatter f;
 auto encoder=f.render("JOB attempt-abcd ENCODER | nvenc/h264 GPU 1 | file=feed.mp4 pid=99 physical_id=pci-b");CHECK(encoder.text=="job abcd convert feed.mp4 nvenc/h264 GPU 1");CHECK(encoder.color==ow::ConsoleColor::blue);
 CHECK(f.render("RESULT PENDING ACK attempt-abcd failed").color==ow::ConsoleColor::red);
 CHECK(f.render("RESULT PENDING ACK attempt-abcd abandoned").color==ow::ConsoleColor::white);
 ow::WorkerStatus s(wall,0);CHECK(!s.accepting());s.state("accepting","connected",wall,0);CHECK(s.accepting());s.state("paused","coordinator offline",wall+1000,1000);auto table=s.table(wall+1000,1000,readings());CHECK(table[1].text.find("coordinator offline")!=std::string::npos);CHECK(ow::console_text_width(table[1].text)<=80);
}
TEST("running progress row retains the physical card across asset updates"){
 ow::LiveConsoleView view;view.consume("START attempt-abcd media.video.renditions.v1 video renditions");
 view.consume("JOB attempt-abcd ENCODER | nvenc/h264 GPU 1 | file=feed.mp4 card=RTX_3050 physical_id=pci-b");
 view.consume("JOB attempt-abcd PROGRESS | convert feed.mp4 | run 1 | 62% | elapsed 4s | remaining 3s");
 auto frame=view.render({false,false,false});CHECK(frame.find("feed.mp4 (RTX 3050)")!=std::string::npos);CHECK(frame.find("62%")!=std::string::npos);CHECK(frame.find("3s left")!=std::string::npos);
 view.consume("JOB attempt-abcd ENCODER | cpu/h264 | file=feed.mp4 card=cpu");view.consume("JOB attempt-abcd PROGRESS | convert feed.mp4 | run 2 | 10% | elapsed 1s");frame=view.render({false,false,false});CHECK(frame.find("(cpu)")!=std::string::npos);CHECK(frame.find("RTX 3050")==std::string::npos);
}
TEST("outcome colour follows the outcome word regardless of reason wording"){
 ow::WorkerStatus s(wall,0);s.start("attempt-abcd","video",wall,0);s.finish("attempt-abcd",ow::JobOutcome::cancelled,"operator stopped after failed retry",wall+1000,1000);
 CHECK(ow::parse_status_frame(plain(s.history()))[0].color==ow::ConsoleColor::orange);
 ow::ConsoleFormatter f;CHECK(f.render("JOB attempt-abcd CPU fallback: nvenc/hevc unsupported size").color==ow::ConsoleColor::orange);CHECK(f.render("JOB attempt-abcd selected nvenc/hevc 1920x1080").color==ow::ConsoleColor::blue);
}

TEST("idle status exposes withheld card codec reason and retry instead of calling it idle"){
 ow::WorkerStatus s(wall,0);auto r=readings();
 r.gpu_recovery={"RTX 3070 h264 withheld: memory/session unavailable; retry in 30s"};
 auto lines=s.table(wall,0,r);auto text=plain(lines);
 CHECK(text.find("RTX 3070 h264 withheld: memory/session unavailable")!=std::string::npos);
 CHECK(text.find("retry in 30s")!=std::string::npos);
 for(const auto& line:lines)CHECK(line.text.size()<=80);
}

TEST("long withheld reasons wrap without losing retry and remain durable status rows"){
 ow::WorkerStatus s(wall,0);auto r=readings();r.gpu_recovery={std::string(90,'x')+" h264 withheld: probe failed; retry when idle"};
 ow::LiveConsoleView view;std::string all;
 for(const auto& row:s.table(wall,0,r)){
  CHECK(row.text.size()<=80);auto f=view.consume("2026-10-17 15:10:02.000Z "+row.text);
  CHECK(f.durable_status.has_value());CHECK(!f.event);if(row.text.size()>=13)all+=row.text.substr(13);
 }
 CHECK(all.find("retry when idle")!=std::string::npos);
}

TEST("active status exposes the actual selected child memory budget"){
 ow::WorkerStatus status(0,0);status.start("attempt-1234","video",0,0);
 status.placement("attempt-1234","3070",768);ow::StatusReadings readings;
 readings.cards=std::vector<ow::StatusCard>{{"3070","RTX 3070"}};
 CHECK(status.machine(readings).find("child budget 768 MiB")!=std::string::npos);
 status.placement("attempt-1234","3070",1792);
 CHECK(status.summary(1000,1000,readings).find("child budget 1792 MiB")!=std::string::npos);
}

TEST("encoder start completion and stats name the actual GPU for the same file"){
 ow::WorkerStatus s(wall,0);s.start("gpu-job","video renditions",wall,0);
 auto r=readings();r.cards=std::vector<ow::StatusCard>{{"pci-a","NVIDIA GeForce RTX 3070"},{"pci-b","NVIDIA GeForce RTX 3050"}};
 for(const auto& file:{std::string("feed.mp4"),std::string("optimized.mp4")}){
  const auto encoder=file=="feed.mp4"?"nvenc/h264 GPU 1":"nvenc/hevc GPU 1";
  const auto start=s.encoder_line("gpu-job",encoder,file,"pci-b",768,r,false);
  CHECK(start=="JOB gpu-job ENCODER | "+std::string(encoder)+" | file="+file+" card=RTX_3050 child_budget_MiB=768");
  const auto done=s.encoder_line("gpu-job",encoder,file,"pci-b",768,r,true);
  CHECK(done=="JOB gpu-job ENCODER | "+std::string(encoder)+" done | file="+file+" card=RTX_3050");
  CHECK(s.summary(wall+1000,1000,r).find("job gpu-job=RTX 3050 child budget 768 MiB")!=std::string::npos);
  CHECK(done.find("card=cpu")==std::string::npos);
 }
}
TEST("CPU encoder including a retry reports CPU at start completion and in stats"){
 ow::WorkerStatus s(wall,0);s.start("retry-job","video renditions",wall,0);auto r=readings();
 s.encoder_line("retry-job","nvenc/hevc GPU 1","optimized.mp4","pci-b",768,r,false);
 auto start=s.encoder_line("retry-job","cpu/hevc","optimized.mp4","cpu",512,r,false);
 auto done=s.encoder_line("retry-job","cpu/hevc","optimized.mp4","cpu",512,r,true);
 CHECK(start=="JOB retry-job ENCODER | cpu/hevc | file=optimized.mp4 card=cpu child_budget_MiB=512");
 CHECK(done=="JOB retry-job ENCODER | cpu/hevc done | file=optimized.mp4 card=cpu");
 CHECK(s.summary(wall+1000,1000,r).find("job retry-job=cpu child budget 512 MiB")!=std::string::npos);
 CHECK(s.summary(wall+1000,1000,r).find("job retry-job=3050")==std::string::npos);
}

TEST("encoder reporting retains actual child card when display inventory is unavailable"){
 ow::WorkerStatus s(wall,0);s.start("actual-job","video renditions",wall,0);auto r=readings();r.cards.reset();
 auto start=s.encoder_line("actual-job","nvenc/hevc GPU 1","optimized.mp4","pci-b",768,r,false,"NVIDIA GeForce RTX 3050");
 CHECK(start.find("file=optimized.mp4 card=RTX_3050")!=std::string::npos);
 auto done=s.encoder_line("actual-job","nvenc/hevc GPU 1","optimized.mp4","pci-b",768,r,true,"NVIDIA GeForce RTX 3050");
 CHECK(done.find("file=optimized.mp4 card=RTX_3050")!=std::string::npos);
 CHECK(s.summary(wall+1000,1000,r).find("job actual-job=RTX 3050 child budget 768 MiB")!=std::string::npos);
}
