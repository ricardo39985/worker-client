# Candidate status — 0.1.2

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
