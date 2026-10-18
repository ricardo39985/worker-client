# Test policy and evidence

Current worker-console reporting evidence: see
[the log-content gate ledger](../verification/log-content/REPORT.md) and
[the reporting contract](worker-console.md).
The controller counts local outcomes, emits complete stats, and publishes an
idle table on independent live/durable cadences. This change has portable source
verification only; MSVC, native console rendering and actual Windows jobs remain
unrun. No Windows machine or production environment was contacted or changed.


Current `fix/worker-next` evidence: see
[verification/worker-next/REPORT.md](../verification/worker-next/REPORT.md).
The PCI-instance source correction passes Linux contracts; Windows/MSVC and
physical two-GPU acceptance remain unrun. The approved FFmpeg binary and recovered
exact source/build materials are now published with both lock URLs set; see
[the publication report](../verification/ffmpeg-publication/REPORT.md) for fresh
anonymous download verification and the NOTICE's third-party source-scope limit.
Historical publication-pending narratives below describe earlier states.


Retained FFmpeg distribution evidence is in
`verification/ffmpeg-dependency/REPORT.md`. The dependency-only Linux snapshot
passed six CTest suites (187 contracts including 17 new offline policy cases).
Sixteen additional PowerShell converter contracts are registered but unrun here;
Windows compilation, native setup/Doctor and real published-artifact checks are
still required. See `docs/ffmpeg-distribution.md` for the publication procedure.
Current multi-GPU source candidate: see
[verification/multigpu/REPORT.md](../verification/multigpu/REPORT.md) for the
code diagnosis, red/green gates, per-card session policy and operator checklist.
This change has no MSVC/native or physical RTX 3070/3050 execution evidence.
No Windows machine or production server was contacted, changed or restarted.

Current geometry correction evidence: see
`verification/gpu-dimensions/REPORT.md`. The sample size no longer sets encoder
limits; startup tuning is 1920x1080 and larger hardware work uses guarded
validation, targeted demotion and CPU retry. Portable and sanitizer evidence is
recorded there. Windows/MSVC and physical GPU checks remain unrun for this tree.
No worker was contacted or restarted. Historical verification narratives below
do not establish native verification for this change.

Behavior and contracts are the unit of testing, not the current implementation.
Tests must survive changing helper structure, parser implementation, scheduler
internals or storage wrappers while externally promised behavior stays intact.

## Recorded runs

The local environment has GCC 14.2.0, CMake 3.31.6, installed Boost 1.83 headers
and SQLite 3.46.1. Installed Boost is allowed for local development only; the
Windows source installer uses the reviewed 1.89.0 archive. These are not the
same dependency/runtime matrix, and Windows compatibility is not inferred.

verification/01-red.txt records the original failing admission behavior tests.
verification/06-review-green.txt records 50 passing current core contracts.
verification/09-sanitizer-tests.txt records 50 passing ASan/UBSan core contracts,
with detect_leaks=1 and halt_on_error=1. Upstream Boost.JSON is linked as a
separate library; this run instruments the core and test callers, not every
upstream dependency. No local Windows, PowerShell or live gateway run occurred.

## Local commands

With prerequisites already installed (no automatic package installation):

```sh
./scripts/test.sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
```

On Windows, setup.cmd builds Release and runs all eight registered suites before
installation. A standalone build is also possible with a verified Boost root:

```powershell
cmake -S . -B build-win -G 'Visual Studio 17 2022' -A x64 -DOW_BOOST_ROOT='C:\verified\boost_1_89_0'
cmake --build build-win --config Release --parallel 2
ctest --test-dir build-win -C Release --output-on-failure
```

Native tests cover UTF-8 filenames, DPAPI round-trip/corruption rejection, durable
pairing-key identity and termination of a real root/descendant process tree.
They require an actual supported Windows kernel and SDK. A native CI/test failure
must be fixed, not skipped because the portable suite is green.

## Required manual and staging acceptance

Close the console's X while synthetic jobs run; verify they continue and the
tray can reopen the viewer. Change priorities and preferences; confirm running
jobs retain identity, disabled tasks are rejected, and pause rejects reserved
but unstarted tasks. Exit using both drain and stop; inspect durable retry state.
Verify duplicate launch does not create a second worker. Check explorer/tray
restart, Windows sign-out, reboot/logon startup, SSH launch behavior and sleep.

