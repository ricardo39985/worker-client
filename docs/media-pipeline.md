# Bounded native media pipeline

The worker already supports independently admitted jobs and uses native FFmpeg
processes. The encode/upload overlap remains bounded to one encoder, one transfer
and one completed artifact waiting for transfer per admitted job. Verified local
hardware profiles are optional; the existing CPU rendition profile remains the
safe fallback.

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

The diagnostic single-output path remains sequential. Adaptive profiles add no
coordinator message, signed-URL permission, FFmpeg binary, or automatic
installation/startup change.

## Adaptive encoder profiles

Setup first records CPU, RAM, disk and adapter identity/driver inventory, including
when `-NoConversion` is selected. Native setup then records DXGI LUIDs, driver
versions and dedicated memory. NVIDIA CUDA ordinals are resolved by the installed
driver's `cuDeviceGetLuid` result and must map uniquely to a DXGI adapter. QSV and
AMF commands select an explicit DXGI device. During every synthetic hardware run,
Windows PDH GPU Process Memory Dedicated Usage samples are matched to the spawned
FFmpeg PID and adapter LUID. Missing counters, mismatched identities, zero child
allocation, or ambiguous samples disable that adapter's profile.

The approved FFmpeg binary is measured on CPU with strict bounded 640x360 H.264
and HEVC synthetic app outputs. The validator checks the decoded stream codec,
dimensions, exact frame count, MP4 container and AAC audio. Serial and concurrent
candidates each execute two repeated stages with the same total frames and the
same one-CPU, 512 MiB RAM and 128 MiB scratch cap per child. Both include encode
and decode validation. Concurrency two is selected only when its equal-work
aggregate throughput improves by at least five percent. Otherwise the measured
CPU profile stays at concurrency one. The approved FFmpeg hash, device inventory, effective resource
limits, measured profiles, stage durations and unavailable reasons are saved in
`adaptive-hardware.json`. `limits.vram_mb` is optional: absent selects a
conservative aggregate ceiling, a positive number sets an explicit ceiling, and
zero disables GPU probes and profiles before any GPU encoder is launched. Each
positive-cap GPU probe first reserves conservative per-device live headroom
within the global cap, then watches observed child allocation and kills/drains
the contained process tree if the reservation is exceeded. Runtime admission
also refreshes per-adapter dedicated-memory headroom from PDH before reserving a
profile. These are admission limits plus an observed-allocation watchdog; Windows
does not provide a hard Job Object GPU-memory quota here, so allocation between
telemetry samples may briefly exceed the reservation.
Each synthetic GPU measurement records its tested 640x360 maximum. Before
selecting an encoder, runtime reads the actual input's first video frame using
the approved FFmpeg binary; unknown or larger dimensions use the reserved CPU
profile without scaling the delivered output.

Runtime selects measured per-device profiles for actual rendition commands. At
BEGIN, it atomically reserves the selected codec profiles and a CPU retry profile
against machine-wide CPU, RAM, scratch, global VRAM and per-device VRAM budgets,
intersected with the job's offered per-task CPU, RAM and scratch caps. Global
machine capacity is checked against accumulated reservations separately from the
per-task ceilings. Stable device IDs break equal-throughput ties. A move-only
reservation remains held through output validation, upload join, process-tree
drain and result persistence. NVIDIA uses its verified CUDA ordinal; QSV and AMF
use their selected DirectX adapter. Hardware output quality is not claimed
equivalent to CPU CRF. GPU profiles are claimed only after real encode/decode and
matching child telemetry pass; this does not claim any untested GPU hardware.

Each hardware profile uses conservative codec-specific quality controls; its
quality is not inferred to equal the CPU CRF profile. A
runtime hardware failure removes its partial file, drains the contained FFmpeg
process tree, validates a CPU retry, and only then allows the ordered upload
pipeline to read the output. Cancellation before encode prevents the helper from
starting; cancellation during validation prevents success. The worker hashes
FFmpeg on a separate monitoring thread so lease expiry and process cancellation
continue on the watchdog. Hash, adapter identity, driver and device changes
invalidate profiles while retaining CPU fallback where its evidence remains
current.

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
then passed that same overlap test. The October 17 Windows candidate smoke run
observed overlap and matching serial/parallel SHA-256 and byte counts for all
three synthetic outputs; its simulated delay does not establish production
throughput or a speedup.

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
