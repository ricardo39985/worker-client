# NVENC retry landing review and gates

Reviewed original revision: `656ae4ad23650be5cfa345bbc552f4b5ff7d4309`.
Final application/test revision: `3bc6d27e60202ba2be6a9a33b31997bf6e5993f0`.
Base: `46ab545bf6d07abe6848a786f6b1e19e9d33c320`.

## Review outcome

One genuine build defect was found by the real MSVC gate: the new
`windows/probe_recovery.cpp` translation unit omitted `ow/device_identity.hpp`,
which declares `profile_vram_capacity` and `schedulable_device_count`. The
original native configure exited 0, build exited 1 (C2039/C3861 and cascading
errors), SSH/helper exited 1 and zero native suites ran. Commit `3bc6d27` adds
that single include. The failing native compilation supplies the RED evidence;
the unchanged native build/suites supply the corrected gate. No behavioral or
test change was necessary. All Linux/sanitizer gates were repeated after the fix.

No unsafe admission, unbounded immediate retry, busy retry loop or reservation
leak was found in the retry logic. The relevant paths were checked as follows:

- `run_tuning` supplies successful evidence only after repeated real encode,
  decode, strict codec/geometry/frame/audio validation and bounded tuning. Missing
  or failed device/codec evidence cannot become a schedulable profile. Retention
  accepts only current successful in-process profiles, checking physical identity,
  mapping/ordinal, driver, approved executable hash and capacity; diagnostic JSON
  never supplies admission proof. Recovery rechecks inventory/hash/completion and
  the controller's invalidation generation before replacing the scheduler.
- Immediate retry is limited to three attempts with 1000/2000 ms waits and
  cancellation checks. The recovery observer sleeps between polls; failed epochs
  back off 30/60/120/240/300 seconds, remaining capped at five minutes. Busy,
  paused or exiting controllers do not launch recovery work.
- Admission enters probe maintenance atomically only when no accepted offer,
  running job or cancelling job retains resources. The adaptive scheduler must
  also have zero active reservations. Maintenance rejects new offers, remains
  independent of operator pause and is released by an exception-safe guard.
  Contained children drain before an attempt returns or throws. Recovery never
  swaps a scheduler holding job reservations. Existing session, per-card VRAM,
  aggregate RAM/CPU/scratch and CPU-fallback accounting remain in force.

## Execution scope

Linux work used an isolated worktree because another task changed the shared
checkout's branch during review. Windows work follows the prior
`verification/windows-gates/run-windows-gate.ps1` procedure: Git bundle of the
exact revision, existing MinGit/MSVC/CMake and reviewed Boost 1.89.0, Release x64,
parallel 2, CTest JSON discovery and every discovered suite run individually.
Numeric native exit codes are captured immediately after each invocation.

Linux uses the existing reviewed Boost 1.89.0 tree at
`/tmp/ow-boost.mxOndo/boost_1_89_0`. Sanitizers use Debug, IPO off,
`OW_SANITIZE=ON`, plus global address/undefined sanitizer compile/link flags so
all five C++ contract executables are instrumented. `ASAN_OPTIONS` is
`detect_leaks=1:halt_on_error=1`; `UBSAN_OPTIONS` is `halt_on_error=1`.
The 23 Python dependency cases run normally; all 216 C++ cases are instrumented.

No test was changed, weakened, disabled or skipped. Native CTest discovery may
omit the optional Python dependency suite when Python is unavailable, matching
that suite's existing CMake registration and the prior Windows gate. The same
23 dependency cases pass on Linux in both configurations.

This is isolated build/behavioral verification. It does not establish physical
NVENC recovery, real two-card encoding, production job behavior or installation.
No installer, Doctor, real media job, account/coordinator request, startup change,
production restart, deployment, migration, Lightsail or Cloudflare operation ran.
The user authorized merge/push only after all gates pass.

## Gate results

| Suite | Linux Release passed / failed; CTest exit | Sanitizer passed / failed; CTest exit | Windows Release passed / failed; CTest exit |
|---|---|---|---|
| ffmpeg_dependency_contracts | 23 / 0; exit 0 | 23 / 0; exit 0 | not registered (Python unavailable) |
| worker_contracts | 77 / 0; exit 0 | 77 / 0; exit 0 | 77 / 0; exit 0 |
| pipeline_contracts | 9 / 0; exit 0 | 9 / 0; exit 0 | 9 / 0; exit 0 |
| rendition_contracts | 3 / 0; exit 0 | 3 / 0; exit 0 | 3 / 0; exit 0 |
| adaptive_hardware_contracts | 74 / 0; exit 0 | 74 / 0; exit 0 | 74 / 0; exit 0 |
| progress_contracts | 53 / 0; exit 0 | 53 / 0; exit 0 | 53 / 0; exit 0 |
| windows_contracts | n/a | n/a | 12 / 0; exit 0 |
| setup_contracts | n/a | n/a | 25 / 0; exit 0 |
| setup_progress_contracts | n/a | n/a | 15 / 0; exit 0 |
| setup_converter_contracts | n/a | n/a | 17 / 0; exit 0 |
| Total | **239 / 0; 6/6 suites** | **239 / 0; 6/6 suites** | **285 / 0; 9/9 suites** |

Final Linux configure/build exits: **0/0**, in both Release and sanitizer builds.
Final Windows configure/build/discovery exits: **0/0/0**; helper/SSH exits:
**0/0**. Converter suite completed within its unchanged 60-second timeout in
26.41 seconds. No failed final gate was discarded or omitted.

The original `656ae4a` also passed Linux Release and sanitizer suites (239/239
contracts each; configure/build and all six individual CTest exits 0). Its
Windows build failed with exit **1**, configure **0**, helper/SSH **1/1**, and
**0 native suites executed**. `windows-red.log` preserves that failure. All
final gates above were then repeated on the corrected `3bc6d27` revision.

`git diff --check` exited 0. The evidence commit adds only this ledger, logs
and reproduction helpers; application/test sources match the final tested
revision. Local main and freshly fetched origin/main were both `46ab545`
before landing, so the merge introduces no untested application combination.

Captured logs normalize trailing whitespace only.

Files: `linux-gates.log`, `sanitize-gates.log`, `linux-results.json`,
`baseline-linux-results.json`, `windows-red.log`, `windows-green.log`,
`run-linux.py`, `run-windows-gate.ps1` and `run-windows-baseline.ps1`.
The Linux helper uses the gate's temporary directory paths; a reproduction can
choose new output/build directories without changing any test settings.
Windows helper reuses only its own isolated baseline source/build directory:
`C:\Users\reald\worker-nvenc-gates-656ae4a-20261018`.
