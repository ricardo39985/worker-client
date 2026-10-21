# Organizer Windows worker — 0.1.2 source candidate

Current unattended-owner implementation: [secure enrollment, architecture and
separate acceptance gates](docs/unattended-owner.md). Existing-owner limited
headless engine, optional attached tray and bounded `start/restart` are candidate
source, NOT a verified before-login installation. No boot/logoff/lock or owner
session disruption is authorized. The prior CLI was deployed on both PCs;
see [exact prior release receipts](verification/cli/RELEASE.md). Historical
module-specific snapshots below do not override those current ledgers.

Worker-flex stage-concurrency checkpoint: [implementation, enforced/sampled limits and gates](verification/worker-flex-r2/REPORT.md).
Linux Release and both-PC MSVC/native suites pass. Throughput, foreground impact,
exact-asset replay and rollout acceptance remain separate open gates. The operator
has authorized the related Windows experiments; installed workers are unchanged.
The [first-pass investigation](verification/worker-flex/REPORT.md) is historical.

Current frame-timing correction: [measurement, gates and rollout ledger](verification/fps/REPORT.md).

Current installer swap and both-PC rollout: [Windows rollback proof and installed readbacks](verification/installer-swap/REPORT.md).

Current merged decode and completion-reporting gates: [fresh measurements and landing ledger](verification/finish-perf/REPORT.md).
This ledger supersedes the historical candidate snapshots below.

Measured decode continuation: [per-stage performance, quality and gates](verification/gpu-performance/MEASURED.md).
The isolated RTX 3050 candidate preserves output bytes while reducing CPU time;
Normal-priority offline job speedups are 1.123× at 1080p and 1.047× at 4K.
MSVC and all ten native suites passed. The test workspace is deleted; this source
was not installed and does not establish production-job acceptance.


Current GPU child commit budget fix: see [the gate ledger](verification/gpu-budget/REPORT.md).
Linux Release and ASan/UBSan each passed 246 contracts across six suites;
MSVC Release on the authorized Windows host passed 293 contracts across nine
suites, all final gate exits 0. The cap is retained and workload sized. The
supplied physical 3070 A/B proves 768 MiB at 1080p; fresh hardware replay was
deferred because separate FFmpeg measurements were running. Larger-resolution
and concurrent physical qualification remain open. No worker was installed or
restarted, and no production action was performed.


Current NVENC retry verification: see [the landing gate ledger](verification/nvenc-landing/REPORT.md).
Review corrected a missing Windows include. Linux Release and ASan/UBSan each
passed 239 contracts across six suites; isolated MSVC Release passed all nine
native suites (285 contracts), with every final gate exit 0. Physical two-card
recovery remains unrun. No worker installation, production restart or deployment
was performed. Earlier read-only investigation and separate hardware-proof
authorization boundaries remain in [the investigation](verification/second-card/REPORT.md).

Current worker-console reporting evidence: see
[the log-content gate ledger](verification/log-content/REPORT.md) and
[the reporting contract](docs/worker-console.md).
The controller counts local outcomes, emits complete stats, and publishes an
idle table on independent live/durable cadences. This change has portable source
verification only; MSVC, native console rendering and actual Windows jobs remain
unrun. No Windows machine or production environment was contacted or changed.


A native Windows tray worker with typed conversion jobs, durable results and
operator-approved pairing. This candidate contains source, not a compiled or
signed Windows release. Adaptive hardware corrections passed isolated native
verification; see the status and testing documents for evidence and limits.

## Setup

Exit the existing worker using its system-tray Exit menu. Extract this ZIP and
run `setup.cmd` from the `worker-client` folder as your normal signed-in Windows
user. Do not run the whole setup as Administrator.

```powershell
.\setup.cmd
```

Healthy installed prerequisites and configuration are reused. Setup accepts a
bare DNS name and canonicalizes it to HTTPS; enter `rick-organizer-api.duckdns.org`
for this deployment, or your intended coordinator. Only verified HTTPS on port
443 is supported. There is no embedded production default.

Setup offers missing Microsoft C++ tools and SDK installation with a license
prompt and UAC only for those tools. It downloads dependencies from the reviewed
lock, verifies their hashes, repairs damaged generated caches, builds and runs
all registered CTest suites. A required reboot is reported; rerun the same command
after reboot. No global PATH or execution-policy changes are made.

