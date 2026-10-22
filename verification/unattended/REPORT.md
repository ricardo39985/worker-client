# Unattended owner lifecycle — current gate ledger

**Not merged, installed, enrolled, fresh-boot-verified or production-tested.** A short-retention, code-only CI artifact was prepared and independently rehashed; it is not an install or product release.
Draft PR9 branch `feat/unattended-owner`. Prior product529/bookkeeping086 remains
unchanged on both PCs. Owner AWAY: no reboot/logoff/lock/shutdown/suspend/session
disconnect or production lifecycle experiment. Windows password enrollment, when
authorized, is LOCAL secure UI only—never chat/scripts/argv/env/files. See
[owner guide](../../docs/unattended-owner.md).

## Latest exact-code acceptance

The latest completed full gate was Windows run `39699676289` at exact source
`e66225ca0f567d4a1d2a5801f4585fcbf45247bf`: MSVC Release, all24 registered native
behavior suites, actual foreign-owner denial and forged-server zero-request-byte
security gate, actual restricted same-owner/non-elevated/headless media fixture and
code-only packaging all passed. The fixture exercised current-profile DPAPI,
protected bootstrap entry, restart continuation, actual typed FFmpeg child,
hash/decode, admission fencing, desired pause preservation, 52 lease renewals,
reserved BEGIN rejection/expiry, durable retry, wrong-ACK exit blocking and
matching-ACK exit. It explicitly reports `real_coordinator=false`,
`password_task_registration=false`, `cold_boot_test=false`. See retained job log73
and exact artifact verification
[74-code-only-artifact-e662-verification.md](74-code-only-artifact-e662-verification.md).
The artifact has seven checked payload hashes and a verified source archive; its
receipt says no task registration, cold boot, production job or PC installation.
This is not fresh-boot, password-task-registration, actual TLS/coordinator or
production-job proof.

Linux Release CTest and full C++ ASan/UBSan each pass8/8 on exact docs/evidence
head `fcc5351` (logs69/70). Previous focused Windows policy run39692970248 at
`f9edbb0` passed; focused tests are not release acceptance. A subsequent ledger
commit will be documentation/evidence-only; rerun all required gates on the final
source revision before release. Fresh independent security review remains open.

## Current code-only package gate

Run `39693119523` at `bbeca99` again passed MSVC build, 24/24 CTest, foreign-owner
security and actual synthetic media acceptance, but the exact-source package step
failed because the acceptance receipt omitted the already runtime-checked
`noninteractive_window_station` boolean. No artifact was produced. Log65 retains
the real package-policy failure. Test fixture `63340ac` now records that property
only after `windows_unattended_fixture.cpp` verifies the actual station flags.
Workflow `39695571198` at `63340ac` again passed build, 24/24 native tests,
owner-security denial and media fixture, but the package gate still failed: the
limited runner's persisted acceptance JSON came from the inner control fixture
before outer bootstrap/station properties were added. Stdout had them, persisted
package proof did not. Log68 retains this distinct failure. `478c68f` now atomically
rewrites the same acceptance receipt with the exact verified outer properties;
no gate is weakened and media itself must still pass. Workflow `39697201776` passed all steps at `478c68f` and produced a verified
code-only artifact; workflow `39699676289` then repeated every step at exact
source `e66225ca0f567d4a1d2a5801f4585fcbf45247bf` and retained the final tested
code-only artifact `worker-owner-e66225ca0f567d4a1d2a5801f4585fcbf45247bf-39699676289`
(artifact id12392709270, short retention). Downloaded artifact hashes and ZIP
integrity were checked. `65` and `68` retain the two package receipt red gates and
subsequent causal fixes. No PC installation or product release is claimed. Any
new source/evidence commit after `e66225c` requires exact-final-source gates before
being considered a release candidate.

## Owner-only / production gates remain closed

No secure local owner password/task enrollment, fresh zero-interactive-user boot,
physical GPU capability probe, real coordinator/TLS job, PC install, reboot or
merge has occurred. No release artifact is currently claimed. Fresh independent
security review is still open. Merge requires that review and all exact-final-source
gates; PC activity requires the owner present and separate explicit authorization.
Do not infer successful production integration from this synthetic fixture or
protocol documents.