Gateway tests must exercise approval expiry/denial, key/credential binding,
wrong identity, revocation, TLS failure, missing endpoint, mismatched protocol,
message fragmentation, lost ACK, duplicate messages, slow storage, partial input,
invalid digest, upload failure, no Internet, disk pressure and a PC disappearing
while several jobs execute. A stale result must never overwrite a newer attempt.
Inspect RAM, CPU, storage and UI responsiveness on the actual 8 GB laptop before
raising resource ceilings. Native tests and smoke tests may reveal platform
issues that source review could not establish here.

## TDD workflow for each change

State an observable outcome; write and run the failing test; confirm it fails
for the intended reason; implement; run focused and relevant complete suites;
refactor; document exact results and remaining unrun checks. Never assert private
source snippets, method names or arbitrary logging order as a replacement for
behavior tests. Never contact production as part of an automated test.

## Current client update evidence

The original fifty-case runs above are historical. `verification/client-update/`
contains the freshly executed 50-case baseline, 68-case enrollment red/green,
71-case journal-scope red/green and current 71-case ASan/UBSan run. Windows tests
now include six OS contracts and eleven PowerShell setup contracts; neither was
executed in this Linux environment. Setup tests were authored first, but their
runtime was unavailable: do not call that an executed red stage. The actual
pairing port, external credential storage and HTTP transport are faked only at
those boundaries; real temporary SQLite is used for durable journal guarantees.

Read docs/status.md before reporting readiness. Source changes, portable checks,
Windows compilation, real gateway interoperability and actual remote job execution
are separate acceptance levels.

## Terminal progress change (October 7, 2026)

The dedicated `progress_contracts` executable is independent of Boost and Windows.
Eight contracts passed using GCC with `-std=c++20 -Wall -Wextra -Werror -pthread`,
including concurrent updates, measured percentages, unknown totals, quiet
activity, stage reset and failure without invented completion. They also passed
ASan/UBSan with `detect_leaks=0` and `halt_on_error=1`. LeakSanitizer with leak
detection enabled failed because this runtime could not inspect `/proc/2/task`;
no leak-detection pass is claimed. The existing core suite was not rerun.

Fifteen new `setup_progress_contracts` cases cover elapsed/quiet output, real
stream hashing, exact byte limits, truncated downloads, staging collision
preservation, real ZIP extraction, child stdout/stderr before exit, Windows argv
round-trip, nonzero exit and owned-child timeout. They use synthetic local files
and processes, not production media or credentials. Windows PowerShell 5.1 is
unavailable here: neither these cases nor the existing setup suite has executed
for this change. No executed PowerShell red or green stage is claimed.

Required: Windows syntax checks, both PowerShell suites, all eight registered CTest
suites with MSVC, the real converter doctor and setup/bootstrap/network/timeout
observations. The portable counter tests do not establish WinHTTP transfer,
PowerShell process streaming, tray behavior or a successful remote conversion.



## App rendition candidate (October 7, 2026)

Three standalone fixed-profile behavior contracts passed with installed GCC:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -Iinclude -Itests tests/main.cpp tests/media_rendition_test.cpp -o /tmp/ow-rendition-contracts
/tmp/ow-rendition-contracts
g++ -std=c++20 -Wall -Wextra -Werror -Iinclude tests/media_rendition_probe.cpp -o /tmp/ow-rendition-probe
python3 tests/media_rendition_smoke.py --probe /tmp/ow-rendition-probe
```

The smoke run executes the actual command factory with synthetic media, then
probes/decodes full-size H.264, HEVC and WebP. Video dimensions, duration and
frame rate were preserved. It uses installed Linux FFmpeg, not the Windows
pinned binary; no packages or SDKs are installed by these tests.

Two new protocol cases were authored before the parser update. The full portable
protocol/admission/journal suite is blocked here by missing Boost.JSON headers;
its attempted compilation reported `boost/json.hpp: No such file or directory`.
No executed protocol red/green stage or Windows compilation is claimed. Native
Windows/MSVC/PowerShell tests, probes, cancellation across multiple renditions,
ordinary app uploads and computer-loss/local-fallback acceptance remain required.
Setup now registers an additional standalone rendition suite (six CTest suites
on Windows). Passing old diagnostic probes is not proof of these new capabilities.

## Quiet idle console (October 7, 2026)

A behavior-preserving extraction kept the existing eight progress contracts green.
The new activity-output regression then failed with `Idle connection must not
emit activity lines`. It now passes: repeated zero-job/zero-result samples produce
no summary, active jobs and pending acknowledgments remain visible, and a drained
backlog returns to silence. The existing measured/quiet/failure/concurrent progress
contracts also pass with installed GCC using C++20, warnings as errors and pthread.
Windows/MSVC compilation and the actual tray/viewer check have not run for this
change. Run setup's registered native tests before installing a release; observe
an idle interval, a job and its ACK, and a disconnect/reconnect on Windows.

## Truthful conversion progress and readable viewer (October 17, 2026)

The progress behavior was authored contract-first. The initial standalone GCC
compile failed with the intended missing `EncoderProgress`, timeline-total,
remaining-time, retry-generation and live-view APIs. After implementation, the
same warnings-as-errors command passed:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pthread -Iinclude tests/progress_test.cpp -o /tmp/ow-progress-contracts
/tmp/ow-progress-contracts
```

