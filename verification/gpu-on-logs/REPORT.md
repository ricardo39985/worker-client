# GPU-on-logs local merge review — October 17, 2026

Merge parents: logging `999a4f6cc83b29006651b762afce7285ca53f1a9` and hardware
`1edeadacae21953ef995d8a0079395d5d4ad0487`. The merge commit SHA is reported in
Git and the handoff reply (a commit cannot contain its own SHA).

This is a Linux-tested source merge, not Windows or production acceptance.
No push, installation, deployment, startup change, account contact or worker
restart was performed. Local hardware tip identity was verified. There was no
`origin/feat/adaptive-hardware` remote-tracking ref in this checkout; no fetch was
needed to merge the exact requested local tip.

## Conflict decisions

| File | Combined resolution | Naive side choice would lose |
|---|---|---|
| `CMakeLists.txt` | Register the hardware suite and retain the logging suite's shared test main/include paths; add two encoder integration contracts to that suite. | Ours loses hardware contracts; theirs uses the obsolete standalone progress test entry point and fails to build the current logging suite. |
| `windows/worker.hpp` | Retain `ProgressLogPolicy`, logging deduplication state, hardware profiles/reservations/scheduler, and all three worker threads. | Ours loses hardware state/hash watchdog; theirs loses progress policy, capability/activity deduplication and reconnect presentation state. |
| `windows/worker.cpp` | Keep both subject/backend helpers; combine hardware reservation and durable declined-BEGIN handling with full attempt identity; initialize hardware scheduler after probes while emitting the short logging records; apply measured progress to each selected hardware/CPU invocation with a fresh stage generation and parser. | Ours loses hardware assignment, runtime profile state and CPU retry orchestration; theirs loses timeline progress, short probe/activity output, job subjects, retry generations and logging-policy updates. |
| `docs/status.md` | Retain both development histories and prepend the current unverified-hardware boundary and merge evidence pointer. | Either side alone removes the other's history; taking hardware assertions on trust misrepresents Windows verification. |
| `docs/testing.md` | Retain the unchanged logging contract history plus hardware test history, with current evidence and unverified inherited claims clearly distinguished. | Ours loses hardware testing context; theirs loses logging regression evidence and acceptance details. |

The console renderer, progress/parser policy, console executable, durable logger
and original `tests/progress_test.cpp` are byte-for-byte unchanged from the logging
parent. The encoder log-file tailer was extracted into `ow/encoder_run.hpp` so
both hardware and CPU invocations use the same behavior-tested reader. Output
bytes still enforce limits only. Every retry begins a new counter generation,
parser and file cursor; transfer counters remain separate. The GPU-aware command
factory and all 32 hardware contracts are retained unchanged. Long new per-job
hardware commentary was shortened to full-ID job records, which the existing
viewer renders with its suffix labels. Probe loops no longer print repeated
unknown percentages. Detailed hardware probe evidence remains in the candidate's
`adaptive-hardware.json`; the viewer retains main's CPU capability summary (CPU
fallback readiness, not proof that every encode runs on CPU).

## Executed gates

Commands below ran at repository root. All builds used the existing, reviewed
Boost source at `/tmp/ow-boost.mxOndo/boost_1_89_0`; no dependency installation or
upgrade occurred. Its archive `/tmp/boost_1_89_0.tar.gz` matched reviewed SHA-256
`9de758db755e8330a01d995b0a24d09798048400ac25c03fc5ea9be364b13c93`
(`sha256sum /tmp/boost_1_89_0.tar.gz`, exit 0, one matching archive).

1. Expected red:
   `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp tests/encoder_run_test.cpp -o .cache/encoder-run-red`
   — exit **1**, missing `ow/encoder_run.hpp`; zero cases executed. See
   `encoder-run-red.log`. This is a compile-red stage, not an executed test failure.
2. Focused green: same command with `-o .cache/encoder-run-green` — compile exit
   **0**; `.cache/encoder-run-green` — exit **0**, **2/2** contracts. One covers all
   three backend command plans with split real-format FFmpeg reports; the other
   covers four concurrent synthetic hardware failures followed by CPU retry at
   10%, rather than retaining hardware's 99%. See `encoder-run-green.log`.
3. Clean Release:
   `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0`
   — exit **0**; configure/build/ctest each **0**, **5/5** suites, **136/136** contracts:
   **75 core, 9 pipeline, 3 rendition, 32 adaptive, 17 progress/integration**.
   This retains all **15 original logging contracts unchanged** and adds two.
   See `release.log` and `release-contracts.log`.
4. Sanitizer configure:
   `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0 -DCMAKE_CXX_FLAGS=-fsanitize=address,undefined`
   — exit **0**, one fresh Debug build configured. The extra compiler flags
   instrument standalone suites too; OW_SANITIZE alone propagates through core.
5. Sanitizer build: `cmake --build build-sanitize --parallel 2` — exit **0**,
   all targets built; see `sanitize-build.log`.
6. `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure`
   — exit **0**, **5/5** suites, **136/136** contracts, same breakdown as Release;
   no reported ASan/UBSan/leak errors. See `sanitize-ctest.log` and
   `sanitize-contracts.log`.
