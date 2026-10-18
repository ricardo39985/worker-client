# Video timing correction — October 18, 2026

Application revision: `456eecca81635c30b7edb524165010f8fb9218ff`.
Initial timing correction: `1d21709963fda04349699ba6494a22cc24ef6adc`.
Previous main: `4cd3ed81925173043d93399f33d617e71f2a336a`.

## Reproduced cause

The exact source from attempt `610735d5` is 580×1280, 518 frames. ffprobe reports
`r_frame_rate=90000/1`, `avg_frame_rate=4662000/114113`, time base `1/90000`.
Its irregular presentation intervals need individual 1/90000-second timestamps.
The previous software-decode/NVENC command and installed CUDA/NVENC command both
produce `41/1`, with identical timestamp quantization and duplicate timestamps.
This is a pre-existing timing bug, not a regression caused by CUDA decode.
The software factory before decode commit `273441f` also used passthrough with
no encoder time base; its source hash is retained in `previous-factory.sha256`.
No previous worker version was installed to make this comparison.

`-fps_mode passthrough` alone leaves the encoder time base derived from the
average filter rate. It does not retain fine source timestamp precision.
`-enc_time_base:v demux` preserves all 518 exact source PTS in both decode paths.
It does not by itself fix the NVENC nominal/VUI rate: ffprobe reports
`20427/250` (81.708) on both paths. CPU libx265 also needs an explicit `fps`
parameter, or it signals `4662000/114113`. Explicit source time base plus libx265
`fps=90000/1` preserves the coordinator-required `90000/1` in both CPU outputs.

This evidence does not support missing CUDA device-frame metadata as the cause:
software and CUDA results agree before and after setting the time base. The
coordinator source compares **r_frame_rate**, with a 0.000001 tolerance, not
container avg_frame_rate. Its validator is unchanged. Container averages can
still differ with B-frame decode ordering and final packet durations despite
identical presentation timestamps; those differences are recorded, not hidden.
FFmpeg rejects output `-r` combined with passthrough as contradictory (exit 234
on the installed Linux FFmpeg), so no conflicting rate override was added.

## Change and behavioral proof

Every app video uses the input encoder time base with passthrough. A bounded
stream-copy packet inspection uses the already approved FFmpeg; no FFprobe
runtime dependency, new software, lock update or network option is introduced.
CUDA requires complete, strictly constant presentation intervals and the existing
SDR/codec eligibility checks. Irregular inputs use software decode with guarded hardware encoding. A timing
mismatch retries the existing CPU encoders without demoting a working GPU for
other sources; libx265 receives an explicit representable source rate. Ambiguous fractional VFR
rate reports fail closed rather than guessing a rounded rate.

Every video output is checked before upload: the demuxer nominal rate must match,
and every sorted, rational presentation timestamp and frame count must match.
This guard covers hardware, CPU retry and ordinary software app conversions.
Malformed/duplicate timestamps and missing timing evidence are rejected. PTS
comparison permits only a constant origin shift. The source probe is reused for
both video renditions. Inspection still produces and reuses the poster; rotation
and unsupported poster profiles retain their existing ordinary poster behavior.
Admission, leases, child budgets, drain, reservations, upload backpressure,
validation deadlines and durable-result contracts remain intact.

The actual media regression fails against the saved pre-change command probe
(exit 1, `timing-red.log`) and passes the final probe (exit 0,
`timing-green.log`). It checks integer, fractional and fine-resolution irregular
sources, both codecs, exact frame rate, container average and every PTS. Portable
contracts also reject a different rate even with unchanged timestamps, dropped
frames and duplicate timestamps; cover B-frame packet order, equivalent time
bases, ambiguous VFR and missing CUDA timing evidence. The API red compile exits
1 (`contracts-red.log`); the final 14 contracts exit 0 (`contracts-green.log`).
Existing CUDA device/encoder-option assertions remain, with CFR timing evidence
supplied to the now fail-closed selector.

## Native media measurements

Approved FFmpeg SHA256:
`f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`.
Measured on Danny's mapped RTX 3050 / CUDA ordinal 1. Before scripts reproduce
the old command profile; after commands come from the final MSVC-built command
factory. Output files are independently read back and inspected by local
ffprobe, including rational packet PTS, frame counts, raw dimensions and rotation.
See `before-rates.json`, `rejected-rates.json`, `after-rates.json`,
`after-ffprobe.log`, `after-r2-native.log` and native timing logs. Personal media stays in the single
measurement directory, never in this repository or fixtures.

| Source | Old CUDA / old software-decode nominal rate | Corrected rate, both video outputs | Path |
|---|---|---|---|
| Generated integer, 90 frames | 30/1 / 30/1 | 30/1 | CUDA and software-decode NVENC |
| Generated fractional, 90 frames | 30000/1001 / 30000/1001 | 30000/1001 | CUDA and software-decode NVENC |
| Generated VFR, 54 frames | 30/1 / 30/1 | 30/1 | Software decode / guarded NVENC |
| Rotated fractional, 90 frames | 30000/1001 / 30000/1001 | 30000/1001 | CUDA and software-decode NVENC |
| Actual rejected 580×1280, 518 frames | 41/1 / 41/1 | 90000/1 | Software decode/encode |
| Actual 720×1408, 426 frames | Not remeasured before | 24/1 | CUDA and software-decode NVENC |
| Actual 1080×1920, 4,258 frames | 30/1 / 30/1 (PTS quantized) | 30/1 | Software decode / guarded NVENC |

