# Local worker CLI (source candidate, not installed)

`OrganizerWorkerCtl.exe` is a console client, not another worker. Closing it
cannot terminate the controller or jobs. It requires the **updated controller**;
older installations have no control pipe. Installing this candidate requires
separate operator permission. It cannot resume Danny's currently installed old
worker without a separately approved upgrade. No live worker action accompanies
this source change.

## SSH / PowerShell

Run as the same Windows account that owns the signed-in worker. SSH session 0
can contact that account's controller in interactive session 1: the named pipe
is owner-scoped, not window/session-scoped. A different account, including a
separate administrator account, is deliberately not an implicit delegate.
Elevation is unnecessary. No password, token or signed URL belongs in arguments.

```powershell
$root = Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$current = Get-Content -LiteralPath (Join-Path $root 'current.json') -Raw | ConvertFrom-Json
$ctl = Join-Path $current.directory 'OrganizerWorkerCtl.exe'
& $ctl --help
& $ctl --json status
& $ctl pause
& $ctl drain
& $ctl resume
& $ctl config list
& $ctl --json config get limits.cpu_threads
'{"operator.priority":"low","operator.video_preference":"allowed"}' | & $ctl --json config set --stdin
& $ctl config reset operator.priority
# Cold changes persist, but do NOT restart or change existing job limits:
'{"limits.cpu_threads":2,"limits.max_jobs":4}' | & $ctl --json config set --stdin
& $ctl --json status
```

From Linux, use an existing authorized SSH alias and PowerShell on the remote
host; send the JSON through stdin rather than interpolating it into a remote
command. For example, after assigning `$ctl` as above inside the remote session,
`'{"operator.priority":"low"}' | & $ctl config set --stdin` needs no GUI.
No SSH configuration, firewall, account or credential changes are required.

`--json` emits one JSON object with local-control `schema:1`, `ok`, `exit_code`
and, for successful live operations, a fresh `observed_at_utc_ms` observation.
Status includes PID, owner SID, agent version, build revision, uptime,
connected/paused/accepting/draining/exiting, admission reason, heartbeat active
attempt IDs/states, resource reservations and pending result count. No bearer,
lease token, enrollment secret, signed media URL or free-form server error is
returned. `accepting` is the current local gate, not a promise to accept every
resource/capability-specific offer. Offline status files are never control inputs.
A versioned source archive without Git metadata reports `source-archive`.

## Commands and exit codes

| Command | Observable effect |
|---|---|
| `status` | Fresh live structured state, never a cached status-file claim |
| `pause` | Persist pause, reject offers and reserved BEGIN; preserve running jobs |
| `drain` | Same non-exiting pause; running jobs finish normally |
| `resume` | Explicitly clear operator pause; repeated calls never toggle |
| `config list` | All setting types/defaults/current/effective/live/restart flags |
| `config get NAME` | One row in the same schema |
| `config set --stdin` | Atomic flat-key JSON patch; all fields validated before persistence |
| `config reset NAME...` | Explicit named defaults; no destructive reset-all |
| `exit drain` | Pause and finish existing jobs, then exit |
| `exit stop` | Explicit cancellation; capacity stays held until process-tree termination |
| `auth` | Credential-file presence plus the live public, expiring approval label when pending; never private enrollment material/token retrieval |
| `console` | Ask the interactive controller to open its independent viewer |
| `logs` | Return the existing log/data path, without shell execution |
| `history` | Read existing last-outcome report, separate from live jobs |

Exit codes: 0 success; 2 rejected arguments/schema/configuration; 3 unavailable,
busy or timeout; 4 authenticated control denial when returned by the server;
5 bounded response/I/O failure when returned by the server. Pre-connection
Windows ACL denial is intentionally indistinguishable from unavailable (3).
Transport requests/replies are bounded to <64 KiB, depth 32, with two-second
connect/read/write waits. A timeout after sending a mutation is an **unknown
outcome**: reconnect and read back; repeating pause/resume/config assignment is
safe. Requests are serialized; clients never perform blind toggles. General
parse/validation errors do not echo input (which might have contained secrets).

`--offline` explicitly selects stopped-worker configuration, auth-presence or
log-path access. It takes the same exclusive `instance.lock` as worker/setup;
a live/old controller or setup blocks offline mutations. It does not spawn a
worker, send runtime controls, or infer live state. Effective values are null.
There is no automatic fallback from live failure to disk edits. For disposable
live test workers, `--data-root C:\absolute\local\root` selects only the pipe
namespace: no path is transmitted to the server. Custom roots are prohibited
with `--offline`; normal runtime always targets the installed owner root.

## Complete settings / tray / setup parity inventory

Names come from the existing config loader, tray and setup, not speculative
features. All file settings remain schema 1. `operator` is the new atomic home
of existing hot preferences; missing `operator` migrates the legacy SQLite
priority/video_preference/image_preference/paused values once. Defaults below
are template defaults; setup's existing hardware-derived initial limits remain.