## Retained causal failures and actual greens

| Evidence | Actual result / scope |
|---|---|
|01..04|Portable lifecycle TDD missing/behavior red, then six suites green|
|05 /run39602547874 /6d4552a|Native handler genuinely rejects restart: Unknown control command,12/13|
|06 /39604335617 /19b5715|MSVC missing protocol declaration/SDK run-level type, causally fixed|
|07/08|Genuine portable false-processing readiness red ->6/6 green; existence is NOT true|
|09/10|Genuine durable reservation fence red ->6/6 green; journaled expiry/completion cannot disappear between admission removal and outbox publication|
|11 /39606508337 /e79794d|MSVC attached-tray missing control/protocol headers, retained and fixed|
|12 /39608163923 /f531dd5|Actual MSVC+14/14 native suites and real LocalService foreign denial/forged-server zero bytes GREEN, earlier head only|
|13 /39610158748 /0c23d28|New native fixture target/crypto/command-vector compile errors, fixed using real target/APIs|
|14|Actual Debug ASan/UBSan leak/halt, IPOoff:6/6 suites green; all C++ targets instrumented, not Python|
|15 /39611978554 /4094b9e|13/14: new TEST omitted required package manifest SHA; security check NOT weakened|
|16 /39613820411 /155deba|Genuine unknown-startup overwrite red; new package TEST omitted real stop/start/verify ports; privilege stripping alone still elevated. Strict guard kept, documented LUA_TOKEN used|
|17 /39617605533 /41ac67d|New Boost JSON/string test comparison compile error, corrected typed read, not weakened expectation|
|18/19|Genuine busy/unverified PID restart red ->6/6 green, no request to PID0 or alternate activation|
|20|Incremental full C++ ASan/UBSan configuration6/6 green|
|21 /39620033533 /4512180|Genuine native writable-ancestor, pending/cancelled enrollment and unknown-script overwrite reds; fixes follow, no skipped cases|
|24 /39623888209 /62ca7e5|Actual MSVC+16/16 full native contracts GREEN, actual foreign denial/forged zero bytes GREEN. Actual normal-token/current-profile DPAPI bootstrap remains alive but CORRECTLY refuses readiness: processing_ready=false, FFmpeg init0xc0000142|
|22/23|Genuine LF/CRLF artifact identity red ->portable7/7 green; Windows check remained required|
|28 /39626684788 /e841843|16/17: canonical EOL still differs on Windows file(WRITE); fixed via literal configure template with explicit UNIX newline (preserves variable-like text). Private fixture station ALONE did not fix descendants' FFmpeg init0xc0000142. No claimed pass|
|25/26|Genuine artifact producer leaking synthetic extra credential/replacing output/missing-companion/symlink red ->4 behavior cases green; code-only fixed inventory/new output/rehash|
|27/29/30|Portable Release8/8; full C++ ASan/UBSan configuration8/8 at73fbde3. Three Python checks not instrumented|
|31 /39631239554 /73fbde3|17/18: standalone CMake script policyCMP0053 expanded literal template early on Windows; explicit NEW policy correction follows. New noninteractive CLI also exits0xc0000142; no backend success|
|33 /39633410360 /5127a79|Windows canonical launcher EOL GREEN. Genuine signer-missing-key creates identity red. Independent bootstrap gates run despite backend failure: actual restart_exit0 (expected75), malformed headless hidden-UI timeout, CLI0xc0000142, CPU0xc0000142. NONE waived. Actual reduced token still HIGH integrity12288 and default object DACL onlyBA/SY write +logonSID read|
|37 /39637183196 /747b5a0|Actual MSVC+18/18 registered suites GREEN; actual foreign denial/forged0byte GREEN. Media stops before fixture: adjusting own test token defaultDACL error5, source handle lackedTOKEN_ADJUST_DEFAULT. Source20e77da requests that right, no privilege/SID/session/logon changes; normal test context still pending|
|34/35/36|Three genuine secret-erasure/exception/exhaustion behavior reds ->Release8/8 and full C++ ASan/UBSan configuration8/8 green; native integration subsequently compiled/tested39|
|38 /39639250999 /20e77da|Extracted task-security module missing explicit ow/control.hpp declaration; MSVC compile red retained, corrected, NOT a behavior-red claim|
|39 /39640465920 /c984441|Actual MSVC;17/20 suites. Genuine machine-anchor owner-writable acceptance and task INHERIT_ONLY permission reds, genuine fifth intentional restart/limited crash-budget red. Preexec test positive fixture callback accidentally resolved helper-local c instead of case: repair explicit closure, NOT claimed security red yet. Actual real foreign/forged group GREEN; actual limited/private current-profile DPAPI/headless/bootstrap/converter/restart/expiry/lease/hash/decode/wrong+matchingACK media group GREEN, see below|

