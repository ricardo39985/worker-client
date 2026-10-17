# Candidate status — 0.1.2

Retained FFmpeg distribution: see
`verification/ffmpeg-dependency/REPORT.md` and `docs/ffmpeg-distribution.md`.
The expired autobuild is replaced by an exact executable pin and retained-artifact
installation path. Publication is explicitly pending; fresh automatic conversion
remains blocked until the operator publishes and reviews the two lock URLs.
Linux gates pass on an isolated dependency-only snapshot; Windows PowerShell/MSVC,
actual retained download, redistribution review and native probes remain unrun.
No GPU behavior changes or running-host actions belong to this branch.
Current multi-GPU source candidate: see
[verification/multigpu/REPORT.md](../verification/multigpu/REPORT.md) for the
code diagnosis, red/green gates, per-card session policy and operator checklist.
This change has no MSVC/native or physical RTX 3070/3050 execution evidence.
No Windows machine or production server was contacted, changed or restarted.

Current geometry correction evidence: see
`verification/gpu-dimensions/REPORT.md`. The sample size no longer sets encoder
limits; startup tuning is 1920x1080 and larger hardware work uses guarded
validation, targeted demotion and CPU retry. Portable and sanitizer evidence is
recorded there. Windows/MSVC and physical GPU checks remain unrun for this tree.
No worker was contacted or restarted. Historical verification narratives below
do not establish native verification for this change.

Updated October 7, 2026. This source candidate adds bounded dependency/build
repair, verified startup fallback, pinned FFmpeg installation/probing, exact
storage-host approval and explicit expired/unreadable pairing recovery. It carries
forward the 0.1.1 durable endpoint/proof/journal work because the remote main
snapshot `d843cfa106a80cefa67cd6fc70fa427683c9e406` still contains the original 0.1.0
implementation. Source changes must not be mistaken for a compiled release.

## Historical execution

The 0.1.1 portable suite passed 71 contracts and ASan/UBSan; original recorded
commands/counts remain in `verification/client-update/`. The user later reported
passing Windows compilation, all three registered CTest suites and the runtime
crypto/storage/SQLite doctor for 0.1.1. Setup exposed an account-alias startup
verification error; this candidate includes SID resolution and startup fallback.
Successful Windows credential redemption and a real remote conversion were not
conclusively established from those logs.

## Candidate execution and open gates

This workspace has no PowerShell, Windows/MSVC, CMake or Docker runtime. The new
Windows source, 23 PowerShell behavior cases, actual dependency installation,
Task Scheduler/Startup-folder fallback, reboot, DPAPI recovery and FFmpeg probe
are authored but unrun here. The existing portable suite was not rerun for this
candidate. Its recorded historical results are not a new native pass.

The companion server candidate passed 50 Python coordinator/storage/release/infra
contracts locally, including real Linux H.264/JPEG validation. Its new C++ gateway and PostgreSQL integration harness are
unrun. The harness uses simulated worker execution and cannot establish Windows
job execution. The actual PC must run setup's mandatory CTest and doctor gates,
then complete a targeted small job and receive a durable result ACK.

## Deployment boundary

The existing Lightsail native pairing service is deployed at protocol 1 with
job_dispatch:false. New job dispatch is behind a separate disabled-by-default
flag and must go through the Organizer GitHub release flow. This ZIP contains
Windows source only. Operator-queued test results are separate temporary objects;
automatic app rendition offload, inference and GPU processing are outside scope.
No new production deployment, compiled/signed release, paid CI activation or
repository settings change has been performed for this candidate.

## Terminal progress draft — October 7, 2026

The newer operator deployment logs confirm Organizer commit
`0f694d0720d37b79258d43c3c1988727df53acb4` deployed with migration 013 and
healthy services. Windows setup reported job dispatch enabled and received
explicit storage-host approval. A later operator screenshot at 18:20 UTC shows
DESKTOP-3FR54CB online and paired. This supersedes the earlier deployment
boundary above; it does not prove a successful conversion or complete native
acceptance.

This draft adds live setup/worker progress and removes the second Boost tree
copy. Eight isolated portable progress contracts passed, including ASan/UBSan
with leak inspection disabled due to a runtime limitation. Fifteen new
PowerShell progress cases, Windows syntax/MSVC integration and a real job remain
unrun for this change. See docs/testing.md for exact evidence. Production and
the operator's running installation have not been changed by this draft.