Conversion setup offers the pinned FFmpeg build after explicit license approval
and runs real sample conversions before enabling its capabilities. It discovers
storage hosts through the selected coordinator and asks you to approve exact
hosts. Missing dependency downloads, damaged extraction, build cache failures
and startup registration/readback have bounded repair paths. Persistent failures
stay visible in the setup window and reports.

The approved FFmpeg executable and its exact FFmpeg source, BtbN build recipes,
licenses and rebuild notice are now hosted together on the retained dependency
release. Fresh downloads preserve the reviewed executable checksum and explicit
license consent. See [the distribution record](docs/ffmpeg-distribution.md) for
public-download evidence, the notice's third-party source-scope limitation and
separate unrun Windows installation/probe gates. Existing approved converters
remain usable; `-NoConversion` is an explicit pairing-only choice.

Normal-user startup is verified using SID account identity. If Task Scheduler is
unavailable, setup uses a verified shortcut in this user's Startup folder. It
starts the tray worker and checks the process image and readiness marker. Closing
the console leaves jobs running; use the tray to pause, set priority or exit.
Startup runs at interactive Windows sign-in, not as a privileged boot service.

## Pairing and job test

A healthy existing identity is retained. If enrollment expired, setup offers an
explicit fresh pairing code with the existing key. Moving endpoints or unreadable
identity requires explicit archival; keys/journal/media are backed up before
starting a new coordinator scope. Recovery never silently discards identity or
unacknowledged work. Revoke abandoned credentials on their old server.

On Lightsail, run `python3 /opt/organizer/scripts/worker_tui.py`, choose P, enter the
**current** Windows code, press Enter, review the machine, type PAIR and press
Enter. Look for `PAIRING APPROVED: protected identity saved` and `CONNECTED` on
Windows. A code shown earlier is not reusable after its expiry.

Remote conversion requires `job_dispatch:true` from the selected coordinator.
Deploy server changes through the companion Organizer GitHub release flow.
No server overlay is included in this Windows package.
See the companion `docs/native-worker-jobs.md` for exact deployment and first-job
commands. A successful job shows Windows START, RESULT PENDING ACK and RESULT
ACKNOWLEDGED; confirm `state:done` and inspect its separate downloaded output on
Lightsail. ACK alone also covers a rejected result; it is not proof of success.

This first job path is explicitly queued by an operator. It leaves the app's
primary media and existing renditions intact. Outputs are temporary, retained
for about 24 hours from attempt creation. The adaptive media path can select a
local NVENC, QSV or AMF encoder only when backend identity mapping,
synthetic encode/decode and bounded tuning are verified. VRAM admission uses
live telemetry when available, otherwise a conservative declared-VRAM budget.
Unproven mapping or no usable VRAM budget keeps that GPU profile disabled while
measured CPU profiles remain available. There is no inference adapter.

## Scriptable local control

The new `OrganizerWorkerCtl.exe` controls the already-running updated controller
from the same owner's PowerShell/SSH session without a GUI: `--json status`,
`pause`, `drain`, `resume`, and typed atomic `config set --stdin`. It also exposes
all existing priority/preferences, limits, viewer/log/history and exit controls.
Cold settings report restart required and never restart jobs automatically.
See the [complete command/settings reference and SSH examples](docs/cli.md).
For an already-upgraded, running worker (same owner, including an SSH PowerShell
session):

```powershell
$root = Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$current = Get-Content -LiteralPath (Join-Path $root 'current.json') -Raw | ConvertFrom-Json
$ctl = Join-Path $current.directory 'OrganizerWorkerCtl.exe'
& $ctl --json status
# Explicit operator controls: & $ctl pause ; & $ctl drain ; & $ctl resume
```

The reference also provides a quoted Linux SSH heredoc example. Older installed
pre-CLI workers have no control IPC. The prior CLI release is installed on both
PCs; the new unattended implementation does not change those live workers.
Native verification gates are recorded in
[the CLI ledger](verification/cli/REPORT.md).

## Maintenance and limits

