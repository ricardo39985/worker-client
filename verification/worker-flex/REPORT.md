# Worker-flex — investigation and partial candidate

Branch: `fix/worker-flex`, based on local `main` = `225d441`.
**Not complete, not native-gated, not installed.** Remote operations in this
investigation have been read-only. No push, merge, release, installation,
production write, restart or dependency installation was performed.
The operator's added sustainable-concurrency requirement remains an open
acceptance gate, not a claimed property of this partial patch.

## 1. Independently checked cause

### Measured/read back

Both PCs select and actually run
`versions/0.1.2-1b4042d3bbfa85cc/OrganizerWorker.exe`:

- A (`reald@192.168.1.10`), PID 19528;
- B (`danny@192.168.1.189`), PID 36272.

Both `source-revision.txt` markers read
`456eecca81635c30b7edb524165010f8fb9218ff`; both executable SHA-256 values are
`1b4042d3bbfa85cca58eeb4b585ca2bb0a39affa3c84600b09301ed0c6901c0f`.
The brief's claim that the installed binaries came from `225d441` is imprecise:
that is the subsequent evidence commit. `git diff 456eecc..225d441` has no
changes in application, tests, scripts, lock or CMake source.

An existing **local** file, `/tmp/ow-timing-regression-20261018/source.bin`, is
24,609,052 bytes and independently hashes to the brief's
`43806f9d921afe0def16befac5e6a95374f9571f2ae480c6950e9e0f3f85ef27`.
No new media download was needed. It was inspected, not committed as a fixture.
Local FFprobe reports:

- HEVC Main, yuv420p, coded 1920x1080, rotation -90 degrees;
- video time base 1/30000, 717 samples, `r_frame_rate=359/12`,
  `avg_frame_rate=597500/19963`;
- **705** successive PTS gaps of 1000 ticks, **7** of 1152 and **4** of 1151;
- video duration **23.955600 s**, AAC/format duration **24.078345 s**.

This is genuinely irregular presentation timing, not merely failure to recover
an otherwise constant rational rate. It does not establish how the recording
application created those intervals. `359/12` is demuxer rate metadata, not an
exact common interval between its actual frames.

B's retained child log for attempt
`330b8415-8708-4302-b2bc-52413a6d317d` contains `29.92 tbr`, `30k tbn` and all
717 packet records. Worker log:

| UTC / stage | Evidence |
|---|---|
| 13:32:09.149 | START |
| download | 1.091480 s |
| input SHA-256 | 0.035548 s |
| input_timing | 0.108064 s; enclosing stage completed=false |
| 13:32:10.386 | failed result pending ACK |
| 13:32:10.390 | `Source video timing cannot be verified; refusing unvalidated output` |
| 13:32:11.145 | result acknowledged |

Total worker attempt wall time was **1.240064 s**. There is no encode stage.
`completed=false` on the timing stage does **not** prove FFmpeg failed: the
policy throw occurs inside that stage's timer lifetime.

Read-only production SQL independently links the attempt, job and input digest;
`native_worker_attempts.state=rejected`, `begin_confirmed=true`, and
`processing_jobs.payload.media_route={"reason":"worker_failed","destination":"local"}`.
Submission `e20dc564-13f1-4632-9b71-0723ab199c5d` became DB `ready` at
13:35:17.882562; its notification delivery was `provider_accepted` at
13:35:22.109719. Provider acceptance is not proof of delivery/display on a phone.
The primary asset and media job were created at 13:31:30.459461. This corroborates
the completed-upload timing approximately; it is not independent network-byte
instrumentation for the asserted 13:31:26–30 transfer.

Production logs with Docker timestamps show:

- initial preparation: 13:31:33.496974 → placement wait 13:32:07.814774;
- fallback begins 13:32:11.392938;
- required feed quality passes at 13:35:17.071008:
  **717 expected/717 actual frames, SSIM 0.9812280376569041**;
- playback quality passes at 13:41:22.766917, SSIM 0.9867878228730825;
- asset processing ends at 13:41:24.177948, **elapsed_ms=552783**, threads=1;
- job publication log at 13:41:24.194796.