The timeline contract uses a precisely simulated 120-second FFmpeg source report
and a 30-second `out_time_us`, while deliberately supplying a 90,000,000-byte
output counter. It requires 25 percent and a 90-second remaining estimate, which
demonstrates that output size is ignored. Split reports, malformed values,
backward positions, stage retry reset, four concurrent job rows, cancellation at
0 and 99 percent, redirected/disabled presentation, human time, hour-scale work,
millisecond completion and one-time unknown progress are also covered.

The reviewed Boost 1.89.0 archive was downloaded to `/tmp`, matched lock hash
`9de758db755e8330a01d995b0a24d09798048400ac25c03fc5ea9be364b13c93`, and was
passed to CMake without installation. `scripts/test.sh` passed all four portable
CTest executables: 74 core, 9 pipeline, 3 rendition, and the progress/console
contract executable. Configure and build also exited zero under ASan/UBSan; all
four tests passed with `detect_leaks=1` and `halt_on_error=1`. The installed
Linux FFmpeg synthetic profile smoke decoded H.264, HEVC and WebP successfully.

No PowerShell, MSVC, Windows SDK or Windows kernel is available in this authoring
environment. The Windows console/controller code and the native 1,800-line
concurrent rotation test are therefore authored but uncompiled and unrun. A real
Windows gate must still run all registered CTest suites and judge cursor redraw,
colors, legacy-console fallback, viewer close/reopen and a real conversion.

## Short readable console (October 17, 2026)

The progress/console executable now carries fifteen behavior contracts. They
exercise suffix stability, live four-character collision disambiguation,
suffix-to-full-id resolution from a plain durable log, restart determinism,
midnight/month boundaries, fixed and daylight-saving local zones, width after the
ASCII bar is inserted, impossible long messages, old log records, the five-color
mapping, retry visibility, four concurrent rows, cancellation at 0/99 percent,
idle silence and the preserved encoder-timeline byte accounting. The focused
warnings-as-errors command passed 15 contracts:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp tests/progress_test.cpp -o .cache/ow-progress-contracts
.cache/ow-progress-contracts
```

The reviewed Boost 1.89.0 archive was downloaded to the ignored `.cache/`, its
SHA-256 matched lock hash `9de758db755e8330a01d995b0a24d09798048400ac25c03fc5ea9be364b13c93`,
and it was passed to CMake without installation. Results:

```sh
./scripts/test.sh -DOW_BOOST_ROOT="$PWD/.cache/boost_1_89_0"
# 4/4 CTest executables passed: 74 core, 9 pipeline, 3 rendition,
# 15 progress/console (101 contracts); exit 0

cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT="$PWD/.cache/boost_1_89_0"
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure
# 4/4 passed, 101 contracts with leak detection and halt on error; exit 0

