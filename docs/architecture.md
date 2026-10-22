# Architecture decisions

## Control and execution

Lightsail remains the authoritative owner of job state, worker authorization,
routing policy, attempt numbers and leases. This repo is a replaceable compute
client. It must not make itself the production scheduler or access PostgreSQL
remotely. Database migrations and the gateway belong in the Organizer repo on a
reviewed feature branch. Existing saves must continue when all PCs are offline.

The Windows controller owns a persistent outbound WSS connection using native
WinHTTP asynchronous completions. Media uses separate bounded HTTPS streaming
requests, not WebSocket payloads and not the Lightsail application data path.
Only exact operator-approved storage hosts are accepted. Each output URL must
point at a new per-attempt staging object. The coordinator alone publishes it.

## Lightweight native stack

C++20 with MSVC release optimization, checked optional link-time optimization,
static MSVC CRT, and no Electron, browser shell, managed runtime or Linux VM.
Boost.JSON is used for typed bounded protocol parsing; its release source is
pinned to 1.89.0 by SHA-256. WinHTTP uses Windows TLS/certificate handling;
BCrypt provides random generation, SHA-256 and pairing signatures; DPAPI protects
credentials under the normal user's account. A local WinSQLite WAL/FULL journal
stores accepted attempts, settings and pending acknowledgments. Native Shell32
and User32 implement the tray and small menus. Windows Job Objects contain
conversion process trees and impose CPU/memory budgets.

These are implementation choices, not measured performance claims. Native
Windows profiling and dependency/security review remain release gates. The
Build Tools bootstrap follows Microsoft's signed VS2022 channel, not a perfectly
reproducible fixed compiler build. The OS services follow installed Windows
updates; their availability and relevant behavior are checked at runtime.

## Unattended existing-owner candidate

[Full lifecycle, credential and acceptance contract](unattended-owner.md).
`--headless` validates the existing normal-owner profile/DPAPI/state and selected
image before starting the same Worker/lock/owner-only pipe; no UI or implicit
pairing. A separately enrolled Windows Password/Limited boot task runs one fixed
reviewed launcher. The optional `OrganizerWorkerTray.exe` only attaches; it never
owns jobs and logoff/close cannot forward a stop. Legacy interactive startup
remains preserved, not a before-login claim. No SYSTEM/admin engine, S4U, autologon,
foreign-SID migration, arbitrary privileged broker or plaintext credential path.

Restart admission fencing is transient, distinct from durable desired pause.
Exit waits actual child trees, admitted work and journaled reservation/running/
pending states through matching outbox ACK. Journal rows close the expiry-to-result
publication race even if the admission map is already empty. Bounded pending is
not permission to kill/re-run work. Actual owner/image/hash/token, connected and
processing_ready=true are required; task submission is not readiness. This source
policy still requires real Windows enrollment and separately approved fresh-boot
hardware/profile/TLS acceptance. No such boot action is authorized now.

## Console lifetime

OrganizerWorker.exe is a GUI-subsystem tray/controller executable. It starts
OrganizerWorkerConsole.exe with its own console. The console only reads the
log and watches the controller process handle. Closing the console does not
send a stop signal to the controller. The controller doesn't share that console.
The tray can reopen a viewer, pause new starts, change preference/priority, open
logs, and finish-or-stop on exit. The controller must run as the signed-in,
non-elevated Windows user. SSH can build or update it but cannot create a visible
tray in its service session. Optional startup is an interactive logon task,
not a boot service; user sign-in is required for that LEGACY mode. The new
headless entry above is separate and must pass its own gates.

The controller's durable log supplies the console's events and progress. Atomic
read-only status/history files supply the latest idle frame and requested recent
outcomes; the viewer has no control channel. The durable log contains
plain timestamped event and progress records, never terminal escape sequences.
Durable lines keep a UTC millisecond timestamp and the complete attempt id, so
they remain unambiguous evidence and a displayed suffix can be grepped back to
the real attempt. The viewer owns the short local presentation, color and
in-place dashboard redraw, so closing or crashing it cannot signal the controller
or its contained process trees.

