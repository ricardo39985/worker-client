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

## Independent concurrent jobs

One logical task is one job; a retry is a different attempt. An offer reserves
capacity atomically before readiness is acknowledged. It cannot start until
BEGIN supplies a valid lease. Reservations account for RAM, CPU shares, scratch
space and optional VRAM; this build refuses GPU requirements altogether.
The configured ceiling and live available resources both apply. Live headroom
subtracts all outstanding reservations conservatively, so it can temporarily
underutilize a host rather than oversubscribe it. Local maximum job counts remain
an independent safety ceiling.

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
