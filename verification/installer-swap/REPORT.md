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

Pending. No new installation, merge or push is established by this document yet.