Ordinary console lines and idle table rows are capped at 80 columns and target
roughly 60; the specified complete one-line stats report is retained without
truncation. See [worker console reporting](worker-console.md) for its fields and
independent 30-second live / five-minute durable idle cadence. The viewer renders
`dd/mm hh:mm:ss` in local time, shows a job as `job ` plus the last four attempt
characters (`job 93c7`), and grows that suffix only while another live job would
otherwise be indistinguishable. One short line records a meaningful transition;
per-poll progress stays in the dashboard, and a stage without a defensible total
shows only its name and elapsed time. Only five semantic colors are used: red
failure, orange warning/retry/cancellation/attention, green success/ACK, blue
ordinary job activity/progress and white system/configuration text. White uses
the terminal default foreground. On a 16-color Windows console orange is the
closest legible analogue: dark yellow (`FOREGROUND_RED|FOREGROUND_GREEN`, ANSI
33), which retains contrast on both dark and light backgrounds. Redirected
output remains plain append-only text with no escape codes and includes the
slower durable status blocks. `NO_COLOR` keeps in-place rows uncolored using
Win32 cursor operations; redirected output suppresses dashboard-only progress rows. Legacy consoles
use Win32 cursor/color operations and ASCII bars instead of assuming
virtual-terminal support.

Conversion progress is an execution measurement, not a file-size estimate.
Typed FFmpeg invocations enable its machine-readable progress stream; the worker
tails the contained child log, parses source duration and monotonic `out_time_us`,
and floors the ratio at 100 percent. Transfer and verification continue to use
exact byte totals. Stage generations make a retry reset explicit, and stages
without a defensible total emit one written explanation rather than a percentage.

## Local control CLI candidate

`OrganizerWorkerCtl.exe` is a separate console client, never a second engine.
Candidate `start/restart` activate only the exact enrolled owner task with bounded
actual readiness; ordinary status/config/control still require the live pipe. An owner-SID authenticated, remote-rejecting named pipe reaches the existing
controller across logon sessions without window messages or a public listener.
Tray and CLI share typed handlers and one atomic configuration document; legacy
SQLite hot preferences migrate once into `worker.json.operator`. Cold file values
stay pending until an explicit restart; no active jobs are implicitly restarted.
The viewer and its status files remain observational only. See [CLI reference](cli.md)
and [verification gates](../verification/cli/REPORT.md); cross-session Windows
acceptance and real jobs are distinct from portable tests or the idle fixture.

## Independent concurrent jobs

One logical task is one job; a retry is a different attempt. An offer reserves
capacity atomically before readiness is acknowledged. It cannot start until
BEGIN supplies a valid lease. Reservations account for RAM, CPU shares, scratch
space and optional VRAM. Coordinator offers still cannot require VRAM in this
protocol version. A local adaptive media encode may reserve a tested device
profile separately. The configured ceiling and live available resources both
apply. Live headroom subtracts all outstanding reservations conservatively, so
it can temporarily underutilize a host rather than oversubscribe it. Local
maximum job counts remain an independent safety ceiling.

Conversion attempts each have an isolated workspace, contained native process
tree, timeout, cancellation flag and result. One bounded orchestration thread
per admitted attempt is acceptable for this small-PC version (max 64 configured,
default 4); heavy conversion remains in FFmpeg. It is not a thread-per-network-
connection server. Task threads spend their transfer/child waits blocked rather
than spinning. The gateway uses async WinHTTP receive while sending heartbeats.
Model-serving adapters can later share a resident model process without merging
the identities or leases of their independent inference jobs.

A cancelled/expired running job retains its resource reservation until the
entire process tree terminates. Windows Job Objects are process/resource
containment, not a security boundary equivalent to a sandbox or VM.

## Priority and preference are separate