**Correction to the brief:** about 186 seconds got the required feed ready, not
all conversion finished in 187 seconds. Optional work continued until about
9m13s after fallback began. The feed row was created at 13:35:17.875897;
playback/poster rows at 13:41:24.161808. No `feed_optimized` row was present at
inspection. The DB readiness is not a new observation of the actual app UI.

### Code and reproduced causal chain

Installed-equivalent `windows/worker.cpp:691` (pre-change) requires:

```cpp
if(!source_timing || source_timing->reported_rate.empty() || !source_timing->rate())
    throw ...;
```

`include/ow/video_timing.hpp::VideoTiming::rate()` first requires identical PTS
steps, otherwise accepts **integral-only** human `tbr` strings (including k/M
suffixes). This source has differing steps and fractional `"29.92"`, so `rate()`
returns no value. The rendition factory separately throws
`Exact source video frame rate unavailable` for the same condition. Failure is
thus an implementation policy, **not** evidence of an unplayable upload or an
encoder/card failure. The generated regression independently reproduces it.
`inspect-refusal.cpp` also feeds the actual retained input-timing log into the
unchanged main parser/factory: **717 samples, 1/30000, reported_rate=29.92,
constant_rate=0**, then the exact-rate exception (exit 1). The candidate plans
all three renditions from that same evidence (exit 0). See
`actual-inspection-before.log` / `actual-inspection-after.log`. These are
inspection/command-construction checks, **not** encodes of the personal file.

### Coordinator disagreement, and limits of this fix

The production host's source at `/opt/organizer/backend/worker/main.cpp:389–391`
still compares remote output `r_frame_rate` to source using 0.000001 tolerance.
Its local path instead has full timeline/quality checks and treats optional
outputs separately. Container revision label:
`7a1e29fde0547a4d99b237d18053f9c7d7ddfe94`. Reading host source and image labels
is not a binary disassembly/provenance attestation.

Local acceptance of this feed therefore does not prove that the remote validator
would accept every timeline-equivalent result, nor that the full worker artifact
set is equivalent to the local publication path. **Coordinator contract alignment
remains required for general VFR acceptance.** No Organizer source or deployment
was changed. The generated rounded-rate source happens to preserve `359/12` in
both CPU outputs; that is not proof of physical GPU/coordinator acceptance.

## 2. Other assumption-based checks, ranked

| Risk | Check / code | Finding and current disposition |
|---|---|---|
| Critical / reproduced | Nominal-rate prerequisites and nominal equality (`video_timing.hpp`, `media_renditions.hpp`, worker source timing) | Legitimate VFR refused before execution; removed in this candidate. Single-frame minimum also corrected. |
| High / observed elsewhere | 30s full-output validation deadline; single-thread software decode (`windows/worker.cpp` validation) | Attempt `d4139de3` encoded optimized output in 5.703s, validation expired at 30.051s, then CPU retry expired at 240.098s. This does not establish corrupt output or a broken GPU. Fixed decode deadline/demotion policy needs separate correction. Not disabled here. |
| High / observed | Remote nominal-rate gate versus local timeline gate (production source above) | Can reject equivalent presentation timelines. Requires coordinated server contract/test work; untouched production. |
| High / structural | CPU fallback required before enabling any app-video capability; CPU sample concurrency only 1/2; 5% gain floor (`probe_ffmpeg`, `choose_profiles`) | A small synthetic sample is not sustainable host capacity. Slower-on-that-sample GPUs are withheld even when CPU is busy; cannot serve as general input-dependent ranking evidence. Real proof before scheduling remains necessary. |
| High / structural | GPU RAM formula and max-profile RAM top-up (`gpu_child_memory_mb`, `reserve_hardware_profiles`) | Host commit slope is extrapolated above sample sizes. BEGIN tops up to the maximum of *all* profiles, not just the selected ones. May waste budget or take CPU; actual child limits remain mandatory. Allocation failure is already distinguished from permanent card failure. |
| High / structural | Job-wide encoder/CPU-fallback/VRAM ownership; fixed one session/card, two worker NVENC sessions; prefer same card for both codecs (`Scheduler`) | Safe conservative proof boundary, but not measured sustainable machine throughput. Candidate releases completed-codec resources after drain. Full stage scheduling, measured session counts and overlap are still unfinished. |
| Medium | NVENC/QSV 4096x4096, AMF 3840x2160; even axes; unknown geometry takes CPU (`encoder_geometry`, worker inspection) | Reviewed ceilings are assumptions, not queried physical maxima. Valid large inputs can lose acceleration; unknown inspection must not mean invalid input. CPU retry remains. |
| Medium | CUDA restricted to CFR and inspected SDR 8-bit 4:2:0 H.264/HEVC (`device_decode.hpp`) | Performance eligibility only, not file refusal; software decode still uses a proven hardware encoder. Broadening needs real format/device proof. |
| Medium | Fixed 5MiB inspection log / 300k packets, positive durations, unique PTS (`video_timing`) | Security bounds retained. Missing/zero packet durations, unusual packet-to-frame mappings and edit-list semantics are not universally solved by this patch. Do not claim arbitrary-codec/container acceptance. |
| Lower / intentional delivery boundary | yuv420p, fixed CRF/CQ, full coded dimensions, audio-copy/AAC policy; fixed input/output/scratch limits | No source-bitrate prerequisite found. These are delivery/security contracts, not measured quality equivalence. Odd geometry/HDR/profile limitations also exist server-side; arbitrary conversion cannot be promised. |
| Lower / graceful degradation | Poster reuse rejects rotation/unrecognized profiles (`inspection_poster_reusable`) | Only disables an optimization; falls back to ordinary poster generation. This actual source is rotated, so fused-poster reuse is not assured. |

