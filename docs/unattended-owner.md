# Unattended existing-owner worker (implementation candidate)

**Not yet a verified unattended installation.** Previous product529/CLI deployment
is separate: see [release receipts](../verification/cli/RELEASE.md). Neither Rick
nor Danny is enrolled by this source change. No restart/logoff/lock/boot test is
implied. The owner is away: preserve both current desktops and working SSH.

## Architecture and guarantees

- `OrganizerWorker.exe --headless` is a real no-UI bootstrap, not the old tray
  guard with an exception. A normal, non-elevated existing owner is required;
  SYSTEM/LocalService/NetworkService and elevated engines are refused.
- Require the existing owner configuration, user-DPAPI credential/key and journal.
  No account/identity creation, silent re-pair, machine-DPAPI downgrade or S4U.
  Reject missing/corrupt state, reparse paths, unsafe ownership/ACLs, escaped
  `current.json` version paths, a changed executable hash or wrong actual image.
  Use the SAME exclusive `instance.lock`, resource accounting and owner-only IPC.
- A Windows-managed **Password/Limited** boot task owns one fixed native
  PowerShell launcher, waits for the engine's lifetime, starts30s after boot,
  permits batteries/offline startup, has no job-duration cutoff, uses IgnoreNew
  and bounded failure retries. It cannot execute arbitrary submitted commands.
  Task DACL grants the exact owner and SYSTEM; the SYSTEM ACE is task management,
  **not** a SYSTEM runtime. Inspect principal/action/trigger/power/retry/ACL before
  activation. No public listener, new SSH privileges, autologon or stored password
  in the application/configuration/artifacts.
- `OrganizerWorkerTray.exe` is only an owner-authenticated client. Optional manual
  attachment after sign-in; closing/logging off it does not send a worker stop.
  Viewer/CLI exit never owns the headless engine or its children. Explicit
  `exit stop` remains a separate, destructive operator choice.
- Existing interactive task/shortcut and wrapper are retained, not broadly
  deleted/replaced. Once enrolled, default interactive engine invocation routes
  to the enrolled engine and optional attached tray, never a second legacy engine.
  An already-running headless engine does not require an automatic tray.

## Local one-time secure enrollment — ONLY after verified install

This is an owner action, **not a request for a password in chat**. It is not
currently executable on the old529 deployment. First finish the green source,
review, exact-main artifact and approved safe installation gates. Do not stop a
live worker to manufacture before-login results.

On each PC, its EXISTING owner (Rick=`DESKTOP-3FR54CB\\reald`, Danny=
`DANNYS-OLD-BUT-\\danny`) opens **local Windows PowerShell as Administrator using
that same owner's UAC elevation**, not a different admin account/SSH service.
Run just this public-data command:

```powershell
$root = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$current = Get-Content -LiteralPath (Join-Path $root 'current.json') -Raw | ConvertFrom-Json
& (Join-Path $current.directory 'OrganizerWorkerCtl.exe') enroll
```

The native client verifies the selected installed directory/image and then opens
Windows' secure credential desktop. Choose the SAME owner's **Windows account
password provider** if Windows Hello/PIN is selected. Enter it ONLY into that
Windows dialog. A PIN, empty password, foreign SID, unsupported provider or denied
batch-logon policy is not silently replaced with S4U/autologon/admin. Do not change
password/MFA/logon policy merely to pass. Cancel safely if unsure and report only
the non-secret failure. Credential buffers are zeroed; only Windows Task Scheduler
keeps its protected task credential. Task XML snapshots contain public definition,
not a recoverable password.

Enrollment records `pending` before task creation and `enrolled` only after
Windows accepts registration. Retrying a pending transaction accepts only the
same owner, restrictive ACL and matching enrollment-id task description. Unknown
preexisting tasks/launchers are retained and refused. Cancellation does NOT mean
an enrolled task or working profile was proved. Never delete somebody else's task
as a repair. Password rotation requires this same local secure step again.

For reference, required existing owner SIDs are:

- Rick: `S-1-5-21-2018316448-104147440-857418166-1001`
- Danny: `S-1-5-21-3947802441-3242602464-3093035390-1001`

## Normal owner CLI / readiness / restart

After enrollment, existing pinned same-owner SSH can run these without desktop
login or elevation of the engine. Do not run lifecycle commands on the current
production installation under the owner-away constraint.