Local task modes are preferred / allowed / disabled; disabled is a hard veto.
The first tray exposes only the implemented video and image adapters. Central
routing combines authorized capabilities, its machine policy, the advertised
local veto/preferences, online state, and actual capacity. A preferred label
never overrides a disabled task or an exhausted resource budget.

Machine modes are maximum / normal / low / backup. Low and backup reduce local
process scheduling priority and CPU share. Maximum does not use Windows High
or Realtime priority and never bypasses configured resource ceilings. Backup
means lower CPU usage locally and a routing hint to Lightsail; only the central
scheduler can guarantee it is offered work solely when other hosts are unsuitable.
Pause prevents both new offers and starts of previously reserved offers.

## Reliability and limits

Results and outbox rows commit together. The same attempt/result can be submitted
again; a contradictory result cannot replace it. Matching result acknowledgments
remove the outbox row. At 512 unacknowledged results, admission stops until the
backlog drains. Reads/transmissions are batched in groups of up to 128.
Restart records interrupted attempts as abandoned, not resumable work, and sends
that status for coordinator reconciliation. Existing jobs survive a transient
socket disconnection only within their lease; no contact means no indefinite
execution. Crash retry is at-least-once execution with fenced publication, not
an assertion that computation happens exactly once.

Worker logs rotate at approximately 5 MiB; up to 20 failed child logs are retained.
Media is removed after attempts. Signed URLs and lease tokens are not logged,
but the protected per-user journal contains job manifests and result tokens.
Attempt tombstones currently remain in SQLite for idempotency. Long-term bounded
retention, revocation/credential rotation tooling, upload resumability and detailed
progress streaming are follow-on work. Disk size monitoring is a watchdog, not
a hard filesystem quota; keep safety headroom.

## Endpoint/pairing module (0.1.1)

`ow/pairing.hpp` exposes coordinator identity validation, a durable pairing journey
and journal-origin binding. It depends on purpose-specific secret-store, signing,
HTTP and clock/cancellation boundaries. Windows adapters supply existing DPAPI,
BCrypt and WinHTTP; the tray/worker is an execution entry point, not a second
pairing policy owner. Setup keeps decision/persistence functions separate from
OS installation and startup effects. Tests assert outcomes through those ports
and durable reopen; implementation helper layout is not the acceptance criterion.
This applies the agreed selective ports-and-adapters structure without moving
unrelated scheduling/media code or changing its behavior.

## Adaptive geometry evidence

Encoder admission ceilings and measurement resolution are separate. The current
approved FFmpeg command boundary has no uniform per-adapter maximum-size query;
reviewed conservative backend ceilings bound admission (NVENC/QSV 4096x4096,
AMF 3840x2160). These are policy ceilings, not a claim that every driver/codec
supports every size inside them. Fixed yuv420p GPU commands require even axes.
CPU profiles carry no GPU geometry ceiling. No sample dimension sets a ceiling.

Startup compares repeated CPU/hardware encode-plus-decode throughput at
1920x1080. Only matching measurements at least that large can reject a hardware
profile for the 5 percent gain floor. Saved profiles and tuning measurements name
their resolution; throughput is synthetic evidence, not a quality or 4K speed
claim. The two-minute per-encode tuning deadline accommodates the larger sample;
all configured resource caps and cancellation checks remain in force.

A larger input within the backend ceiling gets the real full-size hardware encode
with a guarded CPU retry. Every hardware result must decode successfully and
retain the expected codec and input dimensions before upload. Failure discards
partial output, drains the contained child tree, demotes only that device/codec
once for the current probe epoch, and acquires the reserved CPU encoder slot.
Demotion blocks later selection and is checked before reserved work launches;
already running work retains its reservation until drain. Diagnostic profile
state records demotions; restart runs a fresh probe instead of trusting that file.
Unknown dimensions and unsupported geometry take CPU, including fallback slot
acquisition. Logs distinguish geometry rejection, unproven size, measured slower
throughput, insufficient measured gain, and actual runtime failure/demotion.