Hash, TLS/host approval, installed/probed typed capability, lease/BEGIN fencing,
pause, durable result ACK, process containment, per-child budgets and cancellation
are real guarantees, not prediction checks to remove.

## 3. Partial fix and behavioral evidence

### Timing contract

- Keep `-fps_mode passthrough` and `-enc_time_base:v demux`.
- Remove nominal-rate derivation/prerequisites and libx265's invented `fps=`.
- Compare source/output sample counts, **every exact rational relative PTS** and
  **total demuxed sample duration** before upload. Nominal `tbr` is diagnostic.
- Accept a measurable one-frame video. Still reject malformed/missing/duplicate
  timestamps, duration changes, frame loss/duplication and retiming.
- Keep guarded hardware, CPU fallback, decode/geometry validation and bounded
  inspection. No extra executable/runtime dependency, lock change or network
  converter option is introduced.

A useful correction during implementation: a packet's duration at the maximum
PTS is **not** generally the final display hold after B-frame reordering.
Generated evidence had differing durations on that packet but identical stream
length. The final implementation compares the sum of demuxed sample durations,
not that incorrect per-PTS-duration assumption. It does not claim to validate
all audio timing or every container edit-list interpretation.

`contracts-before-final.log` runs the new 18 behavioral contracts against the
unchanged main timing/factory headers: **5 actual runtime failures**, including
`Exact source video frame rate unavailable`. The candidate passes all 18.
`media-red.log` uses a saved baseline executable and fails on `rounded-vfr`.
`media-green.log` passes five generated clips × two codecs, including 717-frame
`359/12` / `29.92 tbr`, integer/fractional CFR, fine VFR and one-frame video.
The independent FFprobe oracle checks decoded frame timestamps and sample
duration; the actual C++ validator runs too, followed by full FFmpeg decode.
All fixtures contain generated pixels, not production media. No PC encode ran.

Changing nominal equality is a deliberate validation-contract change, **not**
deleting a failing test to conceal a regression. Presentation timing is what
is preserved and compared; codec rate metadata is not that timeline.

### Resource-lifetime improvement (not a complete concurrency implementation)

`Scheduler::Reservation::finish_codec` releases only the drained codec's unused
slots, VRAM and aggregate compute cost. Other pending codecs retain their costs;
a shared card remains reserved until its last owner drains. Duplicate completion
cannot release a different job's resources. Worker calls this only after
`convert` has returned from encoder/retry/validation drain. Transfer/admission
budget and source/output scratch remain held for their actual later lifetimes.

Two new scheduler contracts cover separate-card/RAM reclamation and shared-card
retention. `stage-release-red.log` is explicitly an **API compile red** (the old
reservation cannot express partial completion), not a pretend executed old
scheduler behavior run. The 80-case adaptive suite then passes. Existing
cancellation/drain, memory, session, pause and persistence contracts remain green.
Native integration of that callback is unrun.

