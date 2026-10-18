# Worker-flex r2: verified compute, rollout held

Branch: `fix/worker-flex`; verified C++/measurement commit **`3ba4a8f`**, following
implementation checkpoint `41cbb42` and first-pass `f2b52d5`. This report
supersedes the checkpoint's pending measurement statements.
The operator authorized related Windows staging/build/test/encode work and
**conditional** merge/push/install. Production-server changes are not authorized.

**Result:** clean native gates and identical-quality offline throughput pass on
both PCs; the exact refused asset completes on Danny. Active-foreground workload
impact is not qualified. **No merge, push, install or production change.**

## Cause and validation contract

The original input was refused before encoding because `VideoTiming::rate()`
required constant cadence or an integral human-readable `tbr`. The real input
has irregular timestamps and rounded `29.92 tbr`, but exact demuxed timing is
available. That was an inspection/prediction refusal, not demonstrated bad media
or a bad GPU. Full investigation, original code/evidence and ranked assumptions:
[the first-pass report](../worker-flex/REPORT.md).

Execution now preserves passthrough timestamps and demux time base. Validation
compares decoded sample count, exact rational relative PTS and total demuxed
sample duration. It still rejects malformed/ambiguous timing and altered output;
no approximate-PTS or duration tolerance was introduced. Single-frame input is
covered. Format/inspection/security bounds remain; this is not universal support
for every possible codec, edit list or malformed container.

Additional real-media regressions found and fixed two compatibility problems:

- `fine-vfr-mux-tail`: unchanged frame PTS/count could still lose sample duration
  (133183 ticks became 132489). The generated command corrects the last encoded
  sample duration with `setts`, explicitly preserving **both** `pts=PTS:dts=DTS`.
  The default combined `TS` was experimentally wrong and is not used.
- `legacy-90k-vfr`: optional exact/integral codec/VUI hints preserve representable
  legacy nominal metadata (`90000/1`) without making a guessed rounded rate an
  execution prerequisite. Runtime nominal-rate incompatibility can use CPU
  fallback without permanently demoting an otherwise functional backend.

The real-media smoke passes seven generated inputs / fourteen video outputs,
including full decode and strict timing. Retained reds/greens are
`mux-tail-{red,green}.log`, `legacy-rate-before.json`, `media-legacy-green.log`.
The deployed coordinator separately compares nominal `r_frame_rate`; synthetic
compatibility and the exact asset's matching metadata are not coordinator ACKs.

## Remaining assumptions, ranked

| Risk | Finding / current disposition |
|---|---|
| High compatibility | Coordinator nominal-rate comparison is separate from true timeline validity; hints/fallback cover tested cases, not general production interoperability. |
| High throughput | Fixed session guesses, 5% backend exclusion and same-geometry RAM estimates wasted proven capacity; replaced by observed simultaneous proof, ranking and actual successful memory evidence. |
| High false failure | Fixed validation timeout could manufacture GPU failure; now bounded by remaining attempt deadline, with full validation retained. |
| Medium hardware eligibility | Backend geometry ceilings, even axes, unknown dimensions and conservative SDR/CUDA eligibility remain acceleration boundaries; CPU fallback, not proof that the input is bad. |
| Medium bounded parser | Inspection bytes/packet counts, positive durations and unique PTS remain security/representation boundaries. Missing/ambiguous timing is not universally solved. |
| Intentional delivery/security | yuv420p, encoder quality policy, audio handling and input/output/scratch limits remain. No source-bitrate prerequisite was found. |

## Adaptive stage implementation

- Bounded parallel preparation, independent child/log/progress directories,
  streaming single delivery lane and stable result-role identity. A ready poster
  can overtake a slow video. Failure cancels siblings and joins/drains all work.
- Atomic input compute-escrow handoff and separate stage CPU/private-commit claims.
  Encoder/session/VRAM ownership ends only after contained encode trees drain;
  validation and transfer acquire separate claims. CPU retry reacquires capacity.
  Deleted output/source scratch envelopes can retire early.
