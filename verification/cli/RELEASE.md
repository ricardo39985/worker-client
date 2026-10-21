# PR8 release — BOTH PCs upgraded; production jobs not tested

**Current 2026-10-21 11:48 UTC:** both PCs are independently verified on the SAME
source529ec104 artifact, correct non-elevated owner/session1, real probes passed,
coordinator connected, processing ready, unpaused and accepting. Actual SSH0 CLI
and config readback passed; all release tasks removed. Latest explicit idle-stop
authority superseded the earlier owner-tray-only blocker. Full actual stop/race,
artifact, per-PC verification and cleanup evidence: [IDLE-STOP-RELEASE.md](IDLE-STOP-RELEASE.md).
Real coordinator-accepted job per PC remains **NOT TESTED**; no public workflow
or charge was invented. The earlier blocker section below is retained historical
context, not the current outcome.

Rick authorized: **When it is green, merge and deploy on both machines.** This
supersedes the historical build/test-only boundary, not any safety gate. No
OS/account/network/backend/signing/reboot changes or force-kill were authorized.

## Exact source and promotion

- Final tested feature: `dd08912e41c3acc97560651cadcf440a1a25c8df`.
- Tested tree: `25be7d4ccdabf1f9e96a11b804d321befdde17dc`.
- PR8 merged normally: `529ec104931a17ad4997771bb8bfad2eca5bcd2d`.
- Freshly fetched main contained the tested head; merged tree is **identical**.
- Shared distributable built from the exact merged-main archive, with full
  compiled source revision (Git absent on the build PC; no software installed).
- Subsequent verification/documentation changes do not alter product source;
  artifact provenance remains explicitly the merged commit above.

## Gates actually executed

| Gate | Result and retained evidence |
|---|---|
| Clean MSVC and native contracts | PASS: exact feature Windows manual run39545204659, all13 suites plus owner security step (`71-final-feature-native-green.log`). Actual LIMITED session1 owner clean build/all13 also PASS (`72..73`). |
| Foreign real account / forged server | PASS: existing LocalService SID S-1-5-19 denied error5; forged different-owner server received zero request bytes. Correct owner succeeds. Disposable CI tasks deleted; no account created. |
| Real same-owner cross-session | PASS: actual SSH session0 client reached LIMITED interactive session1 owner controller; public context receipt `75-cross-session-settings.json`. Native13 suites also exercised normal non-elevated owner client subprocesses. |
| All settings | PASS: all14 typed list/get/set/reset/current/effective/restart cases, invalid multi-field atomic rejection, preserved process identity. `tests/job_control_client.ps1`, `75`, `85`. Bound coordinator changes/reset deliberately reject and retain the established RePair journey. |
| Active admitted jobs / lease / durable ACK | PASS: actual typed FFmpeg probe and contained children; running child survived pause; reserved BEGIN rejected until idempotent resume; two actual conversions completed naturally; lease renewals, output hash/decode, durable retries, wrong ACK retention and matching ACK removal, zero reservations/outbox, clean drain exit0/unavailable3. `74-active-job-result.json`, `84..85`. |
| Legacy installation / startup continuity | PASS: exact old main e4366c3 built against old libraries, actual old/new owner controllers and shared installer transaction; protected identity/config unchanged, legacy preferences migrated, startup unchanged, old binary retained (`76-legacy-upgrade-result.json`). |
| Linux and fully instrumented sanitizers | PASS: Release6/6 and ASan/UBSan6/6, logs68..70. Python dependency tests are not sanitizer-instrumented. |
| Exact merged-main build / shared package | PASS: actual LIMITED owner clean MSVC/all13 again (`77..78`), CMake installation and every transferred package file SHA checked (`79-merged-package.json`). |

The job fixture injects coordinator/storage/pressure ports into the actual
production message, admission and outbox handlers. It is **not** real
coordinator/TLS/account or installed-worker job evidence. The legacy adapter's
public old Exit action exists only in the disposable fixture; it is not a
remote exit API for somebody's installed old tray. No real jobs were killed.
All completed owned acceptance/build tasks were removed and absence verified.

## Shared immutable artifact

- Source archive SHA256:
  `6f1160c91d69b1f456b9b0e63ec446755c099717f97da649ef5ea7839fb2ecaa`.
- Package manifest SHA256:
  `665652f2687f755994e038978f92a2c7f8862bad53b74b9b4285dce4f2b96702`.
- Worker SHA256:
  `df8389fa247318e07754aaf271eb30965ea1b18c7a396f0c58a79e871e029aa1`.
- Four executables plus `cli.md`; companion hashes are in `79-merged-package.json`.

## Historical pre-upgrade blocker at 05:01 UTC (resolved at 11:48)

At 2026-10-21 05:01 UTC both PCs still selected and ran the old worker
`88d521957987929f3423814a41121f452f7cf58c6760ce9e68be69928a2f8de9`:
Rick PID16628/session1, Danny PID17192/session1. Recent read-only samples showed
jobs0, Rick accepting, Danny operator-paused. These are snapshots, not a claim
of current health or successful installation.

The same verified package and canonical installer source are staged in each
owner-only temporary `ow-cli-deploy-pr8-529ec104` directory (`82..83`). No installed
pointer, startup, configuration, preferences or credentials were changed. The
actual shared cooperative-exit guard returned **OWNER_EXIT_REQUIRED** on both
installed workers (`80..81`): their versions have no authenticated CLI. No
production deployment task was launched and nothing was force-stopped.

**Exact required owner action:** on each PC, right-click the Organizer Worker
tray icon and choose **Exit worker**. If jobs are active, choose **Yes / finish
current jobs**, never **No / stop**. Closing the console does not exit the worker.
This action was requested through the supported Telegram notifier. It is a
technical cooperative-stop requirement, not another merge/install permission.

After that action: fresh host/user/jobs/maintenance preflight; run the canonical
updater in a bounded LIMITED interactive owner task using this same package;
preserve Rick's fresh admission intent and explicitly resume Danny; allow ten
minutes for real probes; independently verify selected/live path/hash/source,
owner/session, fresh probe/coordinator connected/accepting, new CLI and retained
identity/settings. Do not blind-reinstall or replace files by hand.

Real coordinator-accepted nondestructive jobs on each PC remain **NOT TESTED**.
No authorized production fixture was identified and no public post, backend
activity or charge was invented to make that check green. At that historical checkpoint rollout was **BLOCKED / NOT COMPLETE** despite
pre-merge gates passing. The later authorized continuation resolved it and
verified BOTH installations; production-job acceptance remains separate.