This does **not** yet overlap both encodes, calibrate capacity, implement a 90%
feedback controller, relax per-card proof limits or expose full capacity/in-use
and local-queue reasons in the console. It must not be reported as doing so.

## 4. Throughput assessment

The worker contains real optimizations (eligible CUDA decode, fused inspection/
poster, per-machine encoder probes, encode/upload overlap). It is **not presently
optimized for sustainable end-to-end throughput**:

- both full video renditions still decode the original separately;
- full output software decode is often more expensive than GPU encoding;
- encodes and posterior validation are sequential in the preparation lane;
- poster/encoder sharing is conditional; this source's rotation disables reuse;
- one delivery may overlap preparation, with one pending artifact backpressure;
- source hash is done once, each output hash once: these are necessary integrity
  checks, not evidence of gratuitous re-hashing;
- input packet inspection is reused for both videos; output packet inspections
  are separate required comparisons, not full decode repeats;
- CPU tuning explores only 1 and 2, GPU tuning only 1/card; no simultaneous
  CPU+two-card sustainable mix or desktop/disk saturation calibration exists;
- per-device proof is respected, but a whole job prefers one card and can retain
  resources through unrelated later work. Early codec release addresses only a
  portion of that last issue.

Selected B log measurements, all seconds:

| Attempt prefix | Job wall | Encode stages | Validation stages | Other |
|---|---:|---:|---:|---|
| `d83701d1` | 4.552163 | 1.345990 | 2.265182 | download .296178; upload 1.047554; input+output hashes .014283 |
| `e0f653b3` | 27.453926 | 21.173706 incl. CPU retries | 5.475352 | repeated nominal-timing guard failures; upload .816882 |
| `d4139de3` | 303.894537 | 251.098297 incl. timed-out retry | 50.260528 | output validation timeout led to 240.098358s CPU retry timeout |
| refused `330b8415` | 1.240064 | none | none | download 1.091480; input hash .035548; input timing .108064 |

`validation` encloses `output_timing`; uploads overlap the other lane.
**Do not sum all stage timings into job wall time.** In `d83701d1`, validation is
about 49.8% of wall versus 29.6% encoding; hashes are about .31%. Job CPU time
2.4375s / wall 4.552163s is roughly .535 logical CPU cores averaged over that
attempt, **not peak CPU utilization** and not proof of spare encode-engine
capacity. These different uploads cannot establish before/after speedups or
jobs/hour capacity. Extrapolating `3600/latency` would not measure sustained
queue throughput, so it is deliberately not presented as such.

### Machine observations, not sustainable-capacity measurements

| | A | B |
|---|---|---|
| CPU | i5-8305G, 4 cores / 8 logical | i7-12700KF, 12 cores / 20 logical |
| Total RAM | 8,002 MiB | 65,381 MiB |
| Read-only free-RAM sample | about 1,557 MiB (fluctuates) | about 43,559 MiB (fluctuates) |
| Existing operator ceilings | CPU 4, RAM 2,667 MiB, scratch 8,192 MiB, max jobs 4 | CPU 4, RAM 4,096 MiB, scratch 8,192 MiB, max jobs 4 |
| Installed profiles reported | CPU, QSV HD 630, AMF Vega M GL | CPU, NVENC RTX 3070 + RTX 3050 |
| Current proof/concurrency boundary | CPU 2/codec, hardware 1/card | CPU 2/codec, hardware 1/card; worker NVENC 2 |

A's saved 1080p sample reports QSV 22.785/17.404 fps H.264/HEVC and AMF
14.013/13.299 fps; CPU two-child aggregate 23.992/7.106 fps versus serial
13.918/4.184 fps. Sample CPU pairs thus improve aggregate sample rate about
1.72x/1.70x. They are short saved encode+decode samples, not fresh sustainable
job/quality/desktop measurements. Its saved child-commit peaks range 165–320
MiB, **not complete-job peaks or permission to lower child memory limits**.
B's saved NVENC samples report H.264 60.965 fps on both cards and HEVC
41.522/55.857 fps on 3070/3050. Independent equivalence/sustainable ranking was
not remeasured. At read-only NVIDIA sampling both cards were idle (0% GPU and
encoder); VRAM in use was 1,061/373 MiB. Idle telemetry is not a load peak or
proof of how much more can safely run.

