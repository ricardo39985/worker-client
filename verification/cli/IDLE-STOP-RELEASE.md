# BOTH PCs upgraded — 2026-10-21 11:48 UTC

Latest Rick authority explicitly permitted stopping/restarting exact **safely idle**
old worker/console processes. It superseded the prior owner-tray-only blocker,
not the no-active-job-loss boundary. Same preserved implementation session and
already verified shared artifact; no duplicate build, agent, re-merge or installer.
Only one PC was updated at a time, leaving the other available.

## Actual per-PC verification

| PC | Installed AND actual live | Real startup evidence | Live admission / CLI |
|---|---|---|---|
| Danny | PID19664, session1, expected owner SID, actual token NOT elevated; exact selected/live source529ec104 and hashdf8389fa... | START11:24:38; PROBE RESULT11:29:57 cpu,nvenc; two schedulable GPUs RTX3070/3050; CONNECTED11:29:57.619 | authenticated SSH0 status/resume twice/config14 readback PASS; connected, processing_ready, accepting true; paused false |
| Rick | PID32360, session1, expected owner SID, actual token NOT elevated; SAME selected/live source/hash | START11:39:51; PROBE RESULT11:43:00 cpu,qsv,amf; two schedulable GPUs; CONNECTED11:43:00.654 | independent SSH0 status/resume twice/config14 readback PASS; connected, processing_ready, accepting true; paused false |

Final fresh CLI readback11:48:06/08 on BOTH: current.json and actual image path/hash
agree; active0, resource reservations0, pending_results0, FFmpeg0, release tasks[].
Agent version0.1.2 and full compiled source read from actual authenticated live
CLI. PE FileVersion resource absent(null), not fabricated. Probe results are
startup samples, not universal job/model compatibility or production-job proof.
Danny's conservative GPU capacity remained n=1 after a hard-link-already-exists
capacity-probe fallback; no higher-concurrency pass was invented.

Compiled product source: `529ec104931a17ad4997771bb8bfad2eca5bcd2d`.
Worker SHA256: `df8389fa247318e07754aaf271eb30965ea1b18c7a396f0c58a79e871e029aa1`.
Package manifest SHA256: `665652f2687f755994e038978f92a2c7f8862bad53b74b9b4285dce4f2b96702`.
All companions/reference bytes match the shared manifest. Later commits contain
release evidence only, not rebuilt/relabelled product binaries.

## Safe acquisition/stop paths actually used

Exact historical sourcee4366c3 was inspected. WM_CLOSE opens the viewer, not
shutdown; WM_QUERYENDSESSION requests cancellation and was NOT used. Old public
pause rejects new offers AND previously reserved BEGIN under admission mutex.
Accepted offer replies follow durable reservation; job launch follows journal
start. Pending results remain in the outbox until matching ACK.

**Danny:** already operator-paused; actual LIMITED owner inspection and three
repeated public-journal/tree/network snapshots found zero reserved/running/
pending/outbox, no encoder or maintenance, console/conhost only. Actual existing
native **Exit worker...** menu invoked through UI Automation; worker exit0 and
console natural exit. No termination. Then unchanged canonical updater ran under
the existing LIMITED owner, explicitly resumed Danny, retained protected identity,
settings/limits/startup and immutable old artifact. Actual real probe took5m19s;
ten-minute readiness window was sufficient.

**Rick:** fresh initial intent was accepting/unpaused. Attempt to invoke actual
Pause menu returned **Access denied**, safely; no pause/stop was claimed from that
failed action. Retained failure receipt97. Under the new idle-stop authority,
a separate exact-handle quiescence gate was used:

1. Correct PID16628/creation time/path/hash/SID/session verified; exact console
   child18992 identified; repeated zero journal reservation/work/result/outbox,
   no encoder/maintenance/unexpected descendants or extra transfer sockets.
2. Suspend ALL worker threads, preventing new claims across the decision. In one
   synchronous native region: read-only journal zero check, native full child/
   encoder snapshot, second read-only zero check. Busy/nonzero/unexpected/slow
   checks abort and resume in finally; no SQL writes. Actual frozen region62ms.
3. Terminate only that exact verified **idle** worker handle, wait signaled;
   console exits naturally. No wildcard, console or FFmpeg kill. No accepted
   journaled job/reservation or durable unacknowledged output was discarded.
   An unjournaled unread offer is not an accepted claim: actual old implementation
   journals reservation before sending accepted offer_reply.
4. Same unchanged canonical transactional updater then started Rick under existing
   LIMITED owner, restoring original accepting intent. Initial stale runtime PID
   diagnostics were retried; actual new process/probe verified before success.

No guessed coordinator API/global admission changes, raw SQLite/settings edits,
observational spoofing, software/OS/account/network/backend/signing/reboot changes.
No real coordinator work was cancelled. No active job was stopped to meet a timer.

## Cleanup and evidence

Actual owned inspection/exit/gate/deploy tasks deleted; final BOTH task lists empty.
Exact hashed one-off stop helpers removed from protected remote staging after
local audit retention; package/canonical updater/results retained for recovery.
No parent/user Backend/Tunnel panes or environment work were changed.

- `86..87`: actual canonical deployment results (protected identity/settings retained).
- `88..89`: final actual installed/live SSH CLI/admission readback.
- `90..91`: independent actual process token/owner/session/path/source/hash.
- `92..93`: fresh real startup/probe/connection epochs.
- `94..95`: actual task/helper cleanup receipts.
- `96`: actual Rick exact idle acquisition gate/stop receipt.
- `97`: safely failed Rick UI Automation attempt.
- `98`: actual Danny graceful exit and repeated journal/tree snapshots.
- `99`: actual per-PC SSH CLI/config readback summary.

**Deployment verification: COMPLETE on BOTH PCs.**
**Real coordinator-accepted job on EACH PC: NOT TESTED.** No authorized private
nondestructive fixture was identified. Remaining acceptance request: provide/
authorize one private, non-posting fixture for one coordinator-accepted job per
PC, with no surprise production change or cost. Do not relabel synthetic job
acceptance or connected/accepting state as a real production-job pass.
