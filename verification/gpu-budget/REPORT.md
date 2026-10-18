# GPU child commit budget fix and verification

Application/test revision: `164685d` (initial implementation `2a209bc`).
Branch: `fix/gpu-memory-budget`, based on local main; local commits only.
The later documentation/evidence commit changes no application/test source.

## Result and scope

The protective Job Object aggregate commit cap is retained. GPU startup tuning
now uses 768 MiB at 1080p. Runtime computes a budget from resolution, codec,
host/device input pools and per-card measured overhead, reserving extra RAM
atomically under configured and live machine limits before raising the child
cap. If the reservation cannot fit, CPU fallback is used. See
[policy, coefficients and example budgets](../../docs/gpu-memory-budget.md).

Chosen budgets appear in probe phases, saved profiles, child launch logs and
active status summaries. Allocation evidence under a cap reports insufficient
child memory budget with resolution and MiB, including when followed by heap
corruption. A heap-corruption exit alone is not allocation evidence. Recovery
status retains the full classification. Runtime budget failures use CPU retry
without demoting or diagnosing a proven card. Decode validation receives the
same workload reservation and resolution context.

Reservations, session ceilings, CPU fallback slots, cancellation, lease expiry,
process-tree drain, successful-probe requirements and publication fences remain
in force. Additional local RAM does not rewrite the original offer identity;
duplicate offers remain idempotent. The original wire contract is unchanged.
No measured profile is loaded from disk as scheduling evidence.

## Measurements and physical 3070 proof

These are the **supplied, previously executed physical measurements**, retained
verbatim in [baseline/REPORT.md](baseline/REPORT.md) and
[baseline/COMMANDS.md](baseline/COMMANDS.md). They are not fresh executions of
this branch. The source report used the pinned FFmpeg on `danny@192.168.1.189`,
serial synthetic 72-frame/24fps sources and Job Object peak aggregate commit.
A failed capped peak is censored: it does not measure the memory needed to finish.

| Resolution, 3070 host H.264 | Measured peak MiB | Original cap / exit | New policy MiB |
|---|---:|---|---:|
| 460x390 | 297.43 | 512 / 0 | 512 |
| 1280x720 | 411.11 | 512 / 0 | 640 |
| 1440x810 | 424.71 | 512 / 0 | 640 |
| 1504x846 | 432.32 | 512 / 0 | 640 |
| 1536x864 | 441.15 | 512 / 0 | 640 |
| 1568x882 | 513.75, failed/censored | 512 / 0xc0000374 | 640, not physically retested |
| 1600x900 | 512.70, failed/censored | 512 / 0xfffffff4 (-12) | 640, not physically retested |
| 1920x1080 | up to 562.32 successful | uncapped / 0 | 768, physically proven in baseline |

The exact worker-source confirmation at 1080p:

| Card / input / codec | Cap MiB | Peak commit MiB | Frames | Encode exit |
|---|---:|---:|---:|---|
| RTX 3070 / host / H.264, old cap | 512 | 512.01, failed | none | 0xc0000374 |
| RTX 3070 / host / HEVC, old cap | 512 | 512.22, failed | none | 0xc0000374 |
| RTX 3070 / host / H.264 | 768 | 550.23 | 72 | 0 |
| RTX 3070 / host / HEVC | 768 | 547.67 | 72 | 0 |
| RTX 3070 / device / H.264 | 768 | 533.65 | 72 | 0 |
| RTX 3070 / device / HEVC | 768 | 520.02 | 72 | 0 |
| RTX 3050 / host / H.264 | 512 | 496.25 | 72 | 0 |
| RTX 3050 / host / HEVC | 512 | 474.02 | 72 | 0 |

All successful rows above separately decoded all 72 frames, decode exit 0.
The baseline contains 66 FFmpeg invocations, including 22 complete successful
output validations. It proves the physical 3070 completes both codecs and both
input paths at 1080p under 768 MiB. It does not establish why its per-card driver
footprint differs; the display topology was not changed in that experiment.

For 2560x1440 the policy selects host 1024/1024 MiB and device 896/896 MiB
(H.264/HEVC). For 3840x2160 it selects host 1792/1664 MiB and device 1536/1536
MiB. These larger-resolution budgets are **extrapolations, unmeasured here**.
Per-card successful startup commit samples can raise the conservative 64 MiB
allowance; the startup profile RAM floor also remains binding at runtime.

### Fresh hardware execution boundary

A fresh isolated measurement was attempted only through a read-only idle guard.
It exited **1** because FFmpeg was already running on the RTX host
(`measurement-prepare.log`). Read-only inspection identified an independent
`ow_gpu_measurements.exe` launcher and later other FFmpeg work. The final
read-only check also found a child of the installed worker itself
(`measurement-busy-final.log`). No experiment
directory was created on that host and no competing encode was started.
The other processes and their directories were left untouched. Original worker
PID 33992 remained present with its recorded creation time.