**Unknown:** sustainable capacities per machine, complete-job peak CPU/RAM/VRAM,
disk contention, before/after jobs/hour and latency at identical quality,
foreground latency/responsiveness. No such numbers were obtained or invented.

## 5. Added concurrency requirement: remaining work and measurement boundary

Target approximately 90% of the **measured binding available resource**, not 90%
of every resource and not a fixed job count. Preserve operator ceilings until
an explicit operator-approved change; B's current CPU4/RAM4096 limits are not
90% of its host totals. RAM availability on A may bind before either encoder.
GPU engine, sessions, VRAM, CPU, commit/physical RAM and disk must all be checked.

Required next steps before making a throughput claim:

1. Operator-approved isolated Windows native gates (exact first-phase commands
   in `APPROVAL.md`); no setup/install/tray restart.
2. Separate, explicitly approved capacity experiments on each machine, using the
   pinned installed converter and generated SDR/VFR/rotated workloads. Measure
   serial baseline and stage-concurrent candidates with unchanged encoder
   quality settings, same inputs, repeats, warm-up and steady-state windows.
3. Sweep concurrency across individually proven devices/session counts and CPU
   workers only while resource/desktop headroom holds. Stop escalation before
   violating caps or the foreground-responsiveness budget. Failed samples never
   qualify a card or session count. Sample before/during/after CPU, committed and
   physical RAM, per-card engine/VRAM, disk latency/queue and process peaks.
4. Implement per-stage atomic resource acquisition and drain-bound release,
   separate child logs/progress state for simultaneous renditions, bounded
   output/backpressure, cancellation-aware queue waits and explicit queue reasons.
   Current shared `process.log`/progress state **cannot simply be parallelized**.
5. Expose measured capacity vs actual/reserved use, measurement age, limiting
   resource and admission/queue reasons. Do not call remote queue depth known:
   protocol v1 does not supply it.
6. Compare completed validated jobs per steady-state hour and per-job wall
   distributions; check artifact byte identity where possible, otherwise
   identical encoding settings plus decoded timing/geometry/audio and independent
   quality measures. Retest CPU fallback, concurrent cancellation/drain,
   reservations and output fencing. Re-run all platform gates before landing.

A sampled VRAM watchdog is not a hard OS GPU-memory quota. External desktop/GPU
loads can change between samples. Thus an unconditional claim of never reaching
100% host use or never affecting desktop latency cannot honestly be proved from
SSH sampling alone. Capacity controller/headroom policy and real foreground
operator acceptance are necessary; neither is established by these logs. No
unmeasured automatic concurrency or resource-ceiling increase is included here.

## 6. Gate ledger and permission stop

| Gate | Current result |
|---|---|
| Main-header behavioral timing red | 18 cases, 5 runtime failures, exit 1 |
| Generated baseline media red | exit 1 on rounded VFR |
| Partial-release API red | build exit 2: missing finish_codec; explicitly compile-only |
| Linux Release (`scripts/test.sh`) | 6/6 suites, 273 contracts, exit 0 |
| Full-target ASan+UBSan, leak detection, halt on error | 6/6 suites, 273 contracts, exit 0 |
| Generated candidate real-media smoke | 5 sources / 10 video outputs; exact decoded PTS/count/duration + full decode; exit 0 |
| MSVC/native suites for this candidate | **NOT RUN — remote-write authorization required** |
| Approved Windows FFmpeg actual media / per-machine calibration | **NOT RUN — encode/write authorization required** |
| Per-machine before/after throughput, peak resource and desktop acceptance | **NOT RUN** |
| General coordinator acceptance | **NOT RUN; nominal-rate contract mismatch remains** |
| Commit / push / merge / deployment / installation | First-pass source/evidence checkpoint committed at operator request; native gates still unrun; no push/merge/deploy/install |

Source/test work remains in the requested branch/worktree. Linux uses the already
available reviewed Boost 1.89.0 source root. Sanitizer flags are applied globally,
including header-only standalone test targets, not only `ow_core`. There was no
automatic tool/dependency upgrade. `APPROVAL.md` proposes the exact first Windows
write commands, their reason and cleanup; they have **not** been executed.
