# Worker-flex r2: stage concurrency checkpoint

Branch: `fix/worker-flex`; baseline: first-pass checkpoint `f2b52d5`.
This is an implementation/native-gate checkpoint, **not a rollout acceptance**.
The operator subsequently authorized related Windows staging/build/test/encode
work and conditional merge/push/install, superseding the earlier read-only-PC
boundary. That permission does not authorize production-server changes.

## Implemented

- Bounded parallel rendition preparation; independent child directories, logs
  and progress; one streaming delivery lane. Ready posters may overtake video.
  Exceptions cancel siblings; all preparation threads/contained children drain
  before return. Result metadata retains role identity, not completion order.
- Atomic input-escrow handoff and per-stage CPU/RAM reservations. Encoder leases
  end after encode/retry process-tree drain, before separately budgeted full
  decode/timestamp validation. CPU retries reacquire resources. Uploaded output
  and no-longer-needed source scratch envelopes can be reclaimed early.
- Per-device proven-session ceilings come from successful simultaneous encode
  trials, not installed-card count or a constant two-session assumption. Trials
  observe positive-frame activity, validate all outputs, and compare bytes with
  the single-encode reference. The paced qualification pass is excluded from
  throughput calculation. Bounded sweep points are 2, 4, 8, ... and the ceiling;
  these establish tested points, not a mathematical maximum for every workload.
- Feedback starts conservatively. Healthy measured windows, exercised trial
  concurrency and validated work permit growth; pressure, foreground activity,
  unknown or stale measurements back off/defer. CPU/RAM/disk and busiest GPU
  engine are considered. Reductions do not evict or unaccount running work.
- Interactive-session input age and bounded WM_NULL response checks inform
  foreground deferral; child CPU priority backs off during foreground pressure.
  Capacity/reservations, sampled use, trial versus qualified limits, per-card
  sessions/VRAM and queue reasons appear in console/log records.
- Functional measured encoders are ranked, not discarded because another
  backend was 5% faster. A successfully re-probed smaller child RAM cap is used
  instead of inventing a larger cap for the same measured geometry. Observed
  allocation failures can trigger bounded, atomically reserved RAM growth.
- Fixed 30-second video validation no longer manufactures a hardware failure;
  validation remains bounded by the attempt's remaining overall deadline.

## Enforced versus sampled

Hard guarantees: atomic declared reservation totals within configured machine
budgets; typed/currently probed backend selection; independently bounded encoder
sessions; Windows child-job CPU-rate and committed-memory limits; cancellation,
lease fencing and process-tree drain before release; input/output hashes and
full output validation; durable result retry/ACK protocol. Closing the viewer
still does not own the controller or its children.

The ~90% value is a **feedback utilization target**, not a promise of 90% usage
or an OS GPU quota. GPU engine/VRAM, physical RAM, disk and desktop responsiveness
are sampled. WDDM background priority is a best-effort hint, not a hard GPU time
or memory cap. The parent/controller, driver allocations, shared memory and
unrelated applications are not all bounded by child-job private-commit quotas.
A single encoder or external process can reach 100%; arbitrary foreground work
cannot be guaranteed never to suffer any degradation. WM_NULL response is not
a measurement of game/render-frame latency or every operator workload.

## Gates at this checkpoint

- Linux Release: all six suites, exit **0**, 293 contracts (91 core/capacity,
  13 pipeline, 18 rendition, 84 adaptive, 64 progress, 23 Python dependency).
- MSVC Release, Rick: **10/10 registered suites**, every suite exit **0**,
  `WINDOWS_GATE_EXIT=0`.
- MSVC Release, Danny: **11/11 registered suites**, every suite exit **0**,
  `WINDOWS_GATE_EXIT=0` (Python dependency suite also available there).
- Current archive SHA-256:
  `4a049e796d255ff20df39c176392d1674e52308160edad865573a43aee796252`.
- Initial native failures were real missing-include/declaration errors and a
  measurement-harness function-name typo; retained separately, not counted green.
- Runtime reds include pipeline non-overlap, double-counted live RAM, withheld
  slower-but-functional encoder, geometry overriding an actually proved RAM cap,
  and pressure failing to reject a previously reserved BEGIN. The initial API
  additions also had compilation reds; those are not claimed as runtime failures.
- Globally instrumented ASan/UBSan with leak detection: all six suites,
  293 contracts, exit **0**; see `linux-sanitize.log`.

The old display-adapter footprint fixture previously claimed a successful
512-MiB trial while describing an observed >562-MiB child. It now explicitly
rejects the failed 512-MiB trial and admits the actually successful 768-MiB trial;
separate behavior coverage proves that genuine successful 512-MiB evidence is
not overwritten by a geometry estimate.

## Measurement procedure / still open

The opt-in `ow_flex_benchmark` uses real `Worker::execute`, admission, journal,
factory, child containment and validation. Only source/PUT ports are local files;
no coordinator/storage account is contacted. The isolated constructor is private
and accessible only to the linked measurement harness, never config/network.
Before source is `f2b52d5` plus the reviewed local-storage ports in
`baseline-measurement-ports.patch`; it is **not** the installed `456eecca` binary.
Pins hold codec/device/settings constant; fixed-backend waits intentionally avoid
comparing CPU-CRF output against GPU-CQ output. Compare artifact bytes per role.
This is like-for-like offline compute throughput, not production jobs/hour.

Each PC uses `%LOCALAPPDATA%\Temp\ow-worker-flex-20261018-r2` only. A temporary,
manual-start, normal-user interactive observer records input age and WM_NULL
response only (no titles, contents, activation or injected input). Danny currently
has known response samples; Rick currently has no responsive foreground window.
Do not claim active-desktop impact on Rick from that observation.

Before/after jobs/hour, per-job wall times and sampled CPU/RAM/VRAM peaks are
**not yet accepted**. The controlled after experiment may test larger hard caps
(about 90% logical-CPU ceiling, conservative per-machine free-RAM ceiling) via a
measurement-only environment flag; existing live configuration is unchanged.
Those caps may be applied only after actual successful qualification/measurements.
The exact 24,609,052-byte asset replay remains pending.

The deployed coordinator still independently compares nominal `r_frame_rate`.
Historical 518-frame/90k evidence in `verification/fps/` is an acceptance risk for
the changed timing policy, independent of the rounded-rate refusal correction.
Do not claim general coordinator acceptance from timeline-only/offline success.

No merge, push, release, installation, startup change or production-server write
has occurred. Both installed workers remain untouched. Staging and the temporary
observer task must be removed, then verified absent, after the experiments.
