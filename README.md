# Organizer Windows worker — 0.1.2 source candidate

A native Windows tray worker with typed conversion jobs, durable results and
operator-approved pairing. This candidate contains source, not a compiled or
signed Windows release. The updated Windows build and real job test are pending.

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
for about 24 hours from attempt creation. No GPU/inference adapter is included.

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
doctor. That evidence does not verify this 0.1.2 candidate. Its MSVC/native build,
23 PowerShell setup contracts, clean-machine/bootstrap/reboot paths and actual
Windows-to-Lightsail conversion remain required. See `docs/status.md`,
`docs/testing.md`, `docs/setup.md` and `contracts/worker-v1.md`.

## Terminal progress candidate

Setup now streams native output, counts download/checksum bytes and dependency
files, labels prompts, and reports elapsed time in quiet native stages. Boost
extraction moves the generated tree into staging instead of copying it again.
The worker console reports job stages, measured transfer/checksum bytes,
conversion output bytes and quiet time. It periodically reports active jobs and
durable results awaiting acknowledgment. Unknown totals have no percentage.

Eight isolated portable progress contracts passed on Linux. Fifteen new
PowerShell progress contracts and the updated native Windows integration are
authored but unrun here. This is a draft source change, not a verified release.
