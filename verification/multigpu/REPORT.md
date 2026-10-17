# Physical GPU placement — source candidate, 17 October 2026

Branch: `fix/physical-gpu-placement`, based on `d1b9fea`. Local commit only.
The full operator brief was read before changes. Neither Windows machine nor
the production server was contacted. No push, merge, release, installation,
deployment, startup change or restart was performed.

## Diagnosis before changes

The requirement is reasonable for two independently verified cards. The evidence
does not establish that both cards currently have working profiles for both
codecs, or that a scheduler-wide single-GPU lock exists.

1. **Three DXGI entries do not mean three physical encoders.** At baseline,
   `windows/hardware.cpp:24-28` builds `pci:vendor:device:subsystem:revision:luid:...`.
   The PCI portion is a product description, not a PCI bus address or a unique
   physical-card identifier. Lines 82-102 enumerate every eligible DXGI entry,
   reject software/unsupported-vendor entries, and append the remainder. There
   is no Meta Virtual Monitor attribution or physical deduplication. A virtual
   display/driver alias is plausible, but neither this code nor the brief proves
   which driver created the second LUID. Blaming Meta as a fact would be unsound.
   Removing it is neither necessary nor authorized here.

   NVIDIA mapping at baseline lines 99-100 and
   `src/adaptive_hardware.cpp:120-129` requires a unique CUDA LUID/ordinal match.
   The unmappable third entry is correctly withheld. It must never become
   another schedulable card. Equally, collapsing *all* equal PCI product IDs
   would incorrectly collapse two real identical cards.

2. **The scheduler already admits two cards concurrently.** These baseline
   quotations are from `d1b9fea:src/adaptive_hardware.cpp`:

   ```cpp
   // line 190: per-device AND per-codec primary counter
   return p.stable_id+":"+std::to_string(static_cast<int>(p.codec));
   // line 207: per-device VRAM capacity, not one GPU pool
   state_->device_caps[p.stable_id]=std::max(state_->device_caps[p.stable_id],p.vram_mb*p.max_concurrency);
   // line 258: independently configured machine job ceiling
   if(!s->limits.max_jobs||s->jobs>=s->limits.max_jobs)return {};
   // line 273: this profile's concurrency, not all GPUs' concurrency
   else if(s->primary_jobs[profile_key(p)]>=p.max_concurrency)continue;
   // line 294: VRAM admission independently by device id
   bool device_fit=true;for(const auto& [id,mb]:projected.devices)if(mb+s->device_vram[id]>s->device_caps[id])device_fit=false;
   ```

   Atomic reservations at lines 256-308 check task and aggregate CPU/RAM/scratch,
   global VRAM, device VRAM and configured job count. `max_concurrency=1` applies
   to that profile, not to the whole machine. The original two-NVIDIA contract
   and the new complete H.264/HEVC two-job contract pass before implementation,
   including when measured CPU fallback concurrency is one.

   The actual gaps are duplicate identity admission, cross-codec session
   accounting, and greedy codec placement. Baseline ranking (lines 276-278)
   independently chooses the fastest profile for each codec. When the cards
   trade places as codec speed winners, one rendition job can reserve both
   cards unnecessarily. This is reproduced in log 10. Baseline profile selection
   (lines 83-86) also counted *unmapped* entries when testing backend ordinal
   uniqueness, allowing an unproven alias to suppress a real mapping.

   On the supplied machine the 3070 H.264 probe failed and the extra 3070 LUID
   was unmappable. That evidence does not show two usable H.264 profiles.
   `encoders=cpu,nvenc` establishes an available backend family, not two cards
   or both codecs on each card. The brief omits the complete retained-profile
   matrix and active configuration, so it cannot establish whether codec
   eligibility, split placement, `max_jobs`, or other resource ceilings explain
   every observed run. Raising operator limits or inventing a successful 3070
   profile would be wrong.

