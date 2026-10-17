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

## Console lifetime

OrganizerWorker.exe is a GUI-subsystem tray/controller executable. It starts
OrganizerWorkerConsole.exe with its own console. The console only reads the
log and watches the controller process handle. Closing the console does not
send a stop signal to the controller. The controller doesn't share that console.
The tray can reopen a viewer, pause new starts, change preference/priority, open
logs, and finish-or-stop on exit. The controller must run as the signed-in,
non-elevated Windows user. SSH can build or update it but cannot create a visible
tray in its service session. Optional startup is an interactive logon task,
not a boot service; user sign-in is required.

The controller's durable log is the only input to the console viewer. It contains
plain timestamped event and progress records, never terminal escape sequences.
Durable lines keep a UTC millisecond timestamp and the complete attempt id, so
they remain unambiguous evidence and a displayed suffix can be grepped back to
the real attempt. The viewer owns the short local presentation, color and
in-place dashboard redraw, so closing or crashing it cannot signal the controller
or its contained process trees.

Console lines are capped at 80 columns and target roughly 60. The viewer renders
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
output and `NO_COLOR` remain plain append-only event text with no escape codes;
redirected output also suppresses dashboard-only progress rows. Legacy consoles
use Win32 cursor/color operations and ASCII bars instead of assuming
virtual-terminal support.

Conversion progress is an execution measurement, not a file-size estimate.
Typed FFmpeg invocations enable its machine-readable progress stream; the worker
tails the contained child log, parses source duration and monotonic `out_time_us`,
and floors the ratio at 100 percent. Transfer and verification continue to use
exact byte totals. Stage generations make a retry reset explicit, and stages
without a defensible total emit one written explanation rather than a percentage.

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