The native diagnostic was not hidden with CPU fallback, a longer arbitrary timeout
or disabled readiness. It observed headless=true, identity=true, token_elevated=false,
**session2**, connected=false, processing_ready=false. This is a current/cached
profile test, NOT fresh boot or Session0. The fixture now constructs an owner-only
private noninteractive station without changing any existing desktop ACL or owner
session. Crucially, Microsoft documents that without inherited station handles or
explicit lpDesktop, USER32 falls back to the interactive logon's station. The safe
child handle list intentionally excludes desktops. New source explicitly names
the ACTUAL parent's station/desktop for both bootstrap fixture and contained
children, keeping handle containment and exact owner/limited checks unchanged.
That correction ALONE did not solve console descendants'0xc0000142. Primary process/thread-connection docs were read/retained. Later actual token metadata revealed inherited HIGH IL/admin-only default object DACL surviving LUA reduction. Fixture now LOWERS integrity and gives only the unchanged user/SYSTEM default object access, no added privileges/credentials/session changes; actual native result still pending.

Actual39 media group NOW GREEN: missing/corrupt key no implicit creation, current-owner DPAPI, real CPU probe, noninteractive limited enrollment rejection4/no UI, restart exit75, replacement PID/pause/identity, explicit drain0 and malformed headless2/no UI. Then actual admitted converter finished/decoded/hashed,57 lease renewals, reserved BEGIN veto +its expiry/matchingACK, result retry, wrongACK blocked exit and matchingACK released exit. Actual source c984441, actual session2/private nonvisible station/non-elevated user, NOT fresh Password logon/cold boot/coordinator work. Proper own-user object defaults/owner and LOWERED medium integrity fixed the synthetic token context without disabling probe/token/hash/readiness/containment guards. Native source6d39 additionally verifies/prints actual final integrity. Task registration/password=true was NEVER claimed. New intentional restart lifetime beyond three crash retries genuinely fails39; corrected helper still needs production wrapper integration and native green.

## Independent security review — NOT closed

Immutable-f531dd5 independent review found a HIGH pre-execution trust gap still
applicable: checking native image/root ACLs AFTER current.json selects code cannot
protect the first execution. A self-hash in the mutable manifest is no independent
trust anchor. New native TDD19 tests broad manifest/ancestor ACL, ancestor junction,
mutable manifest+image despite external approval, and replacement after validation.
A new machine-anchor test genuinely caught owner-writable bootstrap acceptance39. Full preexec negative cases have NOT yet run because a positive callback scope error stopped that test; repaired closure is awaiting native behavior run. No fallback to passing fixtures or claim of a high finding closed. Planned correction
uses an admin-owned/owner-read-only protected bootstrap OUTSIDE mutable owner
state, external approved image hashes and held handle/ancestor checks before any
selected code executes. No installation, task or credential action has occurred.

Medium task-DACL full rights were added earlier; actual39 caught INHERIT_ONLY acceptance. Source6d39 now rejects unexpected ACE flags as well as incomplete rights/foreign trustees; actual next native green required. Machine-anchor policy now separately excludes user-writable code/ownership and allows only trusted OS/admin writers; real source test green still pending.
Low bearer heap-copy finding: bounded secret-origin parser now binds BOTH temporary
and result storage to caller-owned non-fallback arena and wipes input/workspace on
success/exception/exhaustion, actual portable red->green34/35 and sanitizer36.
Actual39 compiled/tested native integration; final independent recheck remains. HIGH protected bootstrap/preexecution and legacy caller-independent continuation remain unfinished; no final release/merge claim.