- Per-device ceilings come from successful simultaneous positive-frame encode
  activity, complete decode/validation and byte equality to single-run evidence.
  Bounded 2, 4, 8, ... sweeps retain the last proved point when headroom/pressure
  prevents a larger trial. Paced qualification is excluded from throughput.
- Healthy measured windows, exercised trial counts and validated completions
  qualify growth. CPU/RAM/disk, busiest per-card GPU engine and foreground
  feedback back off/defer new starts; running work remains accounted and drains.
  Unknown/stale samples cannot qualify growth. Trial, qualified, ownership and
  queue reasons are exposed in `worker-capacity.txt` and console/log output.
- Functional slower encoders remain available. Actual successful child-memory
  evidence supersedes a same-geometry estimate; larger/unknown geometry remains
  conservative. Bounded observed-allocation retries reserve growth atomically.
- Full video validation uses remaining attempt time, not a fabricated fixed
  thirty-second corruption/GPU-failure conclusion.

GPU memory sampling also rejected an entire sample when WDDM returned valid
zero process records alongside positive ones. Danny's behavior red had
457457664 positive bytes, six zero records, `worker_known=false`, exit 1.
Rejecting negative rather than zero records restores that positive aggregate;
missing/all-zero process evidence remains unknown. The native resource probe
then exited 0. Rick's idle all-zero/missing process data exited **2**, an unknown
prerequisite, not GPU-memory proof. Adapter dedicated usage is sampled separately;
valid adapter zero does not measure shared GPU RAM.

## Hard guarantees versus feedback

Hard: atomic declared reservations within configured per-machine/per-task
budgets; typed, current successfully probed backend selection; proved session
ceilings; child-job CPU-rate/private-commit limits; lease fencing, pause and
reserved-BEGIN rejection; cancellation/process-tree drain before release;
input/output hashes, full validation and durable result retry/matching ACK.
Closing the viewer still cannot terminate the controller or its jobs.

About 90% is a **sampled utilization target**, not a universal quota or a promise
of constant 90% use. GPU engines/VRAM, physical RAM, disk and desktop effects are
feedback. WDDM background priority is best effort. Private commit is not resident
physical RAM; parent work, driver/shared allocations and external applications
are not all bounded by child quotas. A transient sample can exceed 90% (Rick
reached 99.31% CPU). Never claim zero foreground impact or a hard GPU-time quota.

## Clean native build and artifact evidence

Both optimized MSVC Release/LTO builds used fresh `candidate/build-clean`
directories, static Release CRT, `/O2 /Ob2 /DNDEBUG /Zi`, linker `/DEBUG
/INCREMENTAL:NO`, reviewed installed Boost and the pinned FFmpeg SHA
`f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`.
No dependency upgrade, software installation or machine-wide policy/PATH change.

- Source archive SHA: `c31979418608d3b2a10d2529631fcabe0132e96c8718ef75064e002f3bc54100`.
- `build-inputs.json` lists all 78 CMake/C++ header/source inputs. Both PC source
  readbacks matched **78/78**; executable hashes are in `*-readback.log`.
- Rick: ten registered native suites, every `CLEAN_SUITE_EXIT=0`;
  `CLEAN_BUILD_EXIT=0`, `CLEAN_WINDOWS_GATE_EXIT=0`.
- Danny: eleven registered native suites, every `CLEAN_SUITE_EXIT=0`;
  `CLEAN_BUILD_EXIT=0`, `CLEAN_WINDOWS_GATE_EXIT=0`.
- Linux Release and global ASan/UBSan/leak detection: six suites each, exit 0;
  `linux-final-{release,sanitize}.log` (and earlier r6 logs).

Earlier incremental artifacts crashed with native **-1073741819 / 0xC0000005**
after isolated-worker setup, despite green unit suites. Those were failures,
not accepted measurements. The old offline harness object was timestamped
1792346935905 ms, before the changed capacity header (1792348228990 ms); its
MSVC read-dependency log contained **zero project headers**, including neither
`worker.hpp` nor `capacity.hpp`. This strongly supports stale-layout/ABI artifacts,
not media corruption. No old-crash stack was obtained. Fresh RelWithDebInfo
execution passed two real jobs; clean optimized Release passed the sustained
runs and exact asset. **Never reuse incremental staging objects as gate evidence**:
use a fresh directory or clean-first after every source/header change, then
exercise the actual worker path. Test-only crash tracing searches local symbols.

