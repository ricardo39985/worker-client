# Reservation-drain investigation — October 17, 2026

This is an evidence/test commit, **not a production drain fix**. The supplied
Windows failure does not demonstrate an early process-tree drain. No production
source or existing Windows assertion was changed. No push, installation,
deployment, startup change or worker restart occurred.

## Reproduced cause of the reported assertion

In `tests/windows_test.cpp`, line 81 rejects a second reservation **before**
line 82 constructs the first native child. No cancellation, process termination,
or drain has occurred at the failure. Linux does not register `windows_contracts`,
so a passing Linux CTest run never exercised this assertion.

`repro.cpp` copies the exact fixture and assertion into a portable executable
linked against production `src/adaptive_hardware.cpp`. It fails on Linux too.
The second diagnostic proves why: the first job selects NVENC, retaining its
512 MiB GPU reservation, while the second selects the independent CPU profile.
Both jobs together cost 2 CPU threads, 512 MiB RAM, 256 MiB scratch and 512 MiB
VRAM, within machine limits 2/1024/512/512 and max_jobs 4. The GPU profile's
concurrency of one remains enforced. This matches the existing portable contract
`runtime scheduler atomically enforces global and per-device costs until drain`,
which explicitly permits independent CPU work while GPU capacity is reserved.

Making the original assertion pass by blocking all same-codec CPU admission
would contradict that existing behavior. Changing the fixture would require
correcting the native test rather than fixing a demonstrated production drain
bug. The requested existing test was left byte-for-byte unchanged.

## Existing release gate and limits of this investigation

Source review finds that `Child::stop()` calls `TerminateJobObject`, then polls
`JobObjectBasicAccountingInformation` until `ActiveProcesses == 0` before closing
containment. A query failure terminates the controller rather than returning
capacity. The root process exiting alone does not satisfy this gate.

In `Worker::execute`, cancellation, timeout and lease-loss throw out of the
encoder scope; encoder failure does the same before CPU retry. `Child` destruction
calls `stop()` on all those paths. The attempt's adaptive reservation remains in
`TaskData` through pipeline unwinding, completion and watchdog cleanup. This is
source reasoning, not verified Windows runtime evidence. No premature native
release was established and no production fix is claimed.

Two added portable contracts (34 adaptive contracts total) prove the exact
fixture's independent CPU admission without losing GPU ownership, plus held
budget/move/release/re-admission for CPU, NVENC, QSV and AMF. These use synthetic
profiles, not hardware, native process trees or simulated proof of OS drain.
They passed immediately against the existing implementation. The only executed
red is the diagnostic reproducing the original assertion; this is not a
production red/green fix.

## Executed gates

All commands ran at repository root using the existing reviewed Boost source;
no dependency was installed or upgraded. Logs are beside this report.

| Command | Exit | Counts/result |
| --- | ---: | --- |
| `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp verification/reservation-drain/repro.cpp src/adaptive_hardware.cpp -o .cache/reservation-drain/repro` | 0 | One diagnostic binary compiled |
| `.cache/reservation-drain/repro > verification/reservation-drain/repro.log 2>&1` | 1 (expected) | 2 cases: copied assertion fails, capacity-accounting proof passes |
| `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 > .cache/reservation-drain/release.log 2>&1` | 0 | Configure/build/CTest successful, 5/5 suites |
| `ctest --test-dir build-tests --output-on-failure -V > .cache/reservation-drain/release-contracts.log 2>&1` | 0 | 5/5 suites, 138/138 contracts: 75 core, 9 pipeline, 3 rendition, 34 adaptive, 17 progress/integration |
| `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined > .cache/reservation-drain/sanitize-configure.log 2>&1` | 0 | One Debug configuration; standalone suites instrumented too |
| `cmake --build build-sanitize --parallel 2 > .cache/reservation-drain/sanitize-build.log 2>&1` | 0 | All portable targets built |
| `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure -V > .cache/reservation-drain/sanitize-ctest.log 2>&1` | 0 | 5/5 suites, 138/138 contracts, same breakdown; no reported sanitizer/leak errors |
| `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe > .cache/reservation-drain/media-smoke.log 2>&1` | 0 | 3/3 synthetic H.264, HEVC and WebP formats decoded; Linux CPU FFmpeg |
| `git diff --check` | 0 | Zero whitespace errors |
| `git diff --cached --check` | 0 | Zero staged whitespace errors |

## Still unresolved / real Windows requirements

The reported Windows assertion still fails by construction. There is no native
green claim and this commit does not meet the brief's requested production-fix
acceptance. A native drain reproduction must actually reach child creation and
cancellation, exhaust the resource being asserted, and observe admission relative
to complete descendant exit. The operator should supply a failure after that
point if a separate premature-drain bug exists.

MSVC x64 compilation and all 8 native CTest suites remain unrun here. Native
cancellation, timeout, encoder crash and lease-loss must demonstrate retained
capacity until every descendant exits and successful re-admission afterward,
using CPU and each available successfully probed GPU backend. Approved-FFmpeg
native media smoke and readable-console/logging checks remain Windows gates.
Previously documented independent fallback-ownership defects in
`verification/gpu-on-logs/REPORT.md` are outside this investigation and remain
unresolved. No release readiness is claimed.
