# Finish measured decode and completion reporting — October 18, 2026

Application/test revision: `f56952e` after merging current main `29b76c8` in
`314baf9`. The subsequent concurrent `4604a58` commit adds historical evidence
only and changes no code or tests. Native source was archived from `0771b79`, then updated with the three changed
reporting files from `f56952e` and rebuilt.

The main integration keeps workload-sized child RAM, atomic resizing, actual
commit accounting, retained in-process probe evidence, idle retry, physical
adapter mapping, per-card and global sessions, and reserved CPU fallback. Timing
and validation share those same limits. Neither the reviewed dependency lock nor
runtime validation deadlines changed.

The completion display no longer overwrites the drained encoder's placement
with CPU. Start/completion reporting and the following stats use the same
physical card, until the next actual encoder replaces it. CPU execution and CPU
retry still report CPU. Three new behavioral contracts cover both H.264/HEVC GPU
files and CPU retry; intentional RED build exit 2 precedes GREEN exit 0. This
changes reporting only, with no scheduler or encoder-selection changes.

## Local gates after integration

Release and ASan/UBSan: six suites each, 263 contracts each, all exits 0.
Configure/build/discovery and every suite exit are recorded in
[linux-results.json](linux-results.json). Sanitizers instrument every target via
explicit global flags, with leak detection and halt on error enabled. Real
full-size H.264/HEVC/WebP rendition smoke and real 1080p adaptive tuning smoke
both exited 0; logs 05/06 preserve the output.

Counts: dependency 23, core 79, pipeline 9, rendition 10, adaptive 78,
progress/status/timing 64. Expected RED is not a release gate failure.

The third regression preserves the actual child adapter name even if periodic
display inventory becomes unavailable. Stats use the same retained name.

## Native gates after integration

On `reald@192.168.1.10`, existing MSVC 19.44 and reviewed installed Boost 1.89.0:
configure 0, build 0, discovery 0, every one of the nine registered native suites
0; [numeric ledger](windows-results.json), full [build log](04-native-gate.log)
and final [build and suite log](18-final-native.log). Windows includes the three new card-reporting
contracts and process-tree/commit accounting tests (311 total cases). No test failed or was edited
while running these gates.

The initial runner incorrectly required ten suites and exited 1 before running
them. Diagnosis found that the tenth historical Danny suite was the optional
Python dependency suite; Rick's CMake found no Python interpreter. The runner
now checks the nine required native suite names and executes every discovered
suite. No test was dropped, skipped or weakened. The 23 Python dependency cases
passed in both Linux configurations. No interpreter/software was installed and
no machine PATH or policy was changed. One measurement process launched before
that discovery check was deliberately stopped/drained and its single directory
removed (09 log). The same authorized path was recreated for fresh measurements.

All 86 application/test/script/lock paths in the native source archive equal the
current tree, allowing only declared PowerShell/cmd CRLF/LF normalization;
[tested-source.json](tested-source.json) records both hashes and archive SHA.
The final application revision is `f56952e`; later commits contain evidence only.

## Fresh paired performance after merging main

The retained CUDA decode and inspected-frame poster reuse both improve measured
work. No additional runtime optimization was introduced or reverted here. The
full two-output shared-decode experiment stays excluded: its earlier roughly
1.022x single-job gain does not justify two simultaneous sessions when admission
reserves one sequential session per physical card. Nothing bypasses that budget.

Fresh measurements used the merged `0771b79` native measurement binary on Danny's
mapped RTX 3050, CUDA ordinal 1. Final `f56952e` changes reporting only; the media
factory, timing, process/resource code and measurement tool are unchanged.
Generated H.264 SDR 900-frame / 30-second inputs at 30 fps, AAC, 1080p and 4K;
NVENC p4/VBR/CQ24 and full dimensions before/after. Three repetitions, medians.
Normal priority, 1 configured CPU (5% on the 20-logical-CPU host), 2048 MiB offline
fixture, 100 ms poll. This fixture does not replace runtime workload-sized RAM
reservations. Full software output validation is included. Source generation and
quality analysis are outside job totals; download/upload/coordinator latency is
unmeasured. Caches were not flushed and external load was not controlled.