## Automatic app rendition review candidate

The operator subsequently confirmed a real diagnostic image conversion,
acknowledgment, server state done and a visually correct JPEG. That confirms
that deployed diagnostic path, not ordinary upload offload or this new source.

This candidate adds separate probed `media.video.renditions.v1` and
`media.image.renditions.v1` capabilities, fixed full-size app encoders and
per-artifact direct storage uploads/progress. The current server companion
routes ordinary optimization jobs outside the API and validates/publishes in
an isolated background worker. Unsupported PCs use local fallback.

Three standalone command-profile tests and a Linux synthetic media smoke run
pass. Full Boost-dependent core tests and actual Windows/MSVC/PowerShell builds
are unavailable in this authoring runtime. This source is not installed on the
operator's machine and neither repository's new feature is merged or deployed.
See docs/testing.md for commands and required native acceptance.

## Truthful progress review candidate — October 17, 2026

Conversion progress now uses FFmpeg source duration and `out_time_us`, not output
file bytes. The controller continues to own plain rotating durable logs; the
separate viewer owns stable ASCII progress rows, category colors and redraw.
Redirected output, unavailable consoles and `NO_COLOR` use plain append-only
output. Unknown extents are described once, retries carry a visible run number,
and terminal/stage transitions remove stale rows.

All four Linux CTest executables passed in Release and under ASan/UBSan with leak
detection: 74 core contracts, 9 pipeline contracts, 3 rendition contracts and
the progress/console executable. The installed-FFmpeg synthetic rendition smoke
also passed. The Windows-native rotation contract covers 1,800 concurrent long
lines across the existing cap, but it and the changed worker/viewer have not been
compiled or run with MSVC here. No installation, startup change, deployment,
merge, release publication or running-worker restart was performed.

## Readable console review candidate — October 17, 2026

The second readability pass keeps the durable log as UTC/millisecond plain text
with complete attempt ids, while the viewer renders short `dd/mm hh:mm:ss` local
timestamps and `job <last-four>` labels. Live suffix collisions grow the label;
finished labels can be reused. Meaningful transitions are one terse line, live
progress stays in bounded dashboard rows, and every rendered line is capped at
80 columns. Five semantic colors are fixed to failure, warning/attention,
success/ACK, job activity and neutral system text. On 16-color Windows, orange is
dark yellow (`FOREGROUND_RED|FOREGROUND_GREEN`).

All four Linux CTest executables passed in Release and under ASan/UBSan with leak
detection: 74 core, 9 pipeline, 3 rendition and 15 progress/console contracts
(101 total). The installed-FFmpeg synthetic rendition smoke also passed. The
Windows console/controller/worker changes remain uncompiled and unrun in this
authoring environment; native MSVC, a real 16-color console, viewer redraw and a
live job remain required. No installation, startup change, deployment, merge,
release publication or running-worker restart was performed.

<!-- Inherited hardware-branch narrative below is UNVERIFIED for tip 1edeada.
No Windows evidence here establishes this merged tree or current hardware support. -->
## Adaptive Windows hardware candidate — October 17, 2026

The initial paragraphs below describe the rejected first implementation and
first review candidate. They are historical evidence only; the uncommitted
Correction 2 section at the end is the current implementation and replaces
their hardware outcome and candidate path.

The uncommitted candidate adds a fail-closed per-device NVENC, QSV and AMF
profile policy, explicit encoder/device command generation, DXGI identity/driver/
dedicated-memory inventory, live VRAM sampling, startup synthetic tuning,
persistent profile evidence, runtime invalidation and clean CPU retry before
upload. Protocol messages, leases, output roles and bounded encode/upload overlap
remain unchanged. Injected policy tests cover CPU fallback, codec differences,
heterogeneous adapters, caps, invalidation and failed/cancelled hardware attempts;
those fixtures are not claims about physical GPU support.

Native verification used an entirely separate candidate tree at
`C:\Users\reald\AppData\Local\ow-adaptive-hardware-candidate-20261017-r2` and
the reviewed Boost 1.89.0 dependency. MSVC 19.44 Release compilation with parallel
2 succeeded. CTest passed all 8 registered suites, including the native adapter
inventory and 28 adaptive policy contracts. The synthetic native pipeline smoke
encoded and decoded H.264, HEVC and WebP; serial and overlapped artifacts had
matching byte counts and SHA-256, and overlap was observed. Its 500 ms simulated
transfer delay was local only, with no storage account. Logs and profile evidence
are under `verification/adaptive-hardware/windows/`.

