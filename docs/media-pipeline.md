# Bounded native media pipeline

The worker already supports independently admitted jobs and uses native FFmpeg
processes. This change keeps encoder/decoder/filter thread settings, codec quality,
Windows Job Object limits, local preferences, and `max_jobs` unchanged.

## What changes

For app renditions, encoding artifact N+1 overlaps streaming checksum/upload of
artifact N. There is at most one FFmpeg process and one transfer task per admitted
job, plus at most one completed artifact waiting for transfer. Delivery remains
ordered; the next delivery cannot start until the previous one has joined.

The pipeline owns its asynchronous task until it completes or fails. A failure in
either stage requests cooperative cancellation of the other, retains the first
exception, and joins before returning. Metadata returns to the orchestration
thread. Only a complete result set can be persisted as success; workspace cleanup
and admission release happen after all pipeline work has stopped. Cancellation
and the overall timeout are checked again before recording success.

Encoding and checksum/upload have independent progress counters so concurrent
stages cannot reset each other's timing/byte totals. Both appear in watchdog logs.

The diagnostic single-output path remains sequential. There is no new coordinator
message, signed-URL permission, GPU capability, FFmpeg binary, encoder quality
setting, or automatic installation/startup change.

## Resource boundaries

FFmpeg remains isolated in a Windows Job Object. Streaming hash/WinHTTP and the
extra transfer thread run in the controller, outside that Job Object's CPU/RAM
caps. Existing file streaming and declared scratch budgets remain in place;
outputs are not loaded into RAM. Admission limits are not raised. Aggregate
controller overhead and network contention still need a real job trial before
rollout. A CPU/thread reservation is not an unlimited parallelism allowance.

Cancellation of synchronous WinHTTP is cooperative, between bounded operations;
the pipeline cannot promise instant interruption while Windows awaits a response.
No resource reservation is released until the transfer has joined and the FFmpeg
process tree has drained. Partial remote files remain per-attempt staging objects
under the existing coordinator publication/fencing/retention rules.

## Tests and reproduction

`pipeline_contracts` covers ordered results, empty plans, actual stage overlap,
bounded backpressure, final-transfer joining, cross-stage failure/cancellation,
first-cause preservation, partial-result rejection, and independent pipelines.
`worker_contracts` also verifies four one-CPU app jobs with a fifth rejected until
a cancelled job actually finishes. A Windows contract exercises upload failure
while a real contained child process tree is active and verifies both root and
descendant have terminated before the pipeline unwinds.

A serial baseline of the prior encode/deliver loop passed the metadata test but
failed `next encode overlaps previous transfer`. The asynchronous implementation
then passed that same overlap test.

For a native smoke test, build on Windows and run:

```powershell
cmake --build build-win --config Release --parallel 2
ctest --test-dir build-win -C Release --output-on-failure
.\build-win\Release\ow_pipeline_smoke.exe C:\approved\ffmpeg.exe C:\synthetic\source.mp4 C:\new-workspace
```

The smoke command uses actual rendition arguments and contained native FFmpeg for
both serial and overlapped executions, verifies matching artifact SHA-256/bytes,
and decodes all H.264/HEVC/WebP outputs. It injects 500 ms of **simulated** transfer
latency per artifact and performs a real local checksum; it does not contact a
real storage account. Its timing is not an end-to-end production speedup claim.
The workspace must not exist. Actual WinHTTP/storage acceptance and tray
pause/drain remain separate rollout checks.
