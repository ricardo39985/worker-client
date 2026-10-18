> Historical timing-only gate. The operator subsequently authorized a single removable PC directory and generated-clip encodes. Current implementation, measured results, native gates and verified cleanup are in [MEASURED.md](MEASURED.md); the approval-pending sections below no longer describe current authorization.

# Measured GPU optimization: stage 1 authoring gate

Branch: `perf/measured-device-decode`, based on local `main` at `46ab545`.
The operator brief `/home/rick/.hermes/cache/scratch/gpu-perf-take-gains-brief.md`
was read in full. The resident/device-frame idea is independent of the RTX 3070
failure investigation. Modest demonstrated gains are acceptable; no gain is
claimed here. This is an incomplete, locally committed instrumentation candidate.

## Implemented first: observability without changing media commands

Each app attempt records input inspection, download, input/output hashing,
transcode per artifact/attempt (including failed hardware work and CPU retry),
full software validation, poster, upload, and total job wall time. Startup probe
children and the whole startup probe have separate timing records.

`wall_us` includes controller polling and drain. `launch_us` covers process
creation, containment and resume; it does **not** isolate FFmpeg's internal
demux/codec startup. `root_lifetime_us` comes from Windows process creation/exit
timestamps, so the difference from observed wall is visible. These intervals
overlap: never add launch and root lifetime to stage wall time.

`controller_cpu_us` is the calling thread's kernel+user CPU. `child_cpu_us` is
Windows Job Object kernel+user CPU for the whole tree, retained after drain.
Unavailable measurements are explicit, never fabricated zeros. The job summary
reports CPU for the main attempt thread, timed background stages, and child
trees; process-wide worker CPU would contaminate concurrent jobs and is not used.
Background orchestration outside timed stages is excluded and the scope is named
in the record. Stage wall sums may exceed job wall because delivery overlaps
encoding; the summary explicitly identifies this. Failed work still emits timing.
Timing observer failures do not alter conversion outcomes or pause admission.

Encoder options, decode choice, geometry, deadlines, CPU/VRAM admission, full
software validation, NVENC retry and withholding policy are unchanged. Only
timing hooks surround the existing validator. No device scaler was added.

## Executed local evidence

| Gate | Result | Exit |
|---|---|---|
| First failing timing contract compile | Missing required timing API/header | 1, expected RED |
| Focused strict GCC timing contracts | 7 passed, 0 failed | 0 |
| `./scripts/test.sh` | 6 suites, 238 contracts/cases, 0 failed | 0 |
| Verbose portable CTest | 23 dependency, 75 core, 9 pipeline, 3 rendition, 70 adaptive, 58 progress/status/timing | 0 |
| ASan/UBSan configure, build | Both succeeded | 0 each |
| ASan/UBSan verbose CTest, leak detection enabled | 6 suites, 238 contracts/cases, 0 failed | 0 |
| Installed Linux FFmpeg synthetic rendition smoke | Full-size H.264, HEVC, WebP decoded | 0 |
| `git diff --check` | Clean | 0 |

Exact logs are adjacent, `01-timing-red.log` through `07-sanitize-suites.log`.
The native retention test is authored but unrun. Windows changes and the offline
baseline executable have **not** compiled with MSVC. Linux tests do not establish
Windows accounting, command execution, quality parity, or GPU performance.

## Required performance and quality ledger

| Output / source size | Before: transcode / validation / poster / wall / CPU | After | Multiplier | SSIM / PSNR |
|---|---|---|---|---|
| H.264 / 1920x1080 | Not measured | Not measured | Not measured | Not measured |
| HEVC / 1920x1080 | Not measured | Not measured | Not measured | Not measured |
| H.264 / 3840x2160 | Not measured | Not measured | Not measured | Not measured |
| HEVC / 3840x2160 | Not measured | Not measured | Not measured | Not measured |

There is no achieved speedup or quality number yet. Timing is observability, not
a throughput optimization. Double source decode and full software output decode
remain. Encoder session limits, decoder saturation, CPU cap and validation cost
cannot be assigned measured blame without the experiment. Nothing was reverted:
no optimization step has yet been implemented or accepted on speculative gains.