7. `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe`
   — exit **0**, **3/3** synthetic profile formats (H.264, HEVC, WebP) decoded;
   Linux CPU FFmpeg, not a GPU or Windows encode. See `media-smoke.log`.
8. `git diff --check` and `git diff --cached --check` — each exit **0**,
   **0** whitespace errors. Final staged/merge-tree checks are repeated before commit.

No unchanged logging contract was edited or weakened. The clean builds started
without existing build caches and compiled all portable targets.

## Hardware findings left open, not silently repaired

These defects are inherited from tip `1edeada`, not introduced by conflict
resolution. Fixes require their own contract-first hardware work. The following
failure diagnostics are deliberately separate from the passing acceptance suites.

**High: primary CPU admission ignores running fallback occupancy.**
`Scheduler::reserve` checks `primary_jobs` but not `fallback_busy`. With CPU profile
concurrency one, a GPU job already running its CPU retry does not prevent a new
CPU-primary job from reserving that same slot. This exceeds the measured encoder
concurrency even though global resource totals can still fit.

**High: release clears CPU fallback occupancy without ownership.**
`Scheduler::release` clears `fallback_busy` for every CPU profile on release,
including a GPU reservation which never acquired that slot. Releasing a waiting
job lets a third job acquire a CPU fallback while the first owner is still running.
The reservation move operations also omit `cpu_fallback_keys_`; the eventual fix
must preserve ownership through moves as well as release.

Both safety contracts fail against the exact original hardware implementation:

```sh
git show feat/adaptive-hardware:src/adaptive_hardware.cpp > .cache/original-adaptive.cpp
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -Iinclude -Itests tests/main.cpp verification/gpu-on-logs/hardware_findings.cpp .cache/original-adaptive.cpp -o .cache/hardware-findings-original
.cache/hardware-findings-original
```

Extraction and compilation exit **0**; diagnostic execution exits **1**, **2/2
safety contracts fail**, as recorded in `hardware-findings.log`. The same initial
`.cache/hardware-findings.cpp` diagnostic compiled against the merged
`src/adaptive_hardware.cpp` with identical flags (output `.cache/hardware-findings`):
compile **0**, execution **1**, the same **2/2 failures**. These failures are
reported findings, not skipped or passing tests.

**High, source-reviewed: dimension-based CPU fallback bypasses slot acquisition.**
In `Worker::execute`, removing hardware profiles for unknown/oversized dimensions
makes `rendition.hardware` false. `convert` then directly runs the CPU arguments;
only the hardware-failure path calls `acquire_cpu_fallback`. Multiple GPU-admitted
jobs can therefore exceed the measured CPU concurrency when their inputs exceed
the 640x360 envelope. Needs a runtime behavior contract and correction.

**High, source-reviewed: regular GPU encodes lack child GPU-memory enforcement.**
The probe loop samples the child PID and aborts on its VRAM cap. Regular
`run_encoder` checks cancellation/time/output/log size only; BEGIN samples
adapter headroom, but no ordinary child PID is monitored against the reserved
profile VRAM. A 640x360 sample is not a hard limit for other content, frame rates
or driver behavior. Native watchdog/envelope enforcement remains unfinished;
Windows Job Objects do not impose a GPU-memory quota here.

**Availability, source-reviewed: valid zero/idle memory counters disable GPU use.**
`gpu_memory_totals` rejects zero totals and the PDH adapter rejects zero records;
preflight cannot distinguish a genuinely idle device from unusable telemetry.
This can withhold otherwise usable GPUs. Preserve fail-closed behavior while
adding a verified idle-device telemetry contract before changing that policy.

**Evidence/readiness: inherited Windows narratives conflict with the brief.**
The hardware branch includes older Windows pass narratives and artifacts, while
its final R3 section says native verification is pending. The supplied task
explicitly designates tip `1edeada` as unverified; no earlier artifact establishes
that exact source or this merge. Treat all those claims as unverified provenance,
not fresh execution evidence. The docs now state this at the top.

## Still required on the real Windows machine

- MSVC x64 compile/link of this exact merge, Windows SDK and pinned dependencies;
  all **8 registered CTest suites**, including native process-tree, log rotation,
  both PowerShell suites, inventory and synthetic runtime pipeline smoke.
- Approved FFmpeg doctor: actual NVIDIA/Intel/AMD mapping, driver/codec probes,
  explicit adapter assignment, child PID/LUID counters, missing/idle telemetry,
  configured zero/positive VRAM ceilings, invalidation and reprobe behavior.
- Real hardware H.264/HEVC encode/decode, output dimensions/frame timing/audio,
  quality comparison, resource bounds, and honest timeline progress. Unsupported
  inputs must take CPU fallback safely, without bypassing its concurrency limit.
- Four concurrent jobs, CPU slot contention, retries, cancellation, lease expiry,
  pause including reserved offers, drain/stop, process-tree termination, and
  matching durable result ACK/restart/fencing behavior with a staging coordinator.
- Real console on light/dark palettes and legacy terminal, redirected output,
  `NO_COLOR`, non-UTC timestamps, suffix collisions, viewer close/reopen while jobs
  continue, and plain rotating durable logs.

Those checks have not run here. Reproduced hardware defects must be corrected
before treating this branch as trusted or suitable for installation/release.