## Identical-quality throughput

These are **offline real `Worker::execute`** measurements: real admission,
journal, factory, containment, validation and result handling; only download/PUT
ports are local files. They are not Internet upload or coordinator/publication
throughput. No real account is contacted. The isolated constructor is private to
the linked harness, not selectable via config/network. Harness children use Low
priority; installed operator priority remains Maximum and was not changed.

Before is `f2b52d5` plus explicit measurement ports, **not** the installed binary.
`baseline-measurement-final.patch` fully records those ports (apply-check passes).
Rick's before qualification uses one CPU thread to establish single-session
proof; runtime hard budgets and encoder settings stay unchanged. Failed earlier
wrong-pin, missing-proof, null-exit and deliberately stopped trials are excluded.

Both use the same generated 4822953-byte 1080p/72-frame source, SHA
`b27e0b3a6fb7621231fa38ef0d408f97e075ef6d02df65f4e089abc10b90990c`.
H.264/HEVC settings, audio policy, geometry and device pins are unchanged.
**Every role of every accepted before/after job is byte-identical** on its PC.
Pins: Danny H.264 NVENC 0 / HEVC NVENC 1; Rick both QSV DXGI adapter 1 (not 0).

| PC | Before | Accepted after | Ratio |
|---|---:|---:|---:|
| Danny jobs / measured wall | 21 / 62.337 s | 113 / 70.222 s | |
| Danny observed jobs/hour | 1212.76 | 5793.06 | **4.777x** |
| Danny mean / p95 job wall | 2.998 / 3.053 s | 12.576 / 33.902 s | backlog latency rises |
| Rick jobs / measured wall | 12 / 80.149 s | 13 / 68.097 s | |
| Rick observed jobs/hour | 539.00 | 687.25 | **1.275x** |
| Rick mean / p95 job wall | 6.679 / 7.082 s | 19.448 / 30.923 s | backlog latency rises |

Window drains are included. Reported per-job wall includes stage waits, not just
encoder service time. More admitted jobs trade increased queued latency for
higher total throughput; these figures are not isolated-request latency gains.
Warmup lasts at least sixty seconds and is excluded from measured throughput.
Peaks below include warmup; cadence is nominally 200 ms plus sampling overhead,
not continuous high-water measurement. External FFmpeg was not observed.

Danny trial hard caps increased from CPU4/RAM4096 MiB/max_jobs4 to
CPU18/RAM32384 MiB/max_jobs32; scratch remains 8192 MiB. Thus 4.777x measures the
adaptive implementation **with explicitly larger trial budgets**, not a pure
same-budget algorithm speedup. Rick's accepted comparison keeps CPU4/RAM2667
MiB/scratch8192 MiB/max_jobs4 unchanged.

| Sampled peak | Danny before / after | Rick before / accepted after |
|---|---:|---:|
| Whole-host CPU | 32.62% / 58.64% | 78.13% / **99.31%** |
| Whole-host used physical RAM, bytes | 23513497600 / 24370434048 | 7718305792 / 7549702144 |
| Harness + children private commit, bytes | 582520832 / 1318477824 | 368455680 / 456822784 |
| Harness + children working set, bytes | 312373248 / 834719744 | 261799936 / 340303872 |
| RTX 3050 dedicated VRAM, bytes | 624832512 / 624857088 | n/a |
| RTX 3070 dedicated VRAM, bytes | 1482870784 / 1283280896 | n/a |
| Rick adapter dedicated readings | n/a | zero / zero; **shared RAM unmeasured** |

CPU/physical RAM are whole-host samples, including unrelated activity; commit/RSS
are harness and direct children. GPU-engine/disk feedback is implemented, but
comparable high-frequency GPU-engine/disk peak aggregates were **not retained**
by this harness. Do not invent those peaks or universal 90% utilization.

