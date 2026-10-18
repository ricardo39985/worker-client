# Measured device decode continuation — October 18, 2026

Branch: `perf/measured-device-decode`; local commits only. This continuation uses
the operator's new authorization for generated test clips and encodes in one
removable PC directory. The earlier read-only-PC approval section in REPORT.md
is historical. The separate RTX 3070 host-memory-cap correction was not touched.

## Scope and measurement method

- Actual PC: i7-12700KF, 20 logical CPUs; RTX 3050 8 GiB, CUDA ordinal 1,
  PCI bus 05:00.0, NVIDIA driver 617.42. No RTX 3070 encode was attempted.
- Installed approved FFmpeg n9.0.2-22-g46d8f462ee, SHA-256
  `f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`.
  Existing Boost 1.89.0 and MSVC 19.44 were reused, without installation.
- Generated H.264 SDR yuv420p testsrc2 clips at 1920x1080 and 3840x2160,
  exactly 900 frames / 30 seconds / 30 fps, with AAC audio. Sources are synthetic;
  these numbers are not representative-camera-media or production-job claims.
- Identical NVENC p4/VBR/CQ24 settings and full dimensions before/after;
  AAC is re-encoded at the existing 160 kbit/s setting. No video scaler.
- Each measured child uses the pre-existing baseline tool's 2048 MiB fixture,
  one configured CPU and 100 ms polling. Normal priority (the runtime default)
  permits 5% of this machine's CPU; Low permits 2.5%. Both are measured separately.
  This is an offline measurement fixture, not a change to worker admission or
  the separately owned host-memory-cap policy. All current runtime guards remain.
- The offline stage deadline is fifteen minutes so that validation cost can be
  observed. The worker's existing thirty-second output-validation deadline is
  unchanged. At Low, the 900-frame 4K samples exceed it; an offline timing pass
  must not be mistaken for an accepted runtime job under that budget.
- Three repetitions per complete job/variant/resolution; report medians and
  observed ranges. Filesystem caches were not flushed and external workload was
  not controlled. No device/driver/OS/service setting was changed.
- Complete offline job totals include input inspection, source/output hashing,
  both encodes, both full software validations, and the poster. Source staging,
  quality-analysis work and builds are outside job time. Network download/upload
  and coordinator latency are **unmeasured**, not zero; no real account was used.
- Child CPU is Job Object user+kernel CPU after the whole tree drains. Controller
  CPU is the main thread plus timed background stages, with the scope named in
  records. Wall time includes polling/drain. Launch time and root lifetime overlap
  wall time and must not be added to it. FFmpeg internal startup is not isolated.

## Retained implementation

Conservative input inspection selects CUDA/NVDEC only for known H.264
Baseline/Main/High or HEVC Main, SDR 8-bit 4:2:0. CUDA frames feed the already
selected NVENC encoder on its mapped device. There is no hwdownload, host nv12
format filter, encoder-setting change, or full-size device scaler. Other profiles,
unknown input, HDR, interlace and other backends retain software decode. Runtime
hardware failure follows the existing full-validation/CPU-retry policy; NVENC
retry/demotion/withholding and all resource caps are unchanged.

The first software frame needed for guarded geometry can also produce the fixed
poster. Its scale, quality and encoder options come from the canonical poster
plan. Reuse requires one ordinary unrotated SDR video stream; otherwise the
candidate poster is discarded and the normal poster path runs. Rotated sources
retain the original poster autorotation behavior. A prepared poster enters the
existing delivery pipeline first, preserving one-artifact backpressure and drain
ownership. The H.264 and HEVC source decodes themselves remain separate.

Native testing also corrected the earlier timing hook: Job Object accounting can
reach zero before a forcibly terminated root handle is signalled. Stop now waits
for that handle before sampling its exit timestamps, and live sampling does not
read undefined exit time. Resource ownership still lasts until process-tree drain.
The converter test fixture uses shorter private directory names so its synthetic
.part paths fit legacy PowerShell MAX_PATH inside the authorized deep temp root;
no host long-path setting was changed.