The current command factory establishes software decode/host-frame input to
NVENC; that is command-path evidence, **not** a measured PCIe transfer count.
Physical transfer trace/counters, NVDEC device-frame negotiation with no download,
real-job transfer overlap and actual speedup remain open.

## Native baseline tool prepared for review, not executed

`ow_gpu_baseline` accepts the approved FFmpeg path/hash, two operator-approved
real-source files (1920x1080 and 3840x2160, SDR 8-bit 4:2:0, at least 900 frames),
and a new absolute workspace. It uses only the physically mapped RTX 3050 and
refuses ambiguity; it does not probe or encode on the RTX 3070.

It repeats today's software-decode/NVENC H.264+HEVC+poster commands three times
per source, serially, with one CPU at low priority and a 2048 MiB child memory
limit. Each child is contained, polled at 100 ms and has a ten-minute bound.
Full software output validation remains inside timed job work. Frame count,
dimensions and frame timing (microsecond timebase) are compared with the source.
SSIM and PSNR against run 0 are printed with their actual numbers, followed by
a byte-identity check for timing-only repetitions. Expensive source/frame/quality
checks run outside job timings. Outputs and logs stay in the new workspace.

This is offline serial compute evidence, not a scheduler/admission qualification,
not a comparison against an uninstrumented controller build, and not a network
upload measurement. The original media commands are unchanged; native parity and
instrumentation overhead must still be established before step 2. An example
invocation, requiring resolved approved source paths and separate encode approval:

```powershell
& 'C:\Users\danny\ow-gpu-stage1-20261017\build-native\Release\ow_gpu_baseline.exe' `
  'C:\Users\danny\ffmpeg.exe' $Approved1080Source $Approved4KSource `
  'C:\Users\danny\ow-gpu-stage1-baseline-20261017' `
  'f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4'
```

The variables above are unresolved; this is **not** a request to run an encode
with unspecified inputs. The next authorized work after native and baseline
gates is conservative CUDA/NVDEC device-frame input to the existing encoder,
with unsupported sources/devices using software decode. Retain only measured
improvements with output parity. Shared decode follows that measurement and
must respect the existing per-card session/resource contract. Full software
validation stays; report its measured share separately.

## PC authorization boundary

Read-only SSH confirmed the RTX 3050 and RTX 3070 PCI instances, an active worker,
the installed approved FFmpeg path/hash, existing reviewed Boost and MSVC CMake.
No encode, build, file write, installation, process stop, startup/config change,
production contact, push, merge, deployment or Cloudflare action occurred.
Do not infer the installed GPUs' functional state from inventory.

The first proposed PC write is only source staging, MSVC compilation and native
CTest, without running FFmpeg, Doctor, smoke, or the baseline executable:

```sh
scp /tmp/ow-gpu-stage1-source.zip danny@192.168.1.189:ow-gpu-stage1-source.zip
scp verification/gpu-performance/ow-gpu-stage1-native.ps1 danny@192.168.1.189:ow-gpu-stage1-native.ps1
ssh danny@192.168.1.189 'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\danny\ow-gpu-stage1-native.ps1'
```

Both scripts are reviewable beside this report. They read the existing Boost/toolchain
and writes only the new source/build folder and native test temporary workspaces.
Copy native logs back locally, then clean up only the new source archive and
tree with the following PowerShell commands (test-owned temp directories have
their existing scoped cleanup):

```powershell
Remove-Item -LiteralPath 'C:\Users\danny\ow-gpu-stage1-20261017' -Recurse -Force
Remove-Item -LiteralPath 'C:\Users\danny\ow-gpu-stage1-source.zip' -Force
Remove-Item -LiteralPath 'C:\Users\danny\ow-gpu-stage1-native.ps1' -Force
```

Approval is required by the user's explicit read-only SSH rule, not an inferred
release policy. Real-source paths and approval of the final resolved encode
command/cleanup are separate prerequisites before any PC encode. Human-only
acceptance still includes user-session responsiveness and representative playback;
neither can be established by these portable tests.
