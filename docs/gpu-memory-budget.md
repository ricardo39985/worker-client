# Contained GPU child commit budgets

The Job Object aggregate commit limit remains mandatory. It bounds process-tree
RAM commit, independently of VRAM reservations and GPU watchdog telemetry.
The RTX 3070 A/B evidence is retained in
[the measurement ledger](../verification/gpu-budget/baseline/REPORT.md), with
[every command and exit](../verification/gpu-budget/baseline/COMMANDS.md).

The portable policy uses width, height, codec, input location and card overhead:

```
base MiB = 256 + ceil(width * height * pool_bytes_per_pixel / 1048576)
footprint MiB = base + card_overhead
budget MiB = round_up_to_128(footprint + max(128, ceil(footprint / 4)))
```

Host H.264/HEVC pool coefficients are 128/120 bytes per pixel; device-surface
coefficients are 112/108. These describe aggregate buffering/driver commit,
not the bytes in one video frame. The envelope is calibrated above successful
measurements from 460x390 through 1080p. A conservative 64 MiB card allowance
covers the observed display-adapter difference. Each successful startup sample
records peak Job Object commit and raises its per-card/codec allowance when
`peak - base` exceeds 64 MiB. A headless card is never assumed to need less
memory merely because another card is display attached. The allowance survives
in-process probe recovery; disk diagnostics cannot establish scheduling proof.

Startup tuning uses this policy at 1920x1080 instead of 512 MiB. Runtime currently
uses host input; device coefficients are explicit for measurement/future typed
surface paths, and do not silently enable a hardware decoder. Examples:

| Resolution | Host H.264 / HEVC MiB | Device H.264 / HEVC MiB |
|---|---:|---:|
| 460x390 | 512 / 512 | 512 / 512 |
| 1280x720 | 640 / 640 | 640 / 640 |
| 1536x864 | 640 / 640 | 640 / 640 |
| 1568x882 | 640 / 640 | 640 / 640 |
| 1920x1080 | 768 / 768 | 768 / 768 |
| 2560x1440 | 1024 / 1024 | 896 / 896 |
| 3840x2160 | 1792 / 1664 | 1536 / 1536 |

Values above 1080p are extrapolations until independently measured. Every runtime
hardware child also retains its startup profile RAM floor and any larger measured
card allowance. Budgets do not prove geometry support, speed, arbitrary source
compatibility or concurrent operation.

A coordinator's 512 MiB offer remains the initial reservation and duplicate-offer
identity. A live BEGIN may atomically reserve additional local RAM under the
configured machine ceiling and live headroom. Once input dimensions are known,
the adaptive reservation atomically grows to the workload budget before a GPU
child starts. If either owner cannot cover it, the job takes CPU fallback.
A successful first-stage top-up can remain conservatively reserved if the second
stage declines. Extra RAM is released only at task completion after child drain;
cancellation and lease expiry retain it. Sequential codecs use their maximum
RAM cost, as before. Per-card sessions, machine NVENC sessions, CPU fallback slots,
VRAM and scratch limits remain independent admission checks.

Child launch logs show `child_budget_MiB`; active status summaries show the actual
`child budget N MiB`. Startup phase/status and saved profiles name the budget.
An allocation error under the cap is reported as `insufficient child memory
budget: WIDTHxHEIGHT budget=N MiB (allocation failure under cap)`, including when
heap-corruption cleanup follows it. Recovery status preserves that detail.
A bare heap-corruption exit or session-busy error is not allocation evidence.
Runtime budget failures retry CPU without branding or demoting a proven card.
Other hardware/validation failures retain the existing targeted demotion path.

A card still requires successful bounded encode, decode, output validation,
identity/hash matching and tuning before scheduling. Raising a budget or loading
a diagnostic file cannot qualify an adapter. See the [gate report](../verification/gpu-budget/REPORT.md)
for exact executed evidence and limits.