## Implementation/security being gated

- Existing normal-owner DPAPI/key/config/journal only, no silent identity creation,
  no elevation/service SIDs, no reparse/escaped selected image/unsafe leaf OR
  ancestor ACL. Exact module/hash/instance lock, owner-only bounded peer-verified
  remote-rejecting IPC. Failed-live auth/protocol is NOT absence or offline fallback.
- Restart admission fence separate from durable desired pause, admitted/running/
  journal/unacknowledged states wait. Exact verified process handle retained; busy
  unverified instance is waited, never targeted/spawned over.
- Secure LOCAL same-owner elevated UI ONLY for task registration. Password/LUA,
  fixed script/action/SHA, owner/SYSTEM task DACL, enrollment-id provenance and
  pending/enrolled transaction; unknown prior task/file retained. Password buffers
  wiped on every path; Windows alone stores its protected task credential.
  Boot delay, no hard-stop/duration cutoff, battery/idle/network/wake/retry/expiry
  policies inspected. No S4U/autologon/SYSTEM/admin engine/DPAPI downgrade.
- Schema2 seven-file immutable code package, schema1 compatibility, canonical
  updater strict enrolled task/readiness and no legacy downgrade. Pending/cancelled
  enrollment preserves legacy mode. Explicit RePair temporarily pairing_repair,
  preserves Windows task/credential, successful app pairing restores enrolled id.
  Full setup stages instead of overwriting immutable versions and creates/reuses
  reviewed helpers without clobbering operator content; whole full-setup publication
  remains per-file as documented, canonical version update has rollback swap.
- Optional attached tray is a separate owner client; close/logoff not job stop.
  All CLI/tray atomic settings retain shared authoritative handlers.

## Exact-source artifact preparation

Manual CI only; optional package_worker requires full native media/security
success, exact full workflow/source tree, fixed inventory/manifest hashes and
source ZIP. No artifacts have yet been claimed final. Official pinned transport
review: [CI dependency review](CI-DEPENDENCIES.md). Fourteen explicit code/manifest/
source/notice/proof paths, no hidden/profile/Temp-root wildcard, no overwrite,3day
retention. No FFmpeg program, installed configuration, DPAPI material, journal or
job workspace. Native receipt explicitly leaves password-task/cold-boot/production
jobs/installed-on-PCs false. No production install/release/deploy action.

## Remaining distinct acceptance gates

1. Final portable Release/full C++ sanitizer and actual MSVC/all registered suites,
   actual foreign owner/security, actual limited noninteractive bootstrap/typed
   child/lease/hash/decode/ACK, legacy migration/rollback and independent review.
2. Normal green merge/push, exact-main rebuild/test/hash/package. Not old529 bytes.
3. Actual LOCAL owner credential task enrollment/readback and real normal token/
   profile/DPAPI/TLS/hardware on EACH PC. No implicit success from configuration.
4. Separately authorized owner-ready fresh boot, zero owner interactive sessions,
   actual QSV/AMF/NVENC/multi-GPU and user-profile DPAPI/TLS in that exact context.
   No lock/logoff simulation now; current-profile or session-number changes do not
   substitute. Danny CBS/WSL reboot gate belongs to the environment lane.
5. Authorized private non-posting coordinator-accepted job per PC remains NOT
   TESTED; synthetic fixture/connected/accepting is not production work.

SSH readiness is independent: both already Automatic/Running and accepted the
strict pinned key. No SSH change/restart needed. Recovery reset0/count0 means no
SCM crash action, not self-healing. Historical Danny late post-boot/no console
SSH is not prompt recovery after every boot and NOT worker prelogin proof.
Exact owner-local receipts remain outside source; no daemon config/authorized_keys/
secrets/signed URLs copied into repository or artifacts.
