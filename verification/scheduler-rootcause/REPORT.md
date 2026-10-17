# Scheduler admission root cause — October 17, 2026

Local source/test correction only. No push, installation, deployment, restart,
software installation, or contact with a Windows host occurred. MSVC is unavailable.

## Mechanism established before editing

The brief's assertion that the identical sequence passes on GCC is contradicted
by a fresh optimized GCC reproduction. The exact fixture and CHECK from
`verification/reservation-drain/repro.cpp`, linked to production
`src/adaptive_hardware.cpp`, produce the same assertion failure: 2 diagnostic
contracts, 1 failure (exit 1). The second diagnostic proves that the admitted job
selects CPU and the first still owns NVENC. See `exact-red.log`.

This is a test-policy mismatch, not an established Windows-only compiler defect.
Linux CTest does not register `tests/windows_test.cpp` (CMakeLists.txt:79), so its
green result never exercised the erroneous native assertion. The portable
adaptive suite already explicitly permits independent CPU admission.

The branch calculation is exact:

1. `src/adaptive_hardware.cpp:217` orders NVENC (50 fps) ahead of CPU (20).
   Lines 225–230 add the CPU retry profile to that GPU job's reservation.
2. Lines 140–149 take peak CPU/RAM/scratch for a codec, retaining GPU VRAM.
   The held job costs CPU1/RAM256/scratch128/VRAM512.
3. Line 249 increments NVENC primary occupancy, but not CPU primary occupancy
   when CPU is only fallback. On the second call line 214 excludes the full
   NVENC profile, while line 213 retains the unused CPU primary profile.
4. Lines 224–237 admit CPU alone. Combined totals are CPU2/RAM512/scratch256/
   VRAM512, within limits CPU2/RAM1024/scratch512/VRAM512/max_jobs4.
   A third job is rejected with both reservations held. No GPU budget was freed.

The observed Windows TRUE is consistent with exactly this branch, which is
executed and behavior-checked on GCC. We cannot directly observe MSVC's selected
profile here. The corrected native test now asserts the selected profiles; if
it disagrees, capture the actual profile backend/stable_id, jobs and projected
costs at lines 213–237 using that exact binary/source revision. There is no
source evidence for a distinct GCC-refusal mechanism to fix.

## Alternative mechanisms ruled out for this sequence

- Aggregate order: `include/ow/adaptive_hardware.hpp:44–46` is CPU, RAM, VRAM,
  scratch, max_jobs; lines 55–65 define the Profile fields exactly as initialized.
  Its fourteen initializers end at backend_identity_verified=true; omitted
  max_width/max_height initialize to zero. `reserve` never checks dimensions.
- Evaluation order: the two reserve calls are separate full expressions; the
  braced members are in declaration order. There are no competing side effects
  in function arguments, and no platform conditionals in this admission path.
- Lifetime: lines 157–165 transfer active ownership on moves and deactivate the
  moved-from reservation. The named held object is still alive, with jobs=1
  before the second call. A temporary second reservation is destroyed only after
  that CHECK's expression; it cannot undo the already-observed TRUE or release
  the held GPU reservation. The added portable contract exercises this exact
  temporary/move/backend separation, without acquiring any CPU fallback slot.
- `tests/test.hpp:13` evaluates CHECK's expression once; the other occurrence
  is stringification, with no evaluation.
- All scheduler counters/maps are in its per-instance shared State
  (`src/adaptive_hardware.cpp:126–135,152–153`), protected by its mutex. No static
  or thread-local scheduler accounting participates.

## Contract and correction

For worker-local adaptive H.264 scheduling, a second job may reserve an available
primary backend only when its per-task cost, the combined machine budgets and
that backend's concurrency/device budgets fit while retaining every reservation
of the cancelled job until its entire process tree has drained; a GPU job's
reserved CPU fallback budgets do not themselves occupy a CPU encoder slot.

That sentence is now in `contracts/worker-v1.md:155`. Production scheduler code
is unchanged: its admission already implements the relevant policy for this
sequence. The failing observable test was reproduced before editing, rather
than inventing a production change to make it pass.