python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe
# Synthetic full-size H.264, HEVC and WebP profiles decoded successfully; exit 0
```

No PowerShell, MSVC, Windows SDK or Windows kernel is available here. The console
and worker changes are therefore authored but uncompiled and unrun on Windows. A
native gate still has to judge both light and dark 16-color palettes, legacy
console cursor redraw, viewer close/reopen, suffix labels in a real job, and the
short timestamps on a machine whose local zone is not UTC.

| Edge case | Contract |
|---|---|
| Concurrent jobs with colliding suffixes | `two live jobs with the same four characters stay distinguishable` |
| Retry then completion with one suffix | `a repeated stage is a visible retry with one stable suffix` |
| Before/after restart with no counter | `restart needs no counter and never implies continuation` |
| Message too long for the width | `every rendered line fits eighty columns including an impossible message` |
| Midnight and month boundary | `local timestamp format is exact through midnight month and zone changes` |
| DST and non-UTC local zone | same timestamp contract with fixed and DST POSIX zones |
| Many jobs and grep resolution | `many jobs keep suffix-to-id resolvability and concurrent accounting` |
| Old UUID/old-timestamp lines | `old durable lines still render plainly without breaking the viewer` |

<!-- Inherited hardware-branch narrative below is UNVERIFIED for tip 1edeada.
No Windows evidence here establishes this merged tree or current hardware support. -->
## Adaptive hardware correction 2 (October 17, 2026)

The first global-budget regression linked the production `src/adaptive_hardware.cpp`
and failed before the scheduler change. Four offers each cost one CPU and 512 MiB;
the fourth was incorrectly rejected against the task's 512 MiB ceiling. After the
fix, four reservations are accepted under machine CPU4/RAM2048 and the fifth is
rejected. The focused adaptive executable passed 28/28 contracts, including two
synthetic NVIDIA fixture devices, ordinal reordering, PID/LUID telemetry ambiguity,
task/global budgets, live per-device headroom, codec fallback, output cleanup and
drain. These fixtures are not physical GPU claims.

The portable `scripts/test.sh` run passed 5/5 CTest suites. On the reachable
Windows PC, MSVC 19.44 Release with parallel 2 and CTest passed 8/8. The isolated
Doctor used the installed approved FFmpeg and detected the Radeon RX Vega M GL
(4,043 MiB dedicated) and Intel HD Graphics 630 (128 MiB). It enabled one AMD AMF
HEVC profile after the 640x360 synthetic encode, decode, child PID/LUID dedicated
memory sample and repeated tuning. The observed rates are sample-specific; quality
parity is unmeasured. AMD AMF H.264 was withheld below the 5% CPU throughput floor.
Intel QSV ran but had no nonzero dedicated-memory sample; NVIDIA was absent.
Runtime sends inputs outside the tested 640x360 envelope to CPU without scaling.

Before Doctor and smoke, the isolated script checked worker PID 28136 present,
zero unmatched START records and zero live FFmpeg processes. The synthetic native
pipeline smoke passed serial/overlapped H.264, HEVC and WebP decode, observed
overlap, matched artifact bytes and SHA-256, and validated a single CPU retry
before upload observation. Its 500 ms per-artifact transfer delay was simulated;
it did not contact storage or the coordinator. No live config, service, pairing,
startup or job state changed. Exact executed logs are in
`/home/rick/.hermes/cache/scratch/worker-adaptive-hardware-implementation-r2.md`.


## Operator truthfulness and probe visibility — October 17, 2026

See [the audit and gate ledger](../verification/truthfulness/REPORT.md) for the
findings, exact commands and Windows checklist. The portable gate passed 5/5
suites: 75 core, 9 pipeline, 3 rendition, 48 adaptive and 28 progress/console
contracts (163 total). Core/adaptive and a separate strict progress build also
passed ASan/UBSan with leak detection. No MSVC/native/PowerShell or physical GPU
check ran; these are source changes with Linux evidence only. Work stayed on
`feat/gpu-on-logs`; nothing was pushed, installed, deployed or restarted.

Probe output means startup sample evidence, not production success or arbitrary
input support. Heartbeat `verified` retains that protocol meaning; app video
reporting also requires current CPU H.264/HEVC fallback profiles. Runtime hardware
failure suppression affects the reporting ledger only, leaving device selection,
resource policy and scheduler behavior unchanged.


## GPU dimensions correction — October 17, 2026

The current gate ledger is [verification/gpu-dimensions/REPORT.md](../verification/gpu-dimensions/REPORT.md).
The optional installed-FFmpeg smoke checks the actual 1920x1080 tuning filter,
CPU rendition command factory and shared strict tuning-output validator:

```sh
python3 tests/adaptive_probe_smoke.py --evidence-probe build-tests/ow_adaptive_evidence_probe --rendition-probe build-tests/ow_rendition_probe
```

It validates both H.264 and HEVC, complete 72-frame output, AAC audio and geometry
after real encode/decode. It does not measure hardware performance or verify the
approved Windows binary. Portable contracts cover small-sample/large-input
admission, unsupported geometry, comparable measurement resolution, saved evidence,
large encode failure, atomic codec-specific demotion, capacity retention, CPU retry
and the short console reasons. Native MSVC and actual GPU/job gates remain required.