The isolated approved-FFmpeg Doctor probe exited 0 on the reachable Windows host:
Intel Core i5-8305G, 8 logical CPUs, 8,002 MiB RAM, Radeon RX Vega M GL (DXGI
dedicated memory 4,043 MiB) and Intel HD Graphics 630 (128 MiB). The Doctor
verified CPU H.264, HEVC, JPEG, full-size app video renditions and image
renditions; two CPU profiles were saved at one-job concurrency. The synthetic
CPU samples measured about 108.3 H.264 fps and 25.7 HEVC fps. These are machine-
and sample-specific tuning observations, not speedup claims. AMD AMF H.264/HEVC
functional trials failed and were left disabled. Intel QSV was withheld below
the 256 MiB dedicated/live-headroom floor. NVIDIA was not detected. No GPU profile
was enabled. The pictured RTX 3070/3050 system was not available or tested.

Before each synthetic probe the protected worker PID 28136 remained present with
no FFmpeg process under it. Doctor data and smoke work stayed under the isolated
candidate path; no production worker config, pairing, job state, startup, or
service was changed. This does not verify non-elevated tray behavior, a real
coordinator job, upload/publication, or quality parity on any GPU.

The required local `scripts/test.sh` attempt is recorded but could not configure
because this Linux authoring environment lacks `boost/json/src.hpp`; no portable
full-suite pass is claimed for that command. The separately compiled adaptive
policy binary passed 13/13 contracts. Exact red/green, blocked-suite, MSVC/CTest,
Doctor and synthetic smoke logs are:

- `verification/adaptive-hardware/01-policy-red.log`
- `verification/adaptive-hardware/02-policy-green.log`
- `verification/adaptive-hardware/03-command-red.log`
- `verification/adaptive-hardware/06-backend-green.log`
- `verification/adaptive-hardware/07-adaptive-policy.log`
- `verification/adaptive-hardware/08-portable-test.sh.log`
- `verification/adaptive-hardware/windows/build2.stdout.log`
- `verification/adaptive-hardware/windows/ctest.stdout.log`
- `verification/adaptive-hardware/windows/adaptive-probe.stdout.log`
- `verification/adaptive-hardware/windows/adaptive-hardware-final.json`
- `verification/adaptive-hardware/windows/pipeline-smoke.stdout.log`

This is source only, remains uncommitted for independent review, and is not
production acceptance. Real NVIDIA hardware, a successfully probed AMD backend,
concurrent multi-GPU tuning, non-elevated user-session verification and device-
specific output-quality validation remain open.

### Independent review corrections — October 17, 2026

The first candidate was rejected after review. This working correction replaces
its DXGI `CurrentUsage` sampling and family-ordinal assignment claims: that API
reports the caller's usage, and DXGI order is not a verified FFmpeg backend
identity. The corrected inventory records stable DXGI IDs but marks backend
mapping unproven and withholds those GPU profiles with an explicit reason. The
approved runtime has no child-scoped per-adapter VRAM telemetry source, so GPU
use remains disabled until both mapping and telemetry can be established.

The correction adds an atomic adaptive scheduler used at BEGIN, including the
measured CPU retry profile, task CPU/RAM/scratch caps, global and per-device VRAM
accounting, deterministic stable-ID ties, and drain-bound RAII reservations.
Setup inventories before converter work and persists a NoConversion inventory.
`limits.vram_mb` is parsed and recorded as the operator's global VRAM ceiling;
an absent value selects a conservative aggregate default and explicit zero
disables GPU scheduling. CPU concurrency is measured with repeated bounded
one- and two-process candidates when configured caps permit.

### Correction 2 — real backend identity and telemetry — October 17, 2026

The candidate now maps NVIDIA CUDA ordinals through the installed driver
`cuDeviceGetLuid` API and maps QSV/AMF commands to explicit DXGI devices. Windows
PDH GPU Process Memory Dedicated Usage is matched to each child PID and LUID;
runtime admission refreshes per-device memory headroom. Unsupported telemetry
keeps that device on CPU. GPU profiles record the tested 640x360 envelope;
runtime probes input dimensions and selects CPU for unknown or larger frames.