The resident-frame command follows [NVIDIA's FFmpeg guidance](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.1/ffmpeg-with-nvidia-gpu/).
Native debug graphs independently verify the actual pinned executable's CUDA
input format, with no download or automatic host conversion. This is application
graph evidence, not a measured PCIe byte counter; compressed input/output and
software validation still use the host.

## Complete offline jobs

Each job produces **both** H.264 and HEVC plus the poster. Seconds; median of three runs. Stage medians need not sum to the median total.

| Priority / size | Wall before → after | CPU before → after | Actual multiplier | Wall ranges before / after |
|---|---:|---:|---:|---|
| Normal / 1080p | 17.794 → 15.841 | 17.844 → 13.672 | **1.123×** | 17.457–17.899 / 15.663–15.946 |
| Normal / 4k | 55.631 → 53.137 | 56.031 → 38.500 | **1.047×** | 55.525–56.069 / 52.683–53.387 |
| Low / 1080p | 32.387 → 25.262 | 17.891 → 13.375 | **1.282×** | 31.905–33.308 / 25.066–25.384 |
| Low / 4k | 108.367 → 86.674 | 55.906 → 39.422 | **1.250×** | 105.797–109.610 / 85.444–86.747 |

Normal is the source default; a saved operator preference can select another mode. The Low result must not be advertised as the default-job multiplier. Normal 4K wall time improves only about 4.5%, despite a 31.3% CPU-time reduction.

## Per-output stages

Wall and CPU seconds, before → after. The subtotal multiplier covers that output’s transcode plus full validation, with no invented allocation of shared inspection/poster/hash costs.

| Priority / size / output | Transcode wall | Transcode CPU | Validation wall | Validation CPU | Subtotal multiplier |
|---|---:|---:|---:|---:|---:|
| Normal / 1080p / H.264 | 3.542 → 2.470 | 3.578 → 1.547 | 4.504 → 4.518 | 4.469 → 4.484 | 1.152× |
| Normal / 1080p / HEVC | 3.551 → 2.682 | 3.766 → 1.531 | 5.789 → 5.790 | 5.656 → 5.781 | 1.102× |
| Normal / 4k / H.264 | 10.204 → 8.687 | 10.078 → 1.844 | 17.154 → 17.140 | 17.250 → 16.938 | 1.059× |
| Normal / 4k / HEVC | 10.202 → 9.111 | 10.297 → 1.750 | 17.046 → 17.233 | 17.000 → 17.203 | 1.034× |
| Low / 1080p / H.264 | 6.643 → 2.685 | 3.609 → 1.438 | 8.351 → 8.563 | 4.422 → 4.453 | 1.333× |
| Low / 1080p / HEVC | 6.334 → 2.792 | 3.734 → 1.484 | 10.831 → 10.931 | 5.688 → 5.625 | 1.251× |
| Low / 4k / H.264 | 20.272 → 8.697 | 10.500 → 1.938 | 33.359 → 33.681 | 17.031 → 17.266 | 1.266× |
| Low / 4k / HEVC | 20.049 → 9.116 | 10.609 → 1.875 | 32.900 → 34.211 | 16.875 → 17.391 | 1.222× |

## Inspection, poster, hashing and startup

Other stage medians below are wall / CPU seconds. Afterward the poster is included in inspection; it is not a free zero-cost output. Output hashing sums the three artifact stage medians.

| Priority / size | Input hash before → after | Inspection before → inspection+poster after | Separate poster before | Output hashes before → after |
|---|---|---|---|---|
| Normal / 1080p | 0.091/0.094 → 0.091/0.094 | 0.113/0.094 → 0.118/0.141 | 0.107/0.078 | 0.087/0.062 → 0.087/0.078 |
| Normal / 4k | 0.354/0.359 → 0.369/0.375 | 0.220/0.188 → 0.223/0.250 | 0.107/0.094 | 0.262/0.266 → 0.275/0.266 |
| Low / 1080p | 0.093/0.094 → 0.092/0.094 | 0.116/0.078 → 0.107/0.141 | 0.107/0.094 | 0.087/0.094 → 0.089/0.078 |
| Low / 4k | 0.353/0.359 → 0.357/0.359 | 0.222/0.203 → 0.227/0.250 | 0.107/0.141 | 0.263/0.266 → 0.263/0.266 |

The separate seven-pair Low test isolates the reuse step:

| Size | Inspection + separate poster wall → fused wall | CPU before → after | Preparation multiplier |
|---|---:|---:|---:|
| 1080p | 0.215034 → 0.107781 | 0.156250 → 0.125000 | 1.995× |
| 4k | 0.320788 → 0.214180 | 0.312500 → 0.265625 | 1.498× |

This saves about 107 ms per job, not a large whole-job multiplier. All fourteen paired posters are byte-identical. Before has six contained child launches per job; after has five.

| Priority / size | Median sum of child launch ms, before → after |
|---|---:|
| Normal / 1080p | 28.469 → 23.952 |
| Normal / 4k | 29.407 → 23.264 |
| Low / 1080p | 27.789 → 24.084 |
| Low / 4k | 29.876 → 26.426 |

Launch covers process creation/containment/resume, not internal FFmpeg startup. Per-stage root lifetime and poll/drain gaps are retained in results.json. Whole-worker startup and network transfers were not measured; no speedup is assigned to either.

## Quality and device-frame proof

| Output / resolution | SSIM against current output | PSNR | Frames / duration / dimensions |
|---|---:|---:|---|
| H.264 / 1080p | 1.000000 | ∞ | 900 / 30 s / unchanged |
| HEVC / 1080p | 1.000000 | ∞ | 900 / 30 s / unchanged |
| H.264 / 4k | 1.000000 | ∞ | 900 / 30 s / unchanged |
| HEVC / 4k | 1.000000 | ∞ | 900 / 30 s / unchanged |

All original baseline/device/shared repetitions pass frame-count, microsecond timing and geometry parity; every measured complete-job artifact also matches the original baseline SHA-256. Audio and video bytes remain unchanged. Actual SSIM/PSNR filters ran for both output codecs and resolutions. Repeated byte-identical files reuse that proved reference for expensive extra quality analysis; **each output still underwent full software validation inside its timed job**.

Throughput inputs are generated H.264. Separate 72-frame, video-only comparisons cover H.264 and HEVC inputs × H.264 and HEVC outputs × both resolutions: all eight cases have identical encoded bytes, SSIM 1.000000 and PSNR ∞. Full-duration HEVC-input throughput and representative personal/camera footage are unmeasured.

Eight native debug graphs confirm pixfmt:cuda with no hwdownload or auto_scale host conversion. The production factory arguments equal the experimentally measured CUDA commands. CUDA decoding and NVENC use the same verified ordinal/card. Compressed bitstreams still cross the host/device boundary; this is not a claim of zero total PCIe traffic. Raw graph and metric logs are in native-diagnostics/.

## What was not retained, and what did not improve

| Low single-job experiment | Separate CUDA decodes wall / CPU | Shared decode wall / CPU | Observed wall multiplier |
|---|---:|---:|---:|
| 1080p | 24.918 / 13.250 | 24.392 / 13.094 | 1.022× |
| 4k | 85.434 / 38.547 | 83.595 / 37.516 | 1.022× |

The full two-output shared plan is **not in the worker runtime**. It opens two NVENC sessions while current admission reserves one session per physical card, the maximum sequential codec footprint, and two worker NVENC sessions globally. This single-card experiment does not establish safe concurrent admission or two-card throughput. Its roughly 2% extra single-job gain does not justify bypassing that contract. The narrow inspection/poster reuse is retained instead. No speculative scaler, encoder tuning or weakened validation was kept; no other runtime optimization needed reversion.

Full software validation did not improve: it decodes the same bytes with the same CPU cap. It dominates remaining wall time (roughly 19 s per 1080p Low job and 67 s per 4K Low job). Read-only GPU samples reached encoder 100% and decoder 83%, consistent with encoder work limiting further decode-side gains; no physical engine trace or PCIe byte measurement was taken. Normal 4K is only 1.047× faster overall, not the roughly 2× Low transcode-stage improvement.

**Runtime limit:** Low 4K validation takes about 33 s per output and exceeds the unchanged 30 s worker validation deadline. Normal measurements are about 17 s per output and fit it. The offline tool deliberately permits longer stages to expose this cost. These are offline computation results, not evidence of installed worker admission, a coordinator job, upload/publication, or resource-budget integration. Existing runtime caps/guards remain binding; the separate host-memory-cap correction stays with its owner.

## Gates and adverse results

| Gate | Final result | Exit |
|---|---|---:|
| Strict portable rendition contracts | 10 passed | 0 |
| Linux Release CTest | 6 suites, 245 cases | 0 |
| Sanitizer-configured CTest | 6 suites, 245 cases; leak detection enabled | 0 |
| Explicit ASan/UBSan rendition binary | 10 contracts, leak detection enabled | 0 |
| MSVC 19.44 Release build | All registered binaries compiled | 0 |
| Targeted corrected native gates | 13 Windows + 17 converter contracts passed | 0 |
| Final Windows CTest | 10 suites, 315 cases | 0 |
| Initial quality / poster / negotiation modes | All completed after harness correction | 0 each |
| H.264/HEVC source parity mode | Eight source/output/size cases | 0 |
| Complete Low and Normal paired measurements | Three repetitions per variant/size; all artifact hashes match | 0 each |

Windows counts: dependency 23, core 75, pipeline 9, rendition 10, adaptive 70, progress/status/timing 58, Windows 13, setup 25, setup progress 15, converter 17. Linux omits the four Windows-only suites. The rendition target is separately compiled with explicit sanitizer flags rather than assuming every CMake target inherits them.

Behavior RED evidence is retained in 13-, 19-, 25-, 27-, 30- and 46- logs. Initial native configuration failed from a script macro substitution (10-); first new-source native compilation rejected a nonportable regex flag (33-), fixed using line parsing. Initial frame parity saw 884 visible rows because progress carriage returns prefixed 16 of the actual 900 rows (12-, 32-), fixed with -nostats. An owned quality process was deliberately stopped/drained (38-) before avoiding redundant repeated reference decodes; its partial log is not a final pass. First complete native CTest failed the forced-termination timestamp race and deep PowerShell fixture paths (39-, exit 8); both were corrected, without changing host settings, and the targeted and full final gates pass (51-). Known MSB8029 temporary-build warnings and the inherited cancelled-variable shadow warning remain; neither is a build failure.

Final logs: 48-/49- portable, 52-/53- sanitizer configured, 50- explicit rendition sanitizer, 51- final MSVC/source parity/Normal/native gates. 12- and 39- retain the earlier Low experiments and complete Low jobs. tested-source.json verifies all eight changed code/test/build files against the final native-compiled archive.

## Cleanup and remaining human checks

The sole PC directory `C:\Users\danny\AppData\Local\Temp\ow-gpu-measured-20261018-a72c` was recursively deleted after copying only 17 text/XML evidence files. A fresh SSH connection independently returned `FRESH_CONNECTION_ROOT_EXISTS=False` (54-cleanup.log). Generated clips, output media, staged sources, archives, builds and test/module caches in that directory are gone. No test media is retained locally or committed. Representative playback/appearance, user-session responsiveness and an actual admitted job after the separately owned budget correction remain operator checks. No installation, live worker restart, startup registration, device/driver/service setting change, push, merge, release, deployment, production or Cloudflare action was performed.
