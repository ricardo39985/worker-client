# Danny local Gemma worker acceptance — 2026-10-22/23

Product source: `9994395231e64fe48d0454a1269db614ada3e92a`, published to worker-client `main`.
Worker SHA256: `0ef241d05f72dd65289cb8992587d7b40873f892e7c354d9ed6797ff8b5a7447`.
Package manifest SHA256: `ce296827834534b07801e9e42efac602a43c9e7739ebc8f5c042eb018d937b77`.
Later acceptance-runner and evidence edits do not rebuild or relabel this product.

## Verified gates

- Actual Windows MSVC Release compilation succeeded on Danny. All 14 CTest suites
  passed under the existing limited interactive owner (57.01 seconds).
- The 29 Python behavior tests passed on Linux. The local aggregate C++ build was
  blocked by absent Boost headers; no local C++ pass is claimed.
- Pinned runtime/model setup exercised real text, image, video and audio inference.
  Native Doctor separately ran the adapter inside the worker's Windows Job:
  2,533 MiB peak committed process-tree memory, 44.127 seconds, verified true,
  dispatch disabled, non-elevated. That probe used `eed97fe`; the product's only
  intervening change normalized pixel aspect ratio in the acceptance fixture.
- Real visual retrieval passed on Danny with eight CPU threads in 43.212 seconds.
  The fixture has visible boiling pasta, no audio or supplied caption/filename
  text. Query `pasta` scored 0.633888 on the original clip; in a second clip with
  unrelated blue footage first, the 20–25 second pasta section scored 0.623744
  versus 0.619183 for the 0–20 second blue section. These are relative cosine
  scores on one controlled fixture, with a small margin, not calibrated confidence
  or evidence of broad retrieval quality. No woman/chicken fixture or app query
  was tested. See fixture-provenance.md and recognition-result.json.

The first fixture preparation attempts failed because concatenated videos had
incompatible pixel aspect ratios. The harness now normalizes them. A subsequent
one-thread run hit the adapter's bounded request timeout; the explicit eight-thread
run passed. The acceptance runner exposes and records its thread budget. No
production source file was silently truncated or marked indexed after failure.

## Integration boundary

This verifies local worker inference. Coordinator v2 scheduling, section-vector
persistence, authorized query execution and app search remain unimplemented in
this worker-only change. `gemma.json` has `advertise:false`; Danny retains paused
operator intent. No v2 capability is advertised to the current older coordinator.
The worker never accepts the old v1 embedding backlog. Legacy processing and
OpenRouter retirement remain gated on the later full search acceptance.
No production asset, account or enrichment database row was changed by these tests.

## Deployment

The canonical owner-scoped transactional updater is used, retaining the existing
startup task, credentials, pairing key and settings. The old artifact lacked a
control CLI. Its actual native Exit menu was selected using Microsoft Active
Accessibility only after that menu reported Resume new jobs and zero jobs. The
process exited; no process termination was issued. UI Automation initially exposed
an empty pane, and the exit wrapper could not obtain a usable old-process exit
code. Those attempts were retained on Danny. A separate continuation verified
zero remaining worker/media processes before invoking the canonical updater.

The first updater supervisor was interrupted with 0xC000013A after startup but
before transaction finalization. The documented rollback restored the immutable
old artifact. That old artifact uses a plain startup banner and no modern status
file, so its generic readiness wrapper could not verify it. The canonical recovery
primitive then verified the exact already-restored manifest, actual PID/path/
creation time/session and its current connected log epoch with compatible
readiness callbacks. No worker was force-terminated. Only the stalled owned
recovery supervisor was stopped after checking its only child was conhost.
Recovery completed before the final canonical update was attempted.

The final no-console supervisor completed with exit 0. The old tray's actual
idle Exit action returned exit 0, the updater verified the new worker, committed
the version swap, and confirmed protected identity and settings retention.
Independent live verification at 2026-10-23 00:39:01 UTC found:

- PID 30460, interactive session 1, actual owner matches, actual token not elevated.
- Exact compiled revision and live binary hash match the product pins above.
- Connected, processing ready, paused, zero active attempts and pending results.
- Its own real Gemma startup probe passed; dispatch remains disabled.
- No transaction recovery marker remains.

See final-verification.json, deployment-result.json, recovery-result.json and
cleanup-result.json. All ten task-owned one-off registrations were removed;
the existing production startup task, immutable previous version, provisioned
runtime, release package and retained failure/success evidence remain.
