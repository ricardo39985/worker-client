# Second-card recovery fix — October 17, 2026

Local branch: `fix/nvenc-card-retry`, created from clean `main` at `46ab545`.
This is a committed source fix with portable execution evidence. It is not an
installed Windows fix or proof that the RTX 3070 can currently encode.

The full operator brief was read. Its referenced companion investigation,
`/home/rick/.hermes/cache/scratch/two-cards-readonly-codex.md`, did not exist.
All host findings below were obtained independently using read-only SSH.
No remote FFmpeg/Doctor, build, installation, restart, device change or application
termination was performed. Production, Lightsail and Cloudflare were not accessed.

## Ranked findings and deciding evidence

1. **Confirmed worker defect: failed probes have no recovery path.** At base
   `46ab545`, `probe_ffmpeg` catches the first device/codec tuning error and
   withholds its profile. `Worker::network` calls it once. The watchdog only
   removes profiles; dispatch only reserves existing profiles. A recovered card
   therefore cannot rejoin during that controller lifetime. A worker restart
   starts a new probe, so this is a process-lifetime latch, not an on-disk ban.
   This is the confirmed cause of *permanent* exclusion after an initialization
   failure, and is fixed here.
2. **Confirmed recurring RTX 3070 encoder initialization failure; underlying
   trigger unresolved.** Saved H.264 logs report `CreateInputBuffer` allocation
   failure / out of memory (10), exit `4294967284`. HEVC reports
   `CreateBitstreamBuffer` allocation failure, exit `3221226356` (`0xc0000374`).
   Windows Application event 1000 at host-local **22:20:27**, just before the
   **22:20:30** probe result, records the FFmpeg crash in `ntdll.dll` with that
   exception. The same exception appears on earlier starts around 19:07–19:22.
   Microsoft identifies this code as [STATUS_HEAP_CORRUPTION](https://support.microsoft.com/en-gb/topic/heap-corruption-occurs-in-the-svchost-exe-process-in-windows-7-or-in-windows-server-2008-r2-1cb2d98b-0a06-3782-d3bd-151c47189bc9).
   This supports investigating the FFmpeg/NVENC initialization path, driver/context
   interaction, and contained-process constraints. It does **not** identify which
   component corrupted memory or prove a hardware defect. Calling the trigger
   merely a one-off transient would overstate the evidence.
3. **Application/session contention remains possible historically, but is not
   demonstrated now.** NVIDIA Broadcast, NVIDIA Overlay and Oculus processes
   were present. Both cards reported **zero active encoder sessions** and zero
   encoder utilization during the initial query and repeated session monitoring.
   The final 22:42:57 query still reported zero sessions. Thus a currently full
   session quota does not explain the continued exclusion. No telemetry was
   captured at the original failed initialization, so past contention cannot be
   ruled out. A transient FFmpeg process was seen at 22:33:45 and was absent from
   the later 22:34:42 process query; its ownership was not established.
4. **Ordinary system/VRAM exhaustion is weakly supported.** The 3070 reported
   8192 MiB total, about 1373 MiB used and **6645 MiB free** (6644 MiB in the final
   sample). The 3050 had **7672 MiB free**. Windows reported **43,355,832 KiB free
   physical memory**. This does not exclude NVENC allocation/API or per-child
   budget problems; available host RAM is not proof that every allocation works.
5. **Physical-card inventory loss is contradicted by the evidence.** Both PCI
   cards report healthy state and PnP error code 0 on driver **32.0.16.1742**
   (`nvidia-smi` **617.42**). Saved inventory contains the correct two separate
   PCI instances and verified backend mappings. `ROOT\DISPLAY\0000` is the
   Meta Virtual Monitor, deliberately excluded, as is Basic Render Driver.
   The 3050 retains both profiles (55.77 H.264 / 51.47 HEVC synthetic fps at
   1920x1080); the 3070 retains neither. PCI/ROOT policy was preserved.

Captured logs normalize trailing whitespace only.
Evidence files: `readonly-state.txt` (SSH exit 0), `readonly-driver.txt` and
`readonly-final.txt` (SSH exit 0). The driver/event read returned SSH exit 1 after
printing the evidence because the final Display-event query found no matching
entries; this is not an encode-test exit. `readonly-host.txt` contains the initial
query and follow-mode session samples, ending in SSH exit 255 when its local SSH
reader was disconnected. These are observations, not native execution gates.

## Changes and preserved guarantees

- Each eligible hardware tuning candidate gets at most **three attempts**, with
  **1s / 2s** cancellation-aware backoff. Each contained child drains before retry.
  Allocation/session pressure is reported as `memory/session unavailable` rather
  than permanent incompatibility; unknown failures remain `probe failed`.
- Failed device/codec samples remain pending. An independent recovery thread
  re-evaluates after **30 / 60 / 120 / 240 / 300 seconds**, capped at five minutes,
  without a permanent attempt limit. Busy/paused workers defer until idle/resumed.
- Recovery retains only successful **in-process** samples with current identity,
  mapping, driver and approved FFmpeg hash. It does not load proof from diagnostic
  JSON. Missing/failed profiles still require repeated encode, decode, strict
  codec/geometry/frame/audio validation and bounded tuning. CPU and healthy-card
  tuning is not repeated unnecessarily. No alternate unmeasured encoder options,
  automatic driver changes or dependency upgrades were introduced.
- Atomic admission maintenance excludes new offers during recovery and can begin
  only after all reserved/running/cancelling work drains. Operator pause cannot
  clear this gate, and ending recovery cannot clear operator pause. The network
  and watchdog continue on separate threads. A due retry waits for an idle window;
  this change does not launch an unreserved probe alongside a running job.
- Recovery stages diagnostics separately, checks completion/hash/identity again,
  rejects concurrently invalidated evidence, then swaps the scheduler only with
  no outstanding reservations. Existing PCI exclusions, per-card/shared NVENC
  session caps, VRAM/CPU/RAM/scratch limits, CPU fallback and lease rules remain.
- Runtime device/codec demotions also queue recovery. Status includes card, codec,
  withholding classification and retry countdown/idle deferral, wraps within 80
  columns and survives durable/live status routing. Full FFmpeg reasons remain
  in the diagnostic evidence and durable log. Recovery refreshes the displayed
  encoder evidence and schedulable-card count only after validated proof.

## Executed gates

All commands ran on the authoring Linux machine, using the existing reviewed
Boost 1.89.0 tree at `/tmp/ow-boost.mxOndo/boost_1_89_0`. No software was installed.

| Gate | Counts / result | Exit |
|---|---|---:|
| First behavior RED, before changing retry behavior | 71 contracts; 1 expected failure on first NVENC allocation error | 1 |
| Admission/recovery/status RED | 3/6 suites failed; 4 intended failing contracts | 8 |
| Classification/status-routing RED | 2/2 suites failed; 2 intended failures among 126 contracts | 8 |
| Retained-proof RED | 74 contracts; 1 intended failure | 1 |
| Final `scripts/test.sh -DOW_BOOST_ROOT=...` | configure/build and 6/6 suites passed | 0 |
| Final verbose portable CTest | 239 passed, 0 failed; 6/6 suites | 0 |
| ASan/UBSan configure / build | completed / completed | 0 / 0 |
| ASan/UBSan CTest, leak detection and halt-on-error enabled | 239 passed, 0 failed; 6/6 suites | 0 |
| Real Linux synthetic tuning-output validation | 2/2 H.264/HEVC outputs; 1920x1080, 72 frames, AAC and decode | 0 |
| `git diff --check` | no whitespace errors | 0 |
| Windows MSVC / native suites / physical two-card recovery | **UNRUN; 0 executed**; MSVC/Windows SDK absent here | no exit |

Final counts: dependency **23**, worker/journal/admission **77**, pipeline **9**,
rendition **3**, adaptive **74**, progress/status/console **53** = **239**.
All 231 pre-existing portable contracts are retained; eight were added.
The CMake sanitizer option instruments `ow_core` and its consumers: worker and
adaptive contracts (**151**). All six suites run in that build, but the Python
and standalone pipeline/rendition/progress suites are not instrumented by that
option. The 239-suite total is not a claim of 239 instrumented contracts.

Commands/logs: `portable-gate.txt`, `portable-suites.txt`,
`sanitize-{configure,build,suites}.txt`, `synthetic-smoke.txt`, and `red*.txt`.
An intermediate status-wrap assertion initially concatenated decorated columns;
it was corrected to reassemble the content columns across wrapped rows. That
intermediate failed run is retained in `green-final.txt`; it is not a gate pass.
Windows source review and Linux contracts do not replace MSVC/native execution.
No release readiness or currently usable second card is claimed.

## Exact authorization handoff

**No further remote changes were made.** Two actions need operator authorization:

1. **Clean up only our read-only monitoring process.** `nvidia-smi encodersessions`
   defaults to follow mode and survived disconnect. Final readback identified
   **PID 36304**, parent 17748, created **2026-10-17 22:32:10 host-local**, command
   `"C:\Windows\system32\nvidia-smi.exe" encodersessions`. Authorize terminating
   that exact reader (after rechecking its PID/start time/command), e.g.
   `Stop-Process -Id 36304`. It is not an encoder job. **PID 24992** is a different
   reader created at 22:29:27, predating this investigation; leave it alone.
   No remote termination command was sent because the instruction forbids
   anything beyond reading without authorization.
2. **Run an isolated synthetic NVENC proof on Danny's PC.** After the operator
   pauses new worker starts and existing work drains, authorize a normal-user
   temporary test directory and an isolated copy of the non-secret converter
   configuration, with an offline coordinator origin. Use the installed,
   hash-approved FFmpeg through the existing Doctor, with **no**
   `--check-coordinator` or `--bind-journal`:

   ```powershell
   & '<installed-version>\OrganizerWorkerDoctor.exe' --json --probe-ffmpeg --data-root "$env:TEMP\ow-second-card-proof"
   ```

   This writes synthetic inputs/outputs and diagnostics and creates NVENC
   sessions; it exceeds read-only access. Capture GPU 0/1 and H.264/HEVC outcomes,
   actual child limits and simultaneous `nvidia-smi` session/memory data. The
   installed worker's version/configuration/identity/journal/startup remain
   untouched. Resuming the operator pause is a separate explicit operator step.
   This determines whether the card can encode now and whether failure persists
   under the original options; any standalone/alternate-pixel-format A/B test or
   application shutdown requires its own explicit scope. No evidence presently
   justifies disabling Meta, killing Broadcast, changing drivers or rebooting.

Separately, build this branch with MSVC and run all registered native suites on
an operator-owned Windows development environment before considering installation.
Installing the resulting fix, replacing/restarting the live worker, or changing
any application/device is a later authorization, not covered by this source task.
No push, merge, release, deployment, installation, production or Cloudflare action
was performed.