Rejected policy trial: automatically shrinking Rick's hard private-commit budget
to 896 MiB and allowing fourteen jobs achieved only 398.00 jobs/hour, worse than
539.00 before. It is retained in `reald-rejected-auto-policy.json`, not accepted
for installation. Sampling free physical RAM is not sufficient justification to
replace a configured private-commit ceiling. The accepted fixed-budget trial
avoids that policy. Larger automatic caps on Rick are **not qualified**.

## Exact previously refused asset

Danny completed the actual 24609052-byte input with exact SHA
`43806f9d921afe0def16befac5e6a95374f9571f2ae480c6950e9e0f3f85ef27` in
**53404 ms** using real worker execution/local storage. Numeric benchmark exit 0.
`exact-asset-native.json` records the successful three-role result.

An independent Linux FFprobe/full FFmpeg decode+SSIM oracle checked both copied
Windows video outputs (`exact-asset-independent-oracle.json`):

- **717/717 decoded frames**, every exact rational relative PTS unchanged;
- total demuxed sample duration **59889/2500 = 23.955600 s**, unchanged;
- full coded geometry unchanged; both nominal rates **359/12**, matching input;
- feed SSIM **0.990175**, optimized SSIM **0.990496**; decode/SSIM exit 0.

Asset-run sampled CPU peak 17.09%; host used RAM 23930007552 bytes;
harness/children commit 627916800; working set 399761408; RTX3050 dedicated
588644352 and RTX3070 1235267584 bytes. These are compute/local-port numbers,
not a production retry/publication, remote ACK, or phone-notification proof.

## Foreground and rollout boundary

The temporary manual-start, limited normal-user interactive task records only
input age and WM_NULL responsiveness; no titles/content/input injection or
activation. Accepted Danny after has 434 known message responses, maximum 1 ms,
two unknown foreground samples, and **zero active-input samples**. Rick after
has 417 no-foreground-window samples and one unknown, no message-response samples.
Neither is an active game/render workload test or a human "desktop felt fine"
observation. Rick's transient 99.31% CPU must not be hidden behind the 90% target.

Therefore **active-desktop impact and full rollout acceptance remain open**.
The prepared `install-qualified.ps1` is **not executed against either install**.
Its synthetic-only native contract tests pass successful publication, candidate
launch failure with byte-exact old-config/version rollback, and tampered-profile
rejection (exit 0). They do not establish real updater rollout or foreground QoS.

Both installed workers still use source `456eecca81635c30b7edb524165010f8fb9218ff`,
version `0.1.2-1b4042d3bbfa85cc`, executable SHA
`1b4042d3bbfa85cca58eeb4b585ca2bb0a39affa3c84600b09301ed0c6901c0f`.
Readbacks show **accepting, zero running jobs, idle cards**: cpu/qsv/amf on Rick,
cpu/nvenc on Danny. This verifies unchanged installed-worker continuity, **not**
the candidate running live. No merge/push/release/install/startup or production
write occurred.

## Cleanup and final continuity

Both cleanup processes exited **0**, then independent fresh SSH sessions verified:

- `%LOCALAPPDATA%\\Temp\\ow-worker-flex-20261018-r2` **absent**;
- owned `OrganizerWorker-Flex-20261018-r2` task **absent**;
- **zero** executable processes under the owned staging path;
- installed manifest/config/protected identity files unchanged by cleanup;
- unchanged installed SHA above, same original process: Rick **19528**, Danny
  **36272**, both interactive session 1; fresh status **accepting**, running 0,
  idle encoder/card rows; last connection state CONNECTED.

Evidence: `*-cleanup.log`, `*-final-absence.log`, all numeric exits 0. Only the
owned observer was stopped/unregistered; neither installed worker was restarted.
Four clean-build artifacts per PC were retained privately in ignored local cache,
with all hashes matching readback; they were not published, installed or released.
Real source/outputs are not committed. Future rollout remains conditional on the
missing active-foreground qualification (and honest treatment of unretained
GPU-engine/disk peak aggregates), not merely these cleanup/native successes.