The regression first failed as expected because a second task exceeded its
512 MiB per-task RAM cap when existing aggregate usage was included. It is fixed:
four 1 CPU/512 MiB jobs are accepted under global CPU4/RAM2048, and the fifth is
rejected. The portable CTest suite passed 5/5; MSVC 19.44 Release built with
parallel 2 and CTest passed 8/8. The isolated Doctor passed on the actual
i5-8305G PC and enabled AMD AMF HEVC at 640x360 after encode, decode and matching
child dedicated-memory telemetry. The final Doctor reported AMD AMF HEVC at
71.713 fps and CPU HEVC at 53.933 fps for this synthetic sample. H.264 did not
beat CPU by the 5% selection floor. Intel QSV ran its encode command but lacked
a nonzero dedicated-memory sample; it remains disabled. No NVIDIA device is
installed. AMF quality parity is not measured; no 4K envelope or RTX 3070/3050
hardware result is claimed.

The final isolated serial/overlap/retry smoke passed; the original worker PID 28136
was present and idle before Doctor and smoke, and no live FFmpeg process was
present. No service, live config, pairing, startup or job state was changed.
The final native candidate tree was
`C:\Users\reald\AppData\Local\ow-adaptive-hardware-candidate-20261017-r4`.
See `/home/rick/.hermes/cache/scratch/worker-adaptive-hardware-implementation-r2.md`
for exact logs, counts and remaining hardware gates. The candidate is uncommitted
and is not production acceptance.

### Final correction gate R3 — October 17, 2026

The candidate now treats absent, foreign-LUID, invalid and zero GPU counters as
unavailable; explicit `vram_mb: 0` skips GPU encoder identity initialization and
GPU probes; positive-cap probes reserve live per-device/global headroom and
abort/drain when observed child allocation exceeds that reservation. These are
admission limits with sampled watchdogs, not a hard Windows GPU-memory quota.
CPU concurrency candidates use equal work, repeats, encode/decode stages and
per-child limits; measured stage timings/configuration are saved for review.
GPU jobs reserve CPU fallback resources without occupying the CPU encoder slot;
CPU fallback waits within a bounded cancellation-aware interval. Denied BEGIN
attempts become durable `never_started:<reason>` journal states without outbox
results. NoConversion inventory includes processor, logical CPU count, total and
available RAM, disk and adapter/driver/dedicated-VRAM fields.

Portable `scripts/test.sh -DOW_BOOST_ROOT=/home/rick/.hermes/cache/scratch/boost-extract`
passed all 5 CTest suites. The adaptive contracts passed 32/32 and the core
worker/journal contracts passed 75/75. Expected behavior RED evidence covers
missing GPU telemetry, the one-CPU-slot/two-GPU admission case, unequal tuning
comparison inputs and denied-BEGIN persistence. Isolated MSVC/CTest/Doctor and
production-scheduler runtime smoke remain pending: the protected worker PID
28136 was present, but its current-epoch log showed 3 unmatched START records
when the final idle gate ran. No Windows native tests, Doctor, GPU probes or
smoke were started against that busy worker. This R3 source remains uncommitted;
portable contracts do not establish Windows behavior or physical GPU support.


## Operator truthfulness and probe visibility — October 17, 2026

See [the audit and gate ledger](../verification/truthfulness/REPORT.md) for the
findings, exact commands and Windows checklist. The portable gate passed 5/5
suites: 75 core, 9 pipeline, 3 rendition, 48 adaptive and 28 progress/console
contracts (163 total). Core/adaptive and a separate strict progress build also
passed ASan/UBSan with leak detection. No MSVC/native/PowerShell or physical GPU
check ran; these are source changes with Linux evidence only. Work stayed on
`feat/gpu-on-logs`; nothing was pushed, installed, deployed or restarted.

Probe output means startup sample evidence, not production success or arbitrary
input support. Heartbeat `verified` retains that protocol meaning; app video
reporting also requires current CPU H.264/HEVC fallback profiles. Runtime hardware
failure suppression affects the reporting ledger only, leaving device selection,
resource policy and scheduler behavior unchanged.
