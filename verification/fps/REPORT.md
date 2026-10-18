# Video timing correction — October 18, 2026

Application revision: `1d21709963fda04349699ba6494a22cc24ef6adc`.
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
SDR/codec eligibility checks. Irregular inputs use the existing CPU encoders;
libx265 receives an explicit representable source rate. Ambiguous fractional VFR
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
`after-ffprobe.log` and native timing logs. Personal media stays in the single
measurement directory, never in this repository or fixtures.

| Source | Old CUDA / old software-decode nominal rate | Corrected rate, both video outputs | Path |
|---|---|---|---|
| Generated integer, 90 frames | 30/1 / 30/1 | 30/1 | CUDA and software-decode NVENC |
| Generated fractional, 90 frames | 30000/1001 / 30000/1001 | 30000/1001 | CUDA and software-decode NVENC |
| Generated VFR, 54 frames | 30/1 / 30/1 | 30/1 | Software decode/encode |
| Rotated fractional, 90 frames | 30000/1001 / 30000/1001 | 30000/1001 | CUDA and software-decode NVENC |
| Actual rejected 580×1280, 518 frames | 41/1 / 41/1 | 90000/1 | Software decode/encode |
| Actual 720×1408, 426 frames | Not remeasured before | 24/1 | CUDA and software-decode NVENC |

The first independently read-back batch has 20/20 outputs with matching rates,
every exact source PTS and frame count, dimensions and rotation (exit 0).
On the actual 17.75-second 24 fps source, final H.264 transcode is 0.807 seconds
CUDA vs 1.392 software decode; HEVC is 0.858 vs 1.376 seconds. This is one offline
pair, with normal uncontrolled background load and process launch included, not
a repeated median or coordinator/network latency claim. The existing decode
speed gain is retained for supported sources. Correctness takes priority for
irregular sources that fail the hardware rate contract.

A third actual source, 1080×1920 / 4,258 frames, is being checked at full length.

## Gates

Final application/test/script/lock source readback: **92 paths, zero hash
mismatches**, exit **0** (`source-proof.log`, `expected-source.json`). PowerShell
and cmd line endings alone are normalized. Reviewed Boost 1.89.0 and the existing
MSVC 19.44 toolchain are reused; nothing is installed by the gate.

| Gate | Contracts | Numeric exits |
|---|---:|---|
| Linux Release | 267 / six suites | configure/build/discovery/every suite 0 |
| ASan + UBSan + leak detection | 267 / six suites | configure/build/discovery/every suite 0 |
| Windows MSVC Release | 322 / ten suites | extract/configure/build/discovery/every suite/SSH runner 0 |

Sanitizers instrument every compiled target with global compile/link flags;
leak detection and halt-on-error are enabled. See `linux-results.json`,
`windows-results.json`, per-suite logs and `windows-final.log`.
`scripts/test.sh` exits 0 on the final Release build. The existing real media
smoke is recorded separately. No failure was skipped to permit landing.

## Landing, installations and coordinator acceptance

Pending. The package has been independently downloaded and checked against its
manifest, three binary hashes and four updater scripts (`package-proof.json`).
Worker SHA256 is `e45b21c7bba070cf5cc31710b128c5e6ea97eda8c2fc9c3e9627265cbe7b490b`.
No installed-version, connected-state, coordinator acceptance or cleanup success
is implied until the corresponding readback below is recorded.