`ow_gpu_budget_probe` was compiled by MSVC as an opt-in, pinned-FFmpeg synthetic
measurement harness. It uses the actual production Child containment and budget
policy for 720p through 4K, both codecs, both cards and both input paths, with
complete output decode and exact argv/exit/peak recording. It was **not run** on
the busy RTX host. A fresh physical replay of this branch remains open. The
supplied A/B evidence and the fresh native allocation contract below are separate
levels of proof; neither is relabelled as an installed-worker success.

## Failing behavior test first

`red-configure.log`: configure exit **0**; `red-build.log`: build exit **0**.
`red.log`: adaptive executable exit **1**, 75 contracts, 74 passed / 1 failed.
The intended failure was `hardware->ram_mb >= 768`: the existing successful
1080p display-adapter fixture still selected 512 MiB despite the recorded
562.32 MiB physical footprint. The failing binary exercised the original 512 MiB behavior.
After the policy change the same contract passes and asserts the budget carried
into the actual adaptive reservation.

Additional contracts cover codec/surface/resolution selection, measured card
allowance, invalid geometry/overflow bounds, atomic RAM growth, competition,
duplicate-offer identity, cancellation/expiry retention, adaptive resize failure,
session ownership until drain, honest allocation classification and status.

The fresh Windows native contract requests a real **563 MiB VirtualAlloc** in a
contained process. Under **512 MiB** it fails with child exit **12**. Under the
policy's **768 MiB** it completes with child exit **0**, and observed peak commit
is asserted to be at least 563 MiB and below the cap. This independently proves
that the actual Job Object protection still applies and the workload budget
admits an above-old-cap allocation. It is not a physical NVENC test.

## Final gate results

All application/test sources match revision `164685d`. Linux Release and
ASan/UBSan were rerun after the Windows validation-context correction. The
initial `2a209bc` also passed all 293 Windows contracts; its log is `windows.log`.
The final Windows log is `windows-final.log`; no gate was skipped or weakened.

| Suite | Linux passed / failed; exit | Sanitizer passed / failed; exit | Windows passed / failed; exit |
|---|---|---|---|
| ffmpeg_dependency_contracts | 23 / 0; 0 | 23 / 0; 0 | not registered: Python unavailable |
| worker_contracts | 79 / 0; 0 | 79 / 0; 0 | 79 / 0; 0 |
| pipeline_contracts | 9 / 0; 0 | 9 / 0; 0 | 9 / 0; 0 |
| rendition_contracts | 3 / 0; 0 | 3 / 0; 0 | 3 / 0; 0 |
| adaptive_hardware_contracts | 78 / 0; 0 | 78 / 0; 0 | 78 / 0; 0 |
| progress_contracts | 54 / 0; 0 | 54 / 0; 0 | 54 / 0; 0 |
| windows_contracts | n/a | n/a | 13 / 0; 0 |
| setup_contracts | n/a | n/a | 25 / 0; 0 |
| setup_progress_contracts | n/a | n/a | 15 / 0; 0 |
| setup_converter_contracts | n/a | n/a | 17 / 0; 0 |
| **Total** | **246 / 0; 6/6 suites** | **246 / 0; 6/6 suites** | **293 / 0; 9/9 suites** |

Linux configure/build/discovery exits: **0/0/0**, each configuration.
Windows MSVC 19.44 x64 Release configure/build/discovery: **0/0/0**.
Final Windows helper/SSH: **0/0**. Converter suite: 26.54 seconds inside its
unchanged 60-second bound. The optional Python suite's existing registration was
not changed. All 223 C++ portable contracts were instrumented with
address/undefined sanitizers; ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1. The 23 Python dependency cases also passed.

Reproduction: `run-linux.py`, `run-windows-gate.ps1`,
`rerun-windows-gate.ps1` and `remote.py`. Existing reviewed Boost 1.89.0 was used
without installation. Linux logs and `linux-results.json` retain exact commands,
exits and test counts. Native discovery/JUnit files are in `windows-results/`.
The first native build used `git archive 2a209bc`; the final build updated only
`windows/worker.cpp` from `164685d`, the sole application difference.

## Cleanup and remaining gates

The owned Windows build directory was removed after retrieving native evidence:
`windows-cleanup.log`, cleanup/helper/SSH exit **0**, zero owned processes before
removal, and **Test-Path=False**. No directory was created on the busy RTX host.
The supplied baseline independently records removal of its original experiment
and unchanged installed worker/config/executable hashes.

Unproven: fresh physical NVENC execution of this branch; larger-resolution
footprints and complete encodes; concurrent cards/jobs and external session
pressure; arbitrary input formats, reference/GOP structures, long clips,
quality/speed parity; interactive tray/console and real coordinator publication.
The new budgets are bounded policies, not universal memory guarantees.

No push, merge, release, deploy, installation, startup change, production action,
Cloudflare operation or device/driver/machine-setting change was performed.
Only local branch commits, isolated native builds/tests and read-only RTX-host
investigation occurred in this task.
