# Test policy and evidence

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

On Windows, setup.cmd builds Release and runs all five registered suites before
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

Required: Windows syntax checks, both PowerShell suites, all five registered CTest
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



## CPU embedding candidate verification — 2026-10-08

Protected contracts: independent conversion/inference offers and shared live
reservation budget; one model lane including probes; cancellation retains
capacity until child shutdown; typed immutable profile/input/output; copied
output verification and source/lease/deletion fencing; durable embedding/ACK;
prebuilt integrity and rollback with retained identity/journal.

The initial red stages rejected the missing embedding capability and exposed
single-lane dispatch, recursive/unsafe publication failures on changed source
or wrong modality, malformed capability acceptance and overlapping inference
probes. Portable green evidence: 76 client core contracts, standalone normalized
embedding/progress tests and three rendition-profile contracts. PowerShell 7.4
Linux parsed all scripts and ran pure bundle corruption/install/version-isolation/
rollback/backup-retention and injected Microsoft runtime bootstrap contracts.
Verified the downloaded Windows runtime ZIP hash and PE import tables; Microsoft
redistributable URL/hash come from Microsoft winget-pkgs blob
6cdab74037d685cf44e9f811043cb9fc3a44cb80. Runtime install, Authenticode and UAC
execution are native acceptance items. These do not verify Windows startup, DPAPI,
MSVC or Windows PowerShell 5.1. Full existing native setup suites remain required.

Real b11475/Q8_0 CPU probes passed: distinct text, image content and audio tones,
plus four image parts, all 768 finite normalized values. Fixed maximum sample
size 256x256, four frames and ten-second audio with encoder token/batch limits
256 kept measured child peak RSS at 1,576,472 KiB (about 1.5 GiB). Text-only
without projector peaked at 441,044 KiB (about 431 MiB). These are authoring CPU
measurements, not laptop/Windows performance promises. Larger experimental
media batches exceeded the original 2 GB declaration, motivating bounded samples.

Reproduce on a prepared Linux authoring environment with explicit local binaries
and verified downloaded model directory (no automatic SDK/model downloads):

```sh
python3 tests/embedding_model_smoke.py --server /absolute/llama-server --model-directory /absolute/pinned-models
python3 tests/embedding_model_smoke.py --server /absolute/llama-server --model-directory /absolute/pinned-models --text-only
```

Native acceptance still required: MSVC Release compile and all CTest suites;
Windows PowerShell 5.1 parsing/setup; CPU doctor on real Windows; prebuilt update
on a machine without build tools/Boost; damaged-bundle/cache repair; failed
launch rollback; independent live text/media + conversion jobs, cancellation,
reconnect and result publication. No binary release, merge or deployment was
performed by this candidate.