The final independently read-back batch has 22/22 outputs with matching rates,
every exact source PTS and frame count, dimensions and rotation (exit 0).
On the actual 17.75-second 24 fps source, final H.264 transcode is 0.805 seconds
CUDA vs 1.407 software decode; HEVC is 0.823 vs 1.376 seconds. This is one offline
pair, with normal uncontrolled background load and process launch included, not
a repeated median or coordinator/network latency claim. The existing decode
speed gain is retained for supported sources. Correctness takes priority for
irregular sources that fail the hardware rate contract.

The third actual source, 1080×1920 / 4,258 frames, passes at full length with
software decode and NVENC: 14.197s H.264 and 14.297s HEVC, every exact source PTS
retained. The first conservative policy selected CPU encoding for all VFR. Its
H.264 took 64.992s; its owned HEVC measurement was deliberately stopped/drained
at 216.746s before any installation (`stop-owned-measurement.log`). This aborted
measurement is not a passing gate. The final refinement retains guarded NVENC
encoding and passed all gates again. Its behavior test first failed (exit 1,
`guarded-encode-red.log`); the same 14 contracts then passed (exit 0).

The rejected source produces actual NVENC nominal-rate guard failures for both
codecs (validation exit 1), followed by successful CPU retries and final timing
validation exits 0. Supported VFR does not pay that CPU retry cost. These are
source-specific rejections; they do not remove the GPU profile from other jobs.

## Gates

Final application/test/script/lock source readback: **92 paths, zero hash
mismatches**, exit **0** (`source-proof-r2.log`, `expected-source.json`). PowerShell
and cmd line endings alone are normalized. Reviewed Boost 1.89.0 and the existing
MSVC 19.44 toolchain are reused; nothing is installed by the gate.

| Gate | Contracts | Numeric exits |
|---|---:|---|
| Linux Release | 267 / six suites | configure/build/discovery/every suite 0 |
| ASan + UBSan + leak detection | 267 / six suites | configure/build/discovery/every suite 0 |
| Windows MSVC Release | 322 / ten suites | extract/configure/build/discovery/every suite/SSH runner 0 |

Sanitizers instrument every compiled target with global compile/link flags;
leak detection and halt-on-error are enabled. See `linux-results.json`,
`windows-results.json`, per-suite logs and `windows-r2.log`.
`scripts/test.sh` exits 0 on the final Release build. The existing real media
smoke is recorded separately. No failure was skipped to permit landing.

## Landing, installations and coordinator acceptance

Merge and push exits **0**, fresh remote-main readback **0**:
`456eecca81635c30b7edb524165010f8fb9218ff` (`landing-results-r2.json`).

The package was independently downloaded and checked against its manifest,
three binary hashes and four updater scripts (`package-proof.json`). Both PCs
select and run **0.1.2-1b4042d3bbfa85cc** with source marker
`456eecca81635c30b7edb524165010f8fb9218ff`.

Full worker SHA256 on **each** machine:
`1b4042d3bbfa85cca58eeb4b585ca2bb0a39affa3c84600b09301ed0c6901c0f`.

| Machine | Install / independent verification exit | Running PID / session | Current-epoch state |
|---|---|---|---|
| Danny (installed first) | 0 / 0 | 36272 / 1 | connected, accepting; RTX 3070, RTX 3050 and CPU idle |
| Rick (installed second) | 0 / 0 | 19528 / 1 | connected, accepting; QSV, AMF and CPU available |

Final independent readbacks are `danny-final.log`, `rick-final.log` and
`rollout-results.json`. Both select the expected running image, SHA, source
revision, PID/creation-time runtime identity and desktop session. Fresh status
sidecars and latest connection events match the current process epoch. Both
installers verify unchanged credential, pairing-key and configuration bytes.
Neither machine was re-paired. Rollback markers are absent and the preceding
`0.1.2-db2bc15dc83612bb` binaries remain unchanged on both PCs.

### Remaining real app acceptance gate

**No new-build coordinator-accepted app job is claimed.** A read-only check
found no existing renditions for the originally rejected source. The prepared
single-job transaction then refused it because its parent item is deleted. The
transaction rolled back before INSERT; zero jobs were queued. The item was not
restored (`e2e-source-state.json`, `e2e-queue.json`). A global read-only query
found no live ready video without existing renditions or active work
(`e2e-safe-candidates.json`). Reprocessing an existing live video would replace
and schedule cleanup of its old renditions, conflicting with the user's no-data-
deletion constraint. A live disposable test item or explicit rendition-replacement
exception is required for this final gate. The original failed item stays deleted.

The earlier 24 fps attempt `f4b17fa4` is independently confirmed as coordinator
`succeeded`, app job `done`, remote validation route, but it ran on the **old**
build and does not count as new-build acceptance. The original `610735d5`
attempt remains rejected for frame rate; its local fallback job is failed.
See `coordinator-observation.json`. Worker ACK alone is not used as acceptance.

### Cleanup and operational boundaries

The one measurement/staging root on each PC was
`%LOCALAPPDATA%\Temp\ow-fps-20261018`; the local root was
`/tmp/ow-fps-20261018`. Cleanup exits **0** on Danny, Rick and Linux. Fresh,
independent installed-worker readbacks report **STAGING_EXISTS=False** on each
PC; an independent local process also confirms absence. Generated clips,
private measurement copies, archives and measurement executables are removed.
The running versions, identity, configuration and previous installed binaries
are retained. See `danny-cleanup.log`, `rick-cleanup.log`, `local-cleanup.log` and
the final machine readbacks.

No production container/service restart, migration, environment/Cloudflare
change, firewall/SSH change, dependency installation, machine PATH/policy change,
release publication, source-item restoration or production data deletion was
performed. Investigation used read-only SSH. The attempted acceptance action
performed reads and rolled back; it created no job. PC writes were the authorized
installations, normal worker activity and each single staging root.
