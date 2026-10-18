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

Merge/push and per-machine installation readbacks remain pending below.
Desktop console-buffer access through SSH was tried read-only and denied by
Windows (`AttachConsole error 5`). Fresh rendered status-sidecar checks remain
available; desktop visual appearance must not be inferred from them.