```powershell
.\setup.cmd -CheckOnly
.\setup.cmd -RePair
.\setup.cmd -NoConversion
```

`-NoConversion` explicitly installs pairing-only operation. `-NoStartup` and
`-NoLaunch` are maintenance opt-outs. Noninteractive installs require explicit
license/host consent flags described in `docs/setup.md`. An existing worker's
resource limits are retained; setup reports free memory after the Windows reserve.
Fresh installations use available physical RAM with `reserve_ram_mb: 0`.
An existing configured reserve is retained until the operator changes it.
The server's initial offers require 512 MiB for video or 256 MiB for images, one
CPU and sufficient scratch. Paused/disabled/busy machines do not receive starts.

This package targets x64 Windows 10 build 19041 or later, including Windows 11.
ARM64, 32-bit and older Windows need a separate supported build; setup reports
that boundary instead of installing incompatible software. Missing or damaged
Windows OS components require Windows repair; setup never obtains random DLLs.

Installation lives under `%LOCALAPPDATA%\OrganizerWorker`. Reports are written
to `verification/windows-setup.json` and the installed `logs/setup-report.json`.
Live setup progress is also written to `logs/setup-progress.log`; see
`docs/setup.md` for stage counters, quiet warnings and timeouts.
Candidate configuration is staged before publication. Files publish atomically
one at a time; the complete installation is not a transactional rollback.

## Verification

Historical 0.1.1 portable evidence is in `verification/client-update/`. The user
reported passing the prior Windows compilation, three CTest suites and runtime
doctor. The October 17 adaptive-hardware correction passed the portable suite,
MSVC compilation and all eight registered native CTest suites. Its isolated
approved-FFmpeg Doctor verified CPU profiles and one AMD AMF HEVC profile on the
available Windows host after adapter mapping, child-scoped dedicated-memory
telemetry, encode and decode checks. Intel QSV remained disabled because its
child did not report dedicated-memory allocation; no NVIDIA adapter was present.
Clean-machine/bootstrap/reboot paths, GPU quality parity and actual
Windows-to-Lightsail conversion remain separate acceptance gates. See
`docs/status.md`, `docs/testing.md`, `docs/setup.md` and `contracts/worker-v1.md`.

## Terminal progress candidate

Setup now streams native output, counts download/checksum bytes and dependency
files, labels prompts, and reports elapsed time in quiet native stages. Boost
extraction moves the generated tree into staging instead of copying it again.
The worker now derives conversion progress from FFmpeg's reported media position
against the source duration; output size remains only a safety limit. Transfer
and verification retain exact byte totals. The separate console viewer turns
plain durable progress records into stable in-place ASCII bars. It prints one
short transition line at a time, identifies a job by `job ` plus the last four
attempt characters, uses `dd/mm hh:mm:ss` local console timestamps, and caps all
rendered lines at 80 columns. Five semantic colors are used; redirected output
and `NO_COLOR` remain plain append-only event text. Unknown totals show only the
stage name and elapsed time, without an invented percentage.

The current Linux run passed all four portable CTest executables (74 core, 9
pipeline, 3 rendition and 15 progress/console contracts), the same 101 contracts
under ASan/UBSan with leak detection, and the synthetic H.264/HEVC/WebP decode
smoke. The Windows console/controller changes and native log-rotation contract
remain uncompiled and unrun here. This is source evidence, not a verified or
installed Windows release.



### Automatic app media candidate

This branch adds fixed full-size H.264/HEVC/WebP rendition capabilities for the
Organizer background coordinator. It retains the installed identity and operator
preferences. Direct R2 transfers still require explicit approved hosts. The
companion server feature defaults to off; old diagnostic capability names do not
qualify a PC for ordinary app jobs. Read contracts/worker-v1.md and the new
verification section in docs/testing.md before treating this as a release.


### GPU dimension correction candidate

Adaptive encoder ceilings now come from conservative backend policy, separately
from the recorded 1920x1080 tuning resolution. Larger supported inputs use guarded
full-size encoding and validation; a failed device/codec is demoted once and
retried on CPU. The console names the selected encoder and distinguishes geometry,
unproven size and measured throughput fallback. See
`verification/gpu-dimensions/REPORT.md` for local gates and the unrun Windows checks.