3. **The allocation error is not a VRAM measurement.** The brief's roughly
   56 fps NVENC versus 24 fps CPU result establishes working hardware encoding
   on at least a sampled path. It does not explain another device's failed
   buffer allocation. Eight GB is declared capacity; free system RAM is a
   different resource. A driver/session limit, external encoders, resource
   pressure, or a driver/buffer fault remain possible.

   Startup GPU encodes are serial: baseline `windows/worker.cpp:203-204` starts
   with `tested_concurrency=1` and only explores parallel candidates for CPU.
   The NVENC command already uses zero lookahead; the bounded synthetic sample
   is 1920x1080/72 frames, not an unbounded buffer request. There is no evidence
   that the worker's own concurrent startup probes exhausted sessions. There
   is also no driver/session census proving session exhaustion. The supplied
   error therefore cannot determine session limit versus actual allocation
   failure; no buffer-quality changes or automatic retries were guessed here.

   NVIDIA documents a combined system-wide consumer-card session limit, not
   an independent allowance multiplied by each card. The current SDK 13.1
   note lists 12; this is not evidence of the installed driver's allowance.
   See [NVENC licensing policy](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.1/nvenc-application-note/index.html#nvenc-licensing-policy).

## Changes, in order

1. `windows/hardware.cpp`, `include/ow/device_identity.hpp`,
   `include/ow/adaptive_hardware.hpp`, and `src/adaptive_hardware.cpp` now retain
   the NVIDIA driver's nonzero physical UUID as well as the DXGI LUID and encoder
   ordinal. The installed system DLL is queried; no new dependency is installed.
   Missing UUID/mapping withholds NVIDIA. Repeated physical UUIDs, stable IDs or
   backend ordinals are ambiguous and withheld before tuning; profile selection
   and scheduler construction repeat the admission check. An already-unmapped
   alias cannot suppress a valid real mapping. Two identical NVIDIA products
   with different proven UUIDs remain independent. Non-CUDA repeated PCI products
   are conservatively withheld because their physical identity is not proven.
   [CUDA documents UUID and graphics-LUID queries](https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__DEVICE.html).

2. The scheduler reserves **one encode session per card across codecs** and
   **at most two NVENC sessions across this worker**. These are conservative
   worker policies, not discovered driver maxima. A sequential rendition job
   shares its card reservation across H.264 and HEVC. Other jobs cannot use
   that card until drain. Sorting prefers the already-chosen eligible card
   when all budgets fit, keeping another card available to another job. A
   third NVENC job can select CPU, subject to its own slot and resource budgets.
   Checks and updates occur under the existing reservation mutex.

   Per-card VRAM remains independent and uses the maximum sequential codec
   footprint. Automatic aggregate runtime VRAM capacity counts each card once,
   instead of summing its codec profiles. Unmapped/duplicate inventory contributes
   no probe GPU capacity. Current physical identity is checked again at BEGIN
   and before launch. Periodic identity invalidation now removes just stale
   profiles instead of disabling a healthy second card's scheduler profiles.
   Complete inventory failure still disables all hardware.

3. CPU retry remains bounded and cancellation-aware. Acquired CPU fallback
   ownership now survives reservation moves; releasing an unrelated reservation
   cannot clear another job's fallback slot. A new CPU primary cannot steal that
   occupied slot. GPU reservations still reserve CPU fallback resources without
   occupying CPU encoder slots in advance. Cancellation intent/demotion does
   not return CPU/RAM/scratch/VRAM/session capacity before process-tree drain.

4. `include/ow/adaptive_evidence.hpp` persists physical identity, device name and
   session policy. `windows/worker.cpp` and `include/ow/console.hpp` expose the
   actual child launch and completion, plus a card table/count. Example only:

   ```text
   schedulable GPUs=2
   nvenc GPU 0 | NVIDIA GeForce RTX 3070
   nvenc GPU 1 | NVIDIA GeForce RTX 3050
   job abcd nvenc/h264 GPU 0
   job efgh nvenc/h264 GPU 1
   job abcd nvenc/h264 GPU 0 done
   job efgh nvenc/h264 GPU 1 done
   ```

   Full durable launch records carry complete attempt ID, filename, child PID,
   full DXGI identity and physical UUID. They are emitted after child creation;
   `done` is emitted after child-tree drain. It means encode-process completion,
   **not** successful subsequent decode, upload or publication. CPU retry emits
   its own `cpu/h264` or `cpu/hevc` line. GPU ordinals map through the startup
   card table; raw DXGI inventory count remains separately labelled. Device
   counts are refreshed after demotion/invalidation. Suffixes, 80-column limit,
   plain redirected output, progress rows and the five color meanings remain.

## Deliberately retained boundaries

Real encode, decode, strict output validation, compatible dimensions and
throughput qualification remain prerequisites. Failed or unproven 3070 profiles
remain off; this code cannot make its failing probe pass remotely. No arbitrary
commands, driver patches, session-limit bypass, GPU memory-limit inflation,
FFmpeg buffer/preset change, dependency upgrade or operator job-limit change.
Leases/BEGIN, pause, reservations, process containment, outbox/replay and publication
fencing retain their contracts. No coordinator or deployment code was changed.

VRAM admission/telemetry are reservations and sampled watchdogs, not a Windows
hard GPU-memory quota. External applications can allocate memory or consume
driver-wide sessions after a check. No worker-local scheduler can atomically
reserve those external resources. This change prevents **worker-owned** session
oversubscription; driver refusal still drains the failed child, demotes the
device/codec and retries CPU. Actual two-session driver availability needs the
native gate below.

Concurrent dependency/setup edits appeared during the task. Their CMake,
README, THIRD_PARTY, lock, setup scripts, tests and verification files are preserved
and excluded from this commit. To separate that work from this result, the final
gates use a local `git archive d1b9fea` snapshot overlaid with only the ten owned
code/test files. `snapshot.json` records their hashes and source path. The
portable source/test files exactly match the working tree; the native test file
was also synchronized and hash checked. No extra Git branch was created.

## Gate ledger

All exit codes below are the gate's exit, not `tee` or a summary tool. Build
counts are zero executed tests. `B=/tmp/ow-boost.mxOndo/boost_1_89_0` is the
already-present Boost 1.89.0 source used without installation or lock changes.
`S=/tmp/ow-multigpu-review-dx8ulkiu` is the isolated snapshot. Log trailing
whitespace was trimmed; diagnostic text and outcomes are unchanged.

| Log | Command | Exit | Count/result |
|---|---|---:|---|
| 01 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 02 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | 54/56 pass; duplicate identity and cross-codec session RED |
| 03 | `build-tests/ow_progress_contract_tests` | 1 | 30/31 pass; placement readability/color RED |
| 04 | `cmake --build build-tests --parallel 2` | 0 | build only; identity fields added for behavioral tests |
| 05 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | 54/59 pass; five intended identity/session regressions |
| 06 | `cmake --build build-tests --parallel 2` | 0 | build only; intermediate implementation |
| 07 | `OW_BUILD_DIR="$PWD/build-multigpu" ./scripts/test.sh -DOW_BOOST_ROOT="$B" -DOW_ENABLE_IPO=OFF` | 8 | 5/6 suites pass; 60/61 adaptive pass; occupied CPU fallback RED |
| 08 | same script command as 07 | 0 | 6/6 suites pass |
| 09 | `cmake --build build-multigpu --parallel 2` | 0 | build only; prior speed-only ranking retained for next RED |
| 10 | `build-multigpu/ow_adaptive_hardware_contract_tests` | 1 | 63/64 pass; opposite codec winners split-card placement RED |
| 11 | same script command as 07 | 0 | 6/6 suites pass after card affinity |
| 12 | from `$S`: `./scripts/test.sh -DOW_BOOST_ROOT="$B" -DOW_ENABLE_IPO=OFF` | 0 | 5/5 suites; **183/183 contracts** |
| 13 | from `$S`: `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 OW_BUILD_DIR="$PWD/build-sanitize" ./scripts/test.sh -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT="$B"` | 0 | 5/5 suites; **183/183 contracts** |
| 14 | `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o "$S/strict-progress"` | 0 | strict instrumented build only |
| 15 | from `$S`: `python3 tests/adaptive_probe_smoke.py --evidence-probe build-tests/ow_adaptive_evidence_probe --rendition-probe build-tests/ow_rendition_probe` | 0 | 2/2 real 1080p CPU outputs; codec, 72 frames, AAC, geometry and decode |
| 16 | from `$S`: `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe` | 0 | 5/5 synthetic CPU video/image artifacts; H.264, HEVC, WebP decoded |
| 17 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$S/strict-progress"` | 0 | 32/32 console/progress/encoder contracts |
| 18 | extract `LastTest.log` counts; inspect native prerequisites | 0 | 183 cases each build; Linux, no `cl.exe` or `powershell.exe` |
| 19, 20 | copies of isolated `Testing/Temporary/LastTest.log` | n/a | full per-case evidence for 12 and 13, no additional test execution |

Gates 07, 08 and 11 include the separate concurrent dependency suite (six CTest
suites). Final gates 12/13 exclude that uncommitted work (five suites). The final
suite counts are 75 core, 9 pipeline, 3 rendition, 64 adaptive and 32 console/
progress/encoder = 183. CMake's sanitizer option instruments 139 core/adaptive
contracts; standalone pipeline/rendition/progress targets also execute, but are
not instrumented by that option. Gate 17 independently instruments all 32
console/progress/encoder contracts. No GPU or Windows sanitizer coverage claimed.

Two early attempts to launch the shared `build-tests` adaptive binary while
that directory was being rebuilt did not execute tests: the Python wrappers
exited 1 for `PermissionError (13)` and `ETXTBSY (26)`, respectively. Program exit
codes were unavailable and executed-case counts were zero. Fresh successful
builds preceded the recorded red runs; final gates use separate directories.

Final owned-file hash and whitespace/index checks are recorded in
`21-final-review.log` (exit 0 for each; ten source hashes matched and zero
whitespace errors). No additional contract rerun was necessary after documentation
and evidence edits.

## Real two-GPU Windows gate — unrun

**Zero native/physical checks executed; exit codes unavailable.** Linux cannot
establish MSVC compilation, CUDA DLL ABI/mapping, approved FFmpeg compatibility,
native Job Object drain, actual VRAM/session availability, concurrent NVENC or
Windows console presentation. The eight baseline registered Windows CTest suites,
including the updated native inventory/drain contracts, remain required before
release. Concurrent dependency work may register additional suites; record the
actual count for the exact tree built.

The operator can perform the following later, in an approved isolated candidate
environment, without treating this checklist as authorization to change a live
installation:

1. Build with actual MSVC and the reviewed Boost root, then run every registered
   CTest suite. Record compiler version, command, exit and case/suite counts.
   Representative existing commands are:

   ```powershell
   cmake -S . -B build-win -G 'Visual Studio 17 2022' -A x64 -DOW_BOOST_ROOT='C:\verified\boost_1_89_0'
   cmake --build build-win --config Release --parallel 2
   ctest --test-dir build-win -C Release --output-on-failure
   ```

2. With competing encoders absent, run the candidate's approved-FFmpeg probe in
   the isolated environment. Check the RTX 3070 and RTX 3050 have different
   nonzero UUIDs, correct explicit encoder ordinals, and real encode/decode/
   strict-output evidence. The extra LUID must have no profile and contribute
   no capacity. Expect two schedulable cards **only if both actually pass**.
   Inspect the per-codec matrix; a family-level `nvenc` token is insufficient.
   If the 3070 still fails, record actual driver version, external session use
   and memory telemetry; do not declare a session-limit cause from the error alone.

3. With configured `max_jobs >= 2` and enough CPU, RAM, scratch and VRAM for two
   reservations, admit two distinct sufficiently long synthetic video jobs.
   Match short suffixes to full durable attempt IDs and the GPU name/UUID table.
   Require launch A on GPU 0 and launch B on GPU 1 **before either corresponding
   `done` record**. Match their child PIDs and simultaneous per-card Video Encode
   activity in Windows/NVIDIA telemetry. Two selected-profile lines, two START
   lines, total elapsed time alone or utilization of a single card are insufficient.

4. Exercise H.264 and HEVC, including cards with opposite measured codec winners.
   Verify the two jobs remain independent, at most one worker-owned encode runs
   on each card, and a third NVENC request takes CPU or is refused by another
   binding limit. Independently constrain CPU/RAM/scratch/global VRAM/device VRAM
   and job count; no admission may bypass those limits.

5. Cancel one running job and observe its complete child tree drain before its
   card/capacity can be reused; the other card's job continues. In a controlled
   hardware failure, require partial-output removal, only the failed device/codec
   demoted, and a visible CPU retry with independently validated output. Check
   CPU fallback contention, pause of reserved starts, viewer close/reopen,
   suffix collision behavior, 80-column lines, all five colors and NO_COLOR/
   redirected plain output. Keep actual output validation/result status separate
   from an ACK or an encoder `done` line.
