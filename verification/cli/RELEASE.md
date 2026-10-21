# Authorized release continuation (PR8 only)

Rick: **When it is green, merge and deploy on both machines.** This supersedes
only the earlier build/test-only boundary for PR8. Historical REPORT.md and
acceptance.md remain evidence of what had not been verified then, not a stop
instruction for the current authorized lane. No OS/account/network/backend,
signing, reboot, force-kill or unrelated agent changes are authorized.

Preserved worktree/branch: worker-client-cli / feat/full-cli. Revalidated remote
main e4366c3, feature head82bb3cb before continuing. No relevant run was active.

## Current gate ledger (not promotion-ready yet)

| Gate | Evidence / current state |
|---|---|
| Real foreign owner kernel denial and forged server rejection | Source73a2437, manual Windows run39539368442 PASS: LocalService SID S-1-5-19 denied error5; correct owner works; different-owner forged server received zero request bytes. Ephemeral CI tasks deleted. No account created; no personal-PC service task. |
| Complete native MSVC/suites | Source73a2437, all12 suites PASS. Later read-only inspector/updater changes require new exact-source full gate. |
| Active admitted-job/coordinator/ACK | Test-only supported fixture now AUTHORED: same production message/outbox handlers via injected transport; actual approved FFmpeg probe/contained children, real leases, pause/previously reserved BEGIN veto/resume, durable retry/wrong ACK/matching ACK, output hash+decode. Not executed yet. No real TLS/coordinator is claimed by this fixture. |
| Same-owner session0->LIMITED interactive1 | Pending isolated owned staging/task; no production control/config changes before gates. |
| Legacy upgrade | Real old/new controller adapters and shared installer contracts authored; pending exact old-source build and LIMITED native execution. |
| Settings retention/installer | Removing unsafe Stop-Process path; old tray without authenticated CLI must ask for real owner Exit/finish. Package includes CLI/reference, 10-minute probe window, whitelisted read-only preference inspection, cold settings and protected identity retention. Pending native regression gates. |
| Linux + fully instrumented sanitizer suites | Earlier303-case tree green; current read-only-store regression compile-red captured, implemented and portable suite green. Full final combined gates pending. |

Read-only preflight: both pinned SSH identities verified. Both old workers in
session1, no FFmpeg/maintenance/compiler processes observed; Rick accepting,
Danny operator-paused, jobs0. These snapshots are not deployment readback and
will be refreshed before staging or installation. Earlier full preflight helpers
that depended on Console.In EOF timed out; bounded direct encoded queries worked.
No marker/pending OS issue was used as a blanket worker blocker.

Before every remote mutation, exact intended directory/task/action is recorded
in scratch/worker-full-cli-release-checkpoint.md. One heavy build per target,
only bounded LIMITED interactive owner tasks. No production install until all
declared gates are green. Old tray shutdown may require the owner UI; there is
no invented remote WM exit or raw status/database edit.
