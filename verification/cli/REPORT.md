# CLI candidate gate ledger

Base origin/main: e4366c326c124acc46f63bd7251bd0cdf24f4331.
Branch: feat/full-cli. Isolated worktree: worker-client-cli.

Initial regression compile: `01-red.log` records missing control API. First implemented
portable behavior run (`02-portable.log`) failed six new contracts because empty
unconfigured endpoints were rejected; corrected at configuration validation, not skipped.
`03-green-build.log` / `04-green-tests.log`: all six portable suites passed.

Windows compilation/native IPC/CLI integration and complete sanitizers pending.
The manual workflow is the only intended Windows runner; no PC installation,
worker restart, live resume/pause, pairing or deployment is authorized/performed.
The native fixture has a real controller and pipe but no coordinator/jobs.
Cross-session SSH and a different real account remain separate acceptance gates.
Do not call this complete or release-ready until all required gates are established.
