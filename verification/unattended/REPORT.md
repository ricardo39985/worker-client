# Unattended owner lifecycle — in-progress gate ledger

NOT an enrolled/deployed/cold-boot-verified release. Branch
`feat/unattended-owner`, draft PR9. Prior product529 / bookkeeping086 remains
unchanged on both PCs. Owner AWAY: no reboot/logoff/lock/shutdown/suspend/session
 disconnect or production lifecycle experiment. Do all independent preparation;
Windows password enrollment is eventually LOCAL secure UI, never chat/scripts/
argv/env/files. See [owner guide](../../docs/unattended-owner.md).

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
|27|Portable Release8/8 green after package contracts (Python checks not sanitizer instrumentation)|

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
This causally motivated correction still needs the next native run, not a pass
claim. Primary process/thread-connection docs were retrieved/read and retained.

Pending native behavior: successful real CPU initialization, actual restart exit75
(Windows-managed retry even after CLI departure), replacement PID/pause retention,
normal drain exit0 and malformed headless no-UI exit2, then real admitted converter
restart/lease/output/decode/retry/wrong/matchingACK. The CPU gate has so far prevented
those later cases from executing; they are NOT claimed tested. Restart/invalid-mode
source still needs its genuine-red correction once that path executes.

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
