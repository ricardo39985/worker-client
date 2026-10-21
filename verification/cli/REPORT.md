# Local CLI implementation and release record

**Current (2026-10-21 11:48 UTC):** pre-merge gates passed on `dd08912`;
PR8 merged as `529ec104` with the identical tested tree. SAME exact merged-main
artifact is now independently verified installed/running on BOTH PCs, correct
LIMITED owner/session1, actual probes passed, connected/unpaused/accepting, SSH
CLI/config readback passed, release tasks removed. Latest idle-stop authority
superseded the earlier owner-tray-only blocker; actual race-safe stop paths and
per-PC evidence: [IDLE-STOP-RELEASE.md](IDLE-STOP-RELEASE.md).
Real coordinator-accepted per-PC jobs remain **NOT TESTED**. See
[current authorized ledger](RELEASE.md) and [acceptance](acceptance.md).

The report below is retained **historical evidence for source458c068**, including
its then-unimplemented gaps and then-current authorization boundary. Those are
not current gate claims; the later continuation implemented and executed them.

## Historical scope and source

Base `origin/main`: `e4366c326c124acc46f63bd7251bd0cdf24f4331` (fetched/verified).
Worktree: `/home/rick/Downloads/worker-client-cli`, branch `feat/full-cli`.
Draft PR: https://github.com/ricardo39985/worker-client/pull/8 .
The original worktree and every other agent branch are untouched.

Source commits: `73a61ce`, `0bfb288`, `336c05c`, `e595a1c`, `66ec088`,
`cb9e5452a4e033ab40889a7ce600b38aa60321e6`,
`b9366e5c66f4d1c858a4705e59aef8a54b94a86f` (first native green),
**`458c068352db59e2a0c2455896be68cac2c6caec` (final verified implementation)**.
Later ledger/documentation-only commits do not change the tested implementation.
See [reference/parity matrix](../../docs/cli.md) for exact command/setting semantics.
No inference or additional hardware-enable knob is invented.

## Executed portable gates

Existing cached Boost 1.89.0 headers:
`/home/rick/Downloads/worker-client-log2/.cache/boost_1_89_0`.
Native CI separately downloads and checksum-verifies the reviewed lock archive.

```
./scripts/test.sh -DOW_BOOST_ROOT=/home/rick/Downloads/worker-client-log2/.cache/boost_1_89_0
cmake --build build-tests --parallel 2
ctest --test-dir build-tests -V
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/home/rick/Downloads/worker-client-log2/.cache/boost_1_89_0
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V
```

Latest Release and ASan/UBSan gates each pass all **6 suites**: 101 core,
84 adaptive, 64 progress/timing, 18 rendition, 13 pipeline C++ contracts and
23 Python dependency tests (303 cases). Core and all standalone C++ suites are
instrumented in the sanitizer build; Python is not sanitizer-instrumented.
Final prescribed Release gate: `52-linux-final-gate.log`; final full sanitizer
suite output: `53-sanitize-final-gate.log` (both exit 0, source 458c068).
Logs `45-size-green-build.log` through `48-size-sanitize-tests.log` also pass.
`39..42` also record the full prescribed Linux gate on b9366e5. A final boundary
review found that a typed patch could grow a file past the loader's 256 KiB
reopen limit; `43..44` records an actual behavior-red test. Validation now rejects
that full document before persistence. Heap-backed local frames are bounded to
1 MiB so full current/effective metadata for any valid document fits without
Windows stack growth; a new native roundtrip/oversize-response contract covers it.
That follow-up is covered by the final native gate below.
Earlier successful runs and all failed regression evidence are retained.

- `01-red.log`: new control API initially missing (compile-red, not a Windows execution).
- `02-portable.log`: first implementation failed six configuration contracts;
  empty unconfigured endpoints corrected without weakening checks.
- `17..20`: actual behavior-red coordinator scope bypass/partial preference test,
  then centralized core fencing and all-green result. Live/offline do not own
  duplicate scope policy.
- `28..33`: UTF-8 BOM input rejected by Boost.JSON, then accepted only at the
  configuration stdin boundary; UTF-16 remains rejected. No input/secrets echoed.

## Actual Windows gates (manual existing CI, no PC operations)

