# Installer manifest swap — October 18, 2026

## Diagnosis and recovered correction

Started from main `3576e62873c0dc62f84f242015abb3a21ddadf16` on
`fix/installer-swap`. Fetched origin/main equals that revision. Inspected all
ten local worktrees and recent local commits. The prepared correction is in
`c925ea4`, on `perf/measured-device-decode`, in the finish-perf install helpers:
explicit `current.perf-backup.json` plus acquiring the process handle before
waiting. That tree is clean; an unrelated VRAM worktree is dirty and untouched.

The exact failed call was:

```powershell
[IO.File]::Replace($temp,(Join-Path $r 'current.json'),$null)
```

Target: `C:\Users\Danny\AppData\Local\OrganizerWorker\current.json`.
Source: same directory, `current.perf-publish.json`. The log records
`OLD_WORKER_STOPPED_IDLE=True`, then `ArgumentException: The path is not of a
legal form.` PowerShell 5.1 converts the null string backup argument to an empty
path. The new native regression reproduces that exact error before doing a
successful explicit-backup swap. This was argument validation, not a sharing
violation, missing backup, cross-volume error or permissions failure. There is
no evidence of an antivirus/indexer holder. A backup destination need not exist.

The recovered helper is retained as `prior-install.ps1` for comparison, with
the original failure log. The canonical implementation is now
`scripts/update-worker.ps1` and `scripts/worker-swap.ps1`; the historical helpers
are not used for the rollout.

## Behavior and focused Windows proof

Complete version directories are hash-checked before publication. The old
manifest remains immutable on disk until connected/accepting verification.
All manifest replacements use real same-directory backup paths. Only Windows
sharing/lock violations retry, bounded to ten seconds. Failure restores the old
manifest and restarts/verifies the old binary. Interrupted updates retain a
recovery manifest and require explicit recovery before another install. No
remove-live-file fallback exists. Persistent filesystem denial can prevent
recovery; in that case the backup survives and exact failure paths are reported.

`01-windows-red.log`: expected pre-implementation missing swap module, exit 1.
`02-windows-focused.log` and `03-windows-focused-final.log`: six Windows behavior
contracts, exit 0. These reproduce the actual null-backup error, stop a real
manifest-lock holder before retry, exhaust a persistent lock within its bound,
restore after failed launch, kill an installer after publication and explicitly
recover on reopen. The restored fixture executable is actually run, exit 0.
The tests are registered only under WIN32 in CTest and use the one authorized
temporary staging root, with no production account or job contact.

## Full gates and rollout

Source commits: `549a580` and `e4a892d`. Final recovery-marker deletion
regression: `08-recovery-commit-red.log` fails with the candidate still selected
(exit 1); `09-recovery-commit-green.log` passes seven cases (exit 0) after moving
that commit operation inside the rollback boundary. No behavioral test was
weakened. Inactive temporary-file cleanup cannot hide the primary error or turn
a committed update into a reported installation failure.

Linux Release and ASan/UBSan: six suites, 263 cases each; configure, build,
discovery and every suite exit 0 (`linux-results.json`). ASan/UBSan applies global
compile/link instrumentation with leak detection and halt-on-error. The final
additional code/test changes are Windows-only PowerShell, outside the portable
build; no portable code or contract changed after that run.

MSVC 19.44 Release: configure 0, build 0, discovery 0, all ten discovered native
suites 0, SSH runner 0 (`10-windows-final.log`, `windows-results.json`). 318 cases:
79 core, 9 pipeline, 10 rendition, 78 adaptive, 64 progress, 14 Windows,
25 setup, 15 setup-progress, 17 converter, 7 swap. The initial complete native
run also passed; final rerun includes the added recovery-marker test. Existing
compiler shadow warnings remain; no test was skipped or weakened.

`11-final-source-proof.log` reads all 90 application/test/script/lock paths back
from Windows, with zero hash mismatches against the final source (only declared
PowerShell/cmd line endings normalized). The archive and small final update
hashes are recorded in the native logs. Reviewed installed Boost 1.89.0 and the
existing MSVC toolchain were reused; no dependency/host software was changed.

Merge/push and per-machine installation readbacks are recorded below.
Desktop console-buffer access through SSH was tried read-only and denied by
Windows (`AttachConsole error 5`). Fresh rendered status-sidecar checks remain
available; desktop visual appearance must not be inferred from them.


## Landed and installed, Danny then Rick

Fast-forward merge exit **0**, push exit **0**, remote readback exit **0**:
`main = origin/main = 369eb1f65d3ef019c7ac5698e718a31640c81014`
(`landing-results.json`). That is the package source-revision marker; all 90
application/test/script/lock paths match the tested Windows tree. Subsequent
rollout-ledger documentation changes do not change that application source.

The package was generated from the Windows-gated binaries. A fresh local download
independently verified the archive hash, package manifest hash, all three binary
hashes and all four updater-script bytes (declared CRLF/LF normalization only).
See `12-package.log` and `package-proof.json`.

Both machines now select and run **0.1.2-db2bc15dc83612bb**. Full worker SHA256:
`db2bc15dc83612bb3eb833a4a1b1b21b782a60f1481fdc4b360022eba76fb278`.
Both read back installed revision `369eb1f65d3ef019c7ac5698e718a31640c81014`.

| Machine | Install exit | Running PID / session | Current-epoch connection | Encoders | Fresh rendered idle table |
|---|---:|---|---|---|---|
| Danny | 0 | 20980 / 1 | STATE CONNECTED; accepting | cpu,nvenc | RTX 3070 idle, RTX 3050 idle, cpu idle; running 0 |
| Rick | 0 | 24676 / 1 | STATE CONNECTED; accepting | cpu,qsv,amf | Radeon RX Vega M GL idle, HD Graphics 630 idle, cpu idle; running 0 |

Danny reports **schedulable GPUs=2**, excluding the software display alias.
Danny connected at 05:53:12Z; Rick connected at 05:57:22Z. Both startup task exits,
installer exits and independent SSH verification exits are **0**. Installed and
running image, manifest hash, source marker, PID/creation-time runtime identity,
desktop session and fresh table timestamps were all read back. Both installers
verified unchanged credential, pairing-key and worker-configuration bytes.
Neither worker was re-paired. Rollback markers are absent only after successful
verification; the old version directory and unchanged previous worker SHA are
retained on both machines.

Primary evidence: `14-danny-install.log`, `19-rick-install.log`,
`26-danny-final.log`, `27-rick-final.log`, `rollout-results.json`.
No real coordinator job was submitted: the available operator CLI requires an
existing approved asset, and no approved synthetic asset was identified for this
run. No new real-job card/start/done claim is made. The current source retains the
passing card-label behavior contracts from main; a real accepted lease/BEGIN job
remains needed for that separate end-to-end observation.

The idle tables above were read from the fresh native-rendered status sidecars.
Actual desktop screen-buffer capture was denied across SSH sessions
(`AttachConsole error 5`, `05-capture-readback.log`); visual desktop rendering is
not claimed. No temporary scheduled capture task was created or startup changed.

The one staging directory per machine was
`%LOCALAPPDATA%\Temp\ow-installer-swap-20261018`. Deletion exits **0** on both,
then fresh independent connections report **STAGING_EXISTS=False**. See
`20-danny-cleanup.log`, `25-rick-cleanup.log` and both final readbacks. No other PC
scratch directory, tool installation, driver/device setting, firewall/SSH change,
PATH/policy change, production restart, migration, environment or Cloudflare
change was performed. PC writes were the authorized installs, normal installed
worker activity and those staging directories. Old installed data was retained.