Every complete job produces H.264, HEVC and the poster. Wall/CPU seconds include
all timed computation, input/output hashing, inspection and poster. Stage medians
need not sum to the median total. CPU is controller plus drained child-tree CPU.

| Size | Job wall before -> after (s) | Job CPU before -> after (s) | Actual wall multiplier | CPU multiplier |
|---|---:|---:|---:|---:|
| 1080p | 17.358 -> 15.718 | 17.906 -> 13.500 | 1.104x | 1.326x |
| 4k | 55.297 -> 52.703 | 56.344 -> 38.750 | 1.049x | 1.454x |

| Size / stage | Wall before -> after (s) | CPU before -> after (s) | Wall multiplier |
|---|---:|---:|---:|
| 1080p / input SHA-256 | 0.091 -> 0.091 | 0.094 -> 0.094 | 1.003x |
| 1080p / inspection + poster | 0.227 -> 0.107 | 0.109 -> 0.094 | 2.128x |
| 1080p / H.264 transcode | 3.335 -> 2.474 | 3.656 -> 1.531 | 1.348x |
| 1080p / H.264 validation | 4.499 -> 4.503 | 4.484 -> 4.438 | 0.999x |
| 1080p / H.264 compute subtotal | 7.834 -> 6.977 | 8.141 -> 5.969 | 1.123x |
| 1080p / HEVC transcode | 3.333 -> 2.684 | 3.641 -> 1.547 | 1.242x |
| 1080p / HEVC validation | 5.802 -> 5.774 | 5.703 -> 5.719 | 1.005x |
| 1080p / HEVC compute subtotal | 9.135 -> 8.458 | 9.344 -> 7.266 | 1.080x |
| 1080p / all output SHA-256 | 0.087 -> 0.087 | 0.094 -> 0.094 | 1.000x |
| 4k / input SHA-256 | 0.353 -> 0.351 | 0.359 -> 0.359 | 1.003x |
| 4k / inspection + poster | 0.333 -> 0.224 | 0.344 -> 0.266 | 1.482x |
| 4k / H.264 transcode | 10.089 -> 8.683 | 10.688 -> 2.141 | 1.162x |
| 4k / H.264 validation | 17.012 -> 17.015 | 16.938 -> 16.969 | 1.000x |
| 4k / H.264 compute subtotal | 27.101 -> 25.699 | 27.625 -> 19.109 | 1.055x |
| 4k / HEVC transcode | 10.197 -> 9.114 | 10.750 -> 1.906 | 1.119x |
| 4k / HEVC validation | 17.109 -> 17.046 | 17.000 -> 16.953 | 1.004x |
| 4k / HEVC compute subtotal | 27.306 -> 26.160 | 27.750 -> 18.859 | 1.044x |
| 4k / all output SHA-256 | 0.265 -> 0.258 | 0.266 -> 0.266 | 1.025x |

Validation and hashing have no optimization applied; small differences there are
run-to-run variation, not a claimed gain. Both full software validations remain
mandatory. The offline tool permits fifteen minutes to measure cost; the runtime
30-second validation guard is unchanged. Earlier Low-priority 4K checks exceed
that guard and are not accepted-runtime claims. Historical Low results, separate
seven-pair poster measurements and source-profile parity are in
[MEASURED.md](../gpu-performance/MEASURED.md).

SSIM and PSNR filters actually ran against the before output for both codecs at
both sizes. All four report **SSIM 1.000000, PSNR infinity**; every measured
complete-job artifact also matches its original reference SHA-256 (36 checks).
All 900 frames, dimensions, timing and audio bytes therefore remain identical.
Throughput inputs are generated H.264; earlier eight short H.264/HEVC input/output
parity cases do not establish full-duration HEVC-input throughput. These are
synthetic offline computation results, not a coordinator or network-job speedup.

Raw [measurement and quality log](13-measurements.log), [stage data and ranges](fresh-results.json).

## Landing and installation

Pending in this running ledger. Do not infer success from prepared scripts.