```powershell
$ctl = Join-Path $current.directory 'OrganizerWorkerCtl.exe'
& $ctl --json enrollment
& $ctl --json start --wait-seconds 600
& $ctl --json status
& $ctl --json restart --wait-seconds 1800
```

`start` is idempotent for the actual healthy headless controller, not a legacy
interactive controller. `restart` temporarily fences admission WITHOUT persisting
operator pause. It waits admitted reservations, real process trees, journaled
in-flight expiry/completion and durable results until matching ACK. Reserved BEGIN
is vetoed; already-running jobs and lease renewals continue. No forced kill, claim
loss, silent rerun or old-attempt output overwrite. The replacement reloads the
operator's desired pause and cold settings. An unavailable network/ACK can keep
drain pending; timeout is not permission to cancel work.

Readiness is actual matching owner/image/hash + non-elevated headless identity,
connected transport and **processing_ready=true**, not field existence, Task.Run
success, PID presence or cached status. Paused can be ready but not accepting.
Missing/disabled/failed capabilities remain off. `processing_ready=false` never
passes. `enrollment` validates public registration policy, not a fresh password
logon/GPU/DPAPI proof. Read status after an unknown-outcome IPC timeout. CLI exit
codes remain0/2/3/4/5 as in [CLI reference](cli.md).

Use `OrganizerWorkerTray.exe` manually from the selected directory for optional
UI after login. `console` requests are handled through the attached tray when one
exists; no UI is created on a headless station.

## Installer/updater and recovery

Schema2 package inventory contains five executables (worker, CLI, Doctor, console,
attached tray), `cli.md` and `start-headless.ps1`, each pinned by package metadata.
Old schema1 packages are recognizable for historical migration, not accepted as
an unattended downgrade. Publish immutable versions through the shared swap and
verified manifest, retain previous versions/configuration/preferences/identity,
and use safe drain/ACK checks before any approved replacement. No automatic
credential enrollment, desktop login or startup-policy fallback may be inferred.
A failed or unavailable credential/profile/task is actionable failure, not a
reason to create a SYSTEM service or reinstall/re-pair the owner.

Enrolled full setup applies the same protected code-approval rule as the version
updater: it hashes all seven staged files as data before executing the candidate,
archiving pairing state, or publishing configuration/current version. A rebuild
whose exact worker and companion hashes are not already in the external protected
approval is refused with the current engine retained. The operator must first
independently review the code-only artifact and manifest, then use the fixed
protected launcher in an explicitly elevated SAME-owner local session with
`-Mode Approve -Package <directory> -PackageManifestSha256 <reviewed hash>`.
That operation only adds reviewed metadata to the admin-owned anchor; it is not a
password task enrollment and never executes the package. Bootstrap launcher changes
are not self-approved and require secure local migration review.

If a lifecycle command times out: inspect `status`, journal/outbox counts and
`enrollment`; do NOT kill encoders, delete locks, change coordinator, re-pair or
blindly run another engine. Preserve pending-swap/task snapshots and old immutable
versions. Secure owner credential recovery and production maintenance require an
available owner and the separately authorized safe-drain path.

## Acceptance boundaries — keep separate per PC

1. Portable/Linux/sanitizer contracts and native MSVC regression/security matrix.
2. Disposable native limited-owner noninteractive tests: actual bootstrap/no-UI,
   current-profile DPAPI, approved typed converter children, restart fencing,
   leases, output hash/decode, durable retries/wrong/matchingACK. Synthetic
   transport is NOT TLS interoperability or real coordinator work.
3. Actual local owner enrollment; fixed task/principal/ACL/action and actual normal
   token/profile/DPAPI/backend/coordinator readback, with SSH recovery preserved.
4. **Separate explicit owner-ready boot approval**. Then actual new boot, zero owner
   interactive sessions, real user-profile DPAPI/TLS and QSV/AMF/NVENC/multi-GPU
   probes in that exact context. Existing logged-in/cached-DPAPI success and merely
   changing session numbers do NOT prove this. No lock/logoff simulation now.
5. Authorized private, non-posting coordinator-accepted job per PC, no surprise
   charges. Local fixtures/connected/accepting do not establish production jobs.

SSH is separate: both PCs' existing sshd services were Automatic/Running and
accepted the pinned key; no SSH change was needed. Historical Danny SSH while no
console user was reported does not prove worker prelogin behavior or immediate
reconnect after every reboot. Danny CBS/WSL pending reboot remains the environment
lane's real gate; no platform/account/distro/toolchain or reboot change is made
by worker implementation. No promise that maintenance will never need a login.