- [39529757734](https://github.com/ricardo39985/worker-client/actions/runs/39529757734),
  `73a61ce`: real MSVC failure comparing Boost JSON value/std::string. Fixed at
  the string boundary; `08-windows-first-failure.log`.
- Own obsolete run 39530311235 cancelled after that defect was established.
- [39530558868](https://github.com/ricardo39985/worker-client/actions/runs/39530558868),
  `336c05c`: real MSVC optional<bool> JSON assignment failure. Fixed preserving
  **null unknown**, not inventing readiness; `23-windows-second-failure.log`.
- Own obsolete run 39531229562 cancelled after that defect was established.
- [39531260611](https://github.com/ricardo39985/worker-client/actions/runs/39531260611),
  `66ec088`: MSVC built actual worker/CLI/fixtures. **10/12 suites passed**, including
  native owner/anonymous/duplicate-pipe/security/migration/concurrent-tray/offline/
  timeout contracts and all four setup suites. Complete gate FAILED:
  - Existing DST display contract failed on the hosted UTC machine. UCRT's
    environment-TZ path changes `_timezone`/`_daylight` but does not reset
    `_dstbias` inherited from the host. The injected EST/EDT test fixture was
    incomplete on a no-DST host. Its fix explicitly supplies/restores the one-hour
    process-local CRT bias; **all timestamp assertions and production formatter
    code remain unchanged**, no skip or weakened expectation. Evidence: public
    [UCRT tzset source](https://github.com/huangqinjin/ucrt/blob/master/time/tzset.cpp),
    `tzset_from_system_nolock` versus `tzset_from_environment_nolock`.
  - Packaged actual CLI help/status/resume exercised, but piped config failed.
    BOM regression/fix and safe phase diagnostics added; next native run must
    establish the actual result, not a presumed pass.
  - Raw output: `27-windows-native-failure.log` (includes actual packaged help,
    fresh idle status and failed config test). Readiness was false, connected
    false, no jobs: explicitly **not** production/coordinator acceptance.
- [39532563899](https://github.com/ricardo39985/worker-client/actions/runs/39532563899),
  `cb9e545`: MSVC built successfully and **11/12 suites passed**. All original
  timestamp/DST assertions now pass with the complete process-local TZ fixture.
  The packaged real CLI successfully exercised piped atomic config, invalid
  no-partial-edit rejection, restart-required readback, reset, auth, resume,
  drain and exit. The remaining failure is the fixture-process exit-code check;
  `Start-Process` returned a PID-backed Process whose handle was not retained.
  The test now owns its live handle, still requires a known **zero** native exit
  code, and reports failures rather than treating null as success. Complete gate
  still FAILED, not relabelled green. `35-windows-cli-failure.log` preserves output.
- [39533530400](https://github.com/ricardo39985/worker-client/actions/runs/39533530400),
  **`b9366e5c66f4d1c858a4705e59aef8a54b94a86f`: PASS.** Fresh Windows-2022 MSVC
  Release compilation, PowerShell syntax parsing, reviewed Boost checksum,
  and **12/12 CTest suites passed**, gate exit 0. The packaged actual CLI fixture
  now verifies a known native exit code 0 and stopped-worker status unavailable
  (3), in addition to all help/status/config/resume/drain/auth/reset/exit checks.
  No assertions, failures or suites were skipped/reclassified to obtain the pass.
  Exact complete output: `37-windows-green.log`; run/source/job identity:
  `38-windows-result.json`; `36-windows-watch.log` records the successful gate.
  The retained handle fixed the fixture exit-code boundary; no native worker
  stop/exit policy change was needed. This is an actual compiled/exercised CLI,
  not a simulation of Windows, but the controller is intentionally idle/offline.

### Final exact-source Windows result

[39534777104](https://github.com/ricardo39985/worker-client/actions/runs/39534777104),
**458c068352db59e2a0c2455896be68cac2c6caec: PASS**. Fresh MSVC Release build and
**12/12 suites, 0 failures**, including the added large-frame/reply-limit kernel
IPC contract, packaged CLI fixture, all native contracts and all setup suites.
`50-windows-final-green.log` contains complete output;
`51-windows-final-result.json` records exact source/run/job identity;
`49-windows-final-watch.log` records successful completion. The fixture is an
actual console CLI controlling a different live controller process, not another
worker spawned by a control/config command. No released binary or user-PC install
is claimed; hosted build binaries were exercised but not published as a release.

## Outstanding acceptance and limits

[Acceptance list](acceptance.md) is authoritative. Same-owner SSH session 0 to
interactive session 1 has not been exercised. Anonymous ACL denial is not a
substitute for every distinct-account/restricted/non-elevated owner case.
The headless CLI fixture uses a **real controller, real pipe, real persistence,
no coordinator or jobs**. A fake-coordinator/active-job integration fixture has
NOT been authored (remaining implementation/test gap, not a runner-permission
excuse). CLI job drain/renewal/ACK and isolated legacy installer upgrade remain
unverified. Do not call this COMPLETE even if the idle native fixture passes.

No Danny/Rick PC was contacted/changed; no installed worker action, restart,
startup modification, machine policy/firewall/SSH change, release, merge, backend,
signing or production deployment occurred. Await separate release/deploy approval.