| Flat name / existing surface | Default / valid values | Effective semantics |
|---|---|---|
| `operator.paused` / tray pause-resume | false; boolean | Live; running jobs preserved |
| `operator.priority` / tray priority | normal; maximum/normal/low/backup | Live, existing child priority sampling; never High/Realtime |
| `operator.video_preference` / tray video | allowed; preferred/allowed/disabled | Live; affects diagnostic and app video; disabled hard veto |
| `operator.image_preference` / tray image | allowed; preferred/allowed/disabled | Live; diagnostic and app image |
| `limits.ram_mb` / file/setup | 2048; integer 256..1048576 | Restart required |
| `limits.reserve_ram_mb` / file/setup | 0; integer 0..1048576 | Restart required |
| `limits.scratch_mb` / file/setup | 8192; integer 1..1048576 | Restart required |
| `limits.cpu_threads` / file/setup | 2; integer 1..64 | Restart required |
| `limits.max_jobs` / file/setup | 4; integer 1..64 | Restart required |
| `limits.vram_mb` / file | absent/auto; uint64, null resets auto, 0 disables GPU | Restart/probe required; no unprobed GPU activation |
| `storage_hosts` / setup host consent | []; at most 128 exact HTTPS hosts | Restart required; explicit local owner approval via stdin |
| `ffmpeg_path` + `ffmpeg_sha256` / setup converter | empty pair, or local absolute path + 64 lowercase hex | Atomic pair recommended; restart, hash and actual probe still mandatory |
| `coordinator_url` / setup endpoint | empty or credential-free HTTPS origin:443 | Readable in config; changes/reset intentionally require existing `setup.cmd -RePair` scope archival, not raw edits |

There is no user-facing per-device selector, device enable switch, log level,
log rotation limit, arbitrary encoder argument, task shell command, API token
setter, disconnect/reconnect toggle or inference knob in the current source.
Device profiles are **observed probe results**, never configurable proof of
capability. Recovery retries and network reconnect remain automatic bounded
controller policy. `resume` does not imply a connection or clear probe/capacity
vetoes. `status` tells the difference.

Already-scriptable setup/Doctor controls remain authoritative rather than being
reimplemented in IPC: `setup.cmd -CheckOnly`, `-RePair`, `-NoConversion`,
`-NoStartup`, `-NoLaunch`, endpoint, host approval and license consent options;
see [setup](setup.md) for their exact switches and noninteractive use.
`OrganizerWorkerDoctor.exe --json --check-coordinator` and
`--json --probe-ffmpeg --data-root PATH` retain existing diagnostics; do not run
expensive Doctor probes alongside active jobs without separate approval.
`auth` can show the existing live `pairing_user_code` and its expiry for approval
on the coordinator; it never decrypts/exposes a device code, challenge, signature,
private key or bearer. Offline auth reports presence only. Pairing approval remains
on the coordinator, not a local privilege escalation.
No token input/output command is added; DPAPI stores and journal scope fencing
are unchanged. Endpoint changes must drain/stop, then use the existing explicit
archival/re-pair journey. These maintenance operations do not become hot settings.

## Persistence, compatibility and security

Tray and CLI share `Worker::update_settings`, `ControlSettings` validation and
one flushed atomic `worker.json` replacement. Multiple-field patches cannot
partially validate/write. The controller serializes tray/CLI writes and applies
hot settings only after persistence succeeds. Cold effective values stay at the
running process's startup snapshot; no job or process is silently restarted.
Resource/hardware admission, leases and cancel-to-drain accounting are unchanged.
External file edits while running are not supported (setup already requires the
instance lock). Existing installer schema/current.json and per-user startup are
unchanged; CMake installs the companion and reference with the other executables.
Setup preserves added `operator` fields on upgrades. Old binaries predate the
migration and do not read `operator`; downgrades require an explicit settings
migration, not a claimed transparent hot-preference rollback.

The pipe DACL grants only the exact worker owner SID, rejects remote SMB clients,
and uses first-instance protection. Both peers verify owner identity: the client
queries the server process token; the server impersonates only long enough to
read the client SID, then reverts **before** handlers/file access. No wildcard,
Everyone, Authenticated Users, admin-group or machine-wide control grant. Same
owner across SSH/desktop logon sessions is supported by design; an alternate
account is not. No public HTTP listener, admin runtime, delegated file path,
arbitrary command or secret-retrieval operation is provided. ACL rejection and
same-owner cross-session use still require actual Windows acceptance evidence;
Linux tests cannot establish them.

## Verification boundary

See `verification/cli/REPORT.md` for exact executed checks. The native CLI
integration fixture uses a real headless controller, real owner pipe and real
persistence, but **no coordinator or jobs**. It is not proof of Danny's worker
upgrade, production connection, remote job execution or cross-session SSH.
Never install/roll out based solely on this document or portable tests.
