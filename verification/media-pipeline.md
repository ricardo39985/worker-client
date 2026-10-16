# Bounded media pipeline — executed verification

Date: 2026-10-16. Base: deployed source c8aec2446829bc43528f666089e99d10ddb64f10,
including its quiet-idle console fix. This is an implementation candidate, not an
installed update or signed release.

## Red / green

The extracted serial baseline passed ordered metadata but failed the overlap
contract with `pipeline failed to overlap encode and transfer` (2 contracts,
1 failure). The bounded asynchronous implementation passed the same contract.
The pipeline suite was expanded to 9 passing contracts covering backpressure,
joining, cancellation propagation, first-error preservation and independent jobs.

## Executed checks

- Linux GCC, C++20, `-Wall -Wextra -Werror -pthread`: 40 initial combined contracts
  passed; after adding the four-job admission regression, 41 combined contracts
  passed under ASan/UBSan with leak detection enabled. The deployed base's
  progress/activity contracts also passed separately.
- Actual worker PC: MSVC 19.44.35229.0, Windows SDK 10.0.26100.0, x64 Release.
  Build used the existing approved Boost 1.89.0 dependency tree in a separate
  source/build directory; the installed version and startup task were untouched.
- All seven CTest suites passed on the final deployed-base source:
  worker_contracts, pipeline_contracts, rendition_contracts, progress_contracts,
  windows_contracts, setup_contracts, setup_progress_contracts.
- Native process-tree cancellation contract passed: simulated transfer failure
  aborted and drained a real contained root/descendant pair before unwinding.
- Native FFmpeg smoke passed using actual rendition commands, Windows Job Object
  containment, real checksums and decoder validation. All three serial and
  overlapped outputs were byte-identical:
  - feed: 76,703 bytes; SHA256 ecb6c89bae0505634f80b7528dcd39ddcb11a4062bbb8ec9da8acc82a68f3db5
  - feed_optimized: 79,976 bytes; SHA256 aa2652f1a3fd00c54bae077c6d1323e91bde9e6fb07111bd1a7f87309effc31f
  - thumbnail: 6,714 bytes; SHA256 263c56ed73d22987c42f4dc12d8888a3d160d3b5681b1ff121831025cdcc7dd3
- Smoke timing: serial 2249 ms, overlapped 1675 ms, overlap explicitly observed.
  Transfers were a real local checksum plus 500 ms simulated latency per
  artifact, NOT real network uploads. This is not a production speedup claim.
- Independent read-only review: passed, no security concerns or blocking logic
  errors. Remaining suggestions: real storage cancellation/lease-expiry trial
  and integration-level lifecycle coverage before rollout.
- `git diff --check` passed. Existing unrelated setup_policy_test.ps1 line-ending
  changes were not included.

## Candidate and remaining acceptance

Native executable:
`C:\Users\reald\hermes-worker-pipeline-20261016\build\Release\OrganizerWorker.exe`

SHA256: f5a2b81758be84debc75f192d89e75670018e33d81ec7097e5ce7f0a5a432f72

The live worker was not replaced/restarted. No real job, account, or storage
endpoint was used by these tests. Existing codec choices, FFmpeg dependency,
resource/admission limits and publication protocol are unchanged. WinHTTP
response-wait cancellation and a real job's tray pause/drain remain rollout
acceptance; synchronous cancellation is cooperative, not instantaneous.

Raw logs retained locally:
- `/home/rick/.hermes/cache/scratch/worker-pipeline-red.log`
- `/home/rick/.hermes/cache/scratch/worker-portable-sanitized.log`
- `/home/rick/.hermes/cache/scratch/worker-progress-final.log`
- `/home/rick/.hermes/cache/scratch/worker-pipeline-deployed-base.log`