`tests/windows_test.cpp:76–102` now asserts GPU first, independent CPU second,
and rejection of a third reservation before and during asynchronous native stop,
then after stop until explicit GPU release. It verifies both root and descendant
handles signaled before releasing the held reservation. With independent CPU
still held, the next accepted job must select GPU. This preserves the safety
property instead of merely deleting the erroneous assertion or renaming it.
The polling loop is not a guarantee that it samples the short drain interval
on every run; pre-stop and post-stop ownership checks are deterministic.

The additional portable contract at `tests/adaptive_hardware_test.cpp:209`
pins temporary CPU destruction, moving the GPU reservation, denial with both
backends occupied, and GPU-only re-admission after release. It passed immediately
against production source: this is coverage for existing behavior, not a claimed
production red/green repair. The native contract's red is the operator's observed
failure; its corrected green remains an unrun Windows gate.

## Executed gates

Commands ran at repository root with existing reviewed Boost source, no upgrade.
Redirected logs named below are checked in beside this report.

| Command | Exit | Counts/result |
| --- | ---: | --- |
| `g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp verification/reservation-drain/repro.cpp src/adaptive_hardware.cpp -o .cache/scheduler-rootcause/repro` | 0 | One diagnostic compiled before edits |
| `.cache/scheduler-rootcause/repro > verification/scheduler-rootcause/exact-red.log 2>&1` | 1 expected | 2 contracts, 1 failure on the exact assertion, independent CPU proof passes |
| `g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp tests/adaptive_hardware_test.cpp src/adaptive_hardware.cpp -o .cache/scheduler-rootcause/adaptive` | 0 | One focused binary compiled |
| `.cache/scheduler-rootcause/adaptive > verification/scheduler-rootcause/focused.log 2>&1` | 0 | 35/35 adaptive contracts |
| `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 > verification/scheduler-rootcause/release.log 2>&1` | 0 | Configure/build/CTest successful, 5/5 suites |
| `ctest --test-dir build-tests --output-on-failure -V > verification/scheduler-rootcause/release-contracts.log 2>&1` | 0 | 139/139 contracts: 75 core, 9 pipeline, 3 rendition, 35 adaptive, 17 progress/integration; 5/5 suites |
| `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined > verification/scheduler-rootcause/sanitize-configure.log 2>&1` | 0 | One Debug configuration, standalone suites instrumented too |
| `cmake --build build-sanitize --parallel 2 > verification/scheduler-rootcause/sanitize-build.log 2>&1` | 0 | All portable targets built |
| `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure -V > verification/scheduler-rootcause/sanitize-contracts.log 2>&1` | 0 | 5/5 suites, 139/139 contracts, same breakdown; no reported sanitizer/leak errors |
| `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe > verification/scheduler-rootcause/media-smoke.log 2>&1` | 0 | 3/3 synthetic H.264/HEVC/WebP formats decoded with Linux CPU FFmpeg |
| `git diff --check` | 0 | Zero whitespace errors |
| `git diff --cached --check` | 0 | Zero staged whitespace errors |

## Explicit Windows prediction and limits

After an MSVC Release rebuild of this revision, I predict the corrected
`ow_windows_tests.exe` reports **12 contracts, 0 failures**. Specifically, it
selects NVENC first, CPU second, rejects the third while both are held, retains
jobs=2 through native stop, observes both process handles signaled, and selects
NVENC again only after held.release(). The newly added portable mechanism test
should pass in Windows's adaptive executable too (35 contracts, 0 failures).
The original unmodified two-call rejection assertion would still fail on both
platforms; no production admission semantics were changed to force it green.

All 8 registered Windows CTest suites are expected to remain green, but have not
been compiled or run here, including the 12 native contracts and PowerShell
suites. A native rebuild/CTest and approved-runtime media/logging checks remain
operator gates. Linux smoke is not Windows/GPU evidence. The previously recorded
fallback ownership, dimension fallback and runtime VRAM-watchdog findings remain
outside this small admission-test correction; this is not release readiness.