## Physical encoder placement candidate

The Windows Display setup-class inventory is the physical source of truth.
`DisplayConfigGetDeviceInfo` resolves each DXGI LUID to its adapter interface;
SetupAPI resolves that interface to a present Display device instance. Only a
complete `PCI\...\location` instance can establish a physical card. `ROOT\...`
instances are software adapters, retained as explicit exclusions, with no profile,
VRAM capacity or placement. Unmappable adapters remain withheld. PnP-only devices
remain diagnostic entries, including software displays absent from DXGI.

The case-normalized PCI instance is the physical identity used for card counts,
VRAM and session reservations. LUIDs still select child-scoped telemetry and
CUDA ordinal mapping; they cannot create another card. NVIDIA also requires a
nonzero CUDA UUID as encoder mapping evidence, not as physical identity. Two
verified aliases of one instance are ambiguous and both withheld; an unproven
or software alias cannot poison a separately proven real card. Equal product
IDs on different PCI instances do not collapse two real cards. Every backend
still needs successful encode/decode and bounded tuning before admission.

A hardware reservation occupies one encode session per physical card across
its sequential H.264/HEVC renditions, and at most two NVENC sessions across this
worker. One codec cannot evade the other codec's session reservation. Selection
prefers the card already chosen for the job when that codec and every resource
budget fit; different jobs can reserve different cards independently. Per-card
VRAM is the maximum of sequential codec footprints, with a separate aggregate
machine ceiling. CPU/RAM/scratch, job ceilings and live headroom remain binding.
A changed device removes only its stale profiles; complete inventory failure
still invalidates hardware globally. All active resources remain owned until
the contained process tree drains.

CPU fallback budgets do not occupy CPU encoder slots until acquired. The acquired
slot follows reservation moves; unrelated release cannot clear it, and a new
CPU primary cannot take an occupied fallback slot. Existing encode/decode/output
validation and bounded cancellation-aware retry remain mandatory.

The plain log names backend/codec, encoder ordinal, full device identity,
physical PCI instance and child PID at actual child launch; completion is logged after
process-tree drain. The short viewer shows the job suffix and GPU ordinal in
blue, plus a white startup card table and schedulable-card count. The raw display inventory count
remains separately labelled diagnostic evidence. Exclusion records distinguish
software/unmappable adapters, session exhaustion and insufficient VRAM. These are source policies with
portable contracts, not measured two-card Windows execution. Driver/session use
by other programs and GPU allocation remain external constraints; CPU retry is
required when opening a hardware session fails. See
[the multi-GPU gate ledger](../verification/multigpu/REPORT.md).

## Recovery after hardware probe failures

A failed startup sample or runtime codec demotion withholds that profile until
another successful, bounded encode/decode/tuning epoch. Immediate hardware probes
have three attempts with 1s/2s backoff; later retries back off from 30 seconds to
five minutes without a permanent verdict. Allocation failures cannot by themselves
distinguish session pressure from driver or buffer-allocation problems. The status
table names the card/codec, classification and retry/idle deferral; full evidence
stays in diagnostic logs.

Recovery is an independent thread. An atomic admission gate requires all reserved,
running and cancelling work to drain and defers new offers while samples run.
It never clears operator pause. Current successful in-process profiles can retain
their sample evidence; failed profiles require new proof. Driver, mapping, physical
identity and approved converter hash are checked again before adoption. Diagnostics
are staged separately and the scheduler is replaced only with no active adaptive
reservations. The network, watchdog, leases and process-tree containment remain
independent. See `verification/second-card/REPORT.md` for gates and platform limits.

## GPU child commit budgets

GPU startup and runtime child RAM budgets are workload specific. The policy,
local RAM top-up accounting, allocation-failure classification and evidence
limits are specified in [GPU memory budgets](gpu-memory-budget.md). This commit
budget is separate from VRAM. Runtime allocation failures under it retry CPU
without demoting the proven card; other hardware validation failures retain the
existing demotion rule.
