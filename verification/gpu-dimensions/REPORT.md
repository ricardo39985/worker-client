# GPU dimension correction — 17 October 2026

Branch: `feat/gpu-on-logs`, starting at `80ba51d`. Source and local Linux evidence
only. No branch switch, push, install, deployment, restart, Windows connection or
production access occurred. The operator owns Windows build and installation.

## Mechanism

The old tuning result labelled its 640x360 sample dimensions `max_width` and
`max_height`. Profile selection copied those fields directly; job selection then
interpreted a successful tiny measurement as the encoder's maximum capability.
That rejected 1080p before the existing guarded hardware/CPU path could execute.
The throughput comparison also used the tiny sample to reject hardware globally.

Measurement size is now `measured_width`/`measured_height`. Encoder admission
ceilings are independent: NVENC and QSV 4096x4096; AMF 3840x2160; CPU has no GPU
ceiling. The approved subprocess interface does not expose a uniform per-device
maximum-dimension query. Code therefore documents conservative backend ceilings,
labels them `conservative_backend_ceiling`, and never treats them as proof of
all geometries within. Even dimensions are required by the fixed yuv420p GPU
profile. Unsupported/unknown geometries use CPU without scaling.

These fallbacks deliberately avoid asserting newer 8K support from an encoder
name. Native SDKs have hardware-specific query facilities: [NVENC checks width
and height caps](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/nvenc.c),
[AMF documents hardware-specific dimensions and AMFCaps](https://github.com/GPUOpen-LibrariesAndSDKs/AMF/blob/master/amf/doc/AMF_Video_Encode_API.md),
and [Intel VPL requires querying specific resolutions](https://intel.github.io/libvpl/latest/API_ref/VPL_structs_memory.html).
The policy ceilings here are conservative implementation choices, not measured
SDK results from this machine.

Repeated startup encode/decode tuning now uses 1920x1080, 24 fps, 72 frames and
AAC. Codec, geometry, complete frame count, container, audio and successful decode
remain mandatory. A two-minute bound replaces the 30-second encode deadline for
these larger tuning/concurrency encodes; cancellation and resource limits stay
active. Only equal measurements at least 1920x1080 can reject hardware for the
5 percent throughput floor. Smaller/mismatched samples cannot do that. Profiles,
all successful candidate measurements (including withheld slower ones), and CPU
concurrency records save their actual measurement resolution. This is one 1080p
synthetic workload, not a 4K performance or quality-parity claim.

Within the backend ceiling, larger-than-sampled jobs run at their actual size
through guarded hardware execution. All hardware results must decode and match
the expected codec and input dimensions before upload. Encode, decode, geometry
or runtime resource failure discards partial output and retries CPU after the
contained child tree drains. The device/codec is removed atomically from future
scheduler selection once per probe epoch. Reserved work checks the current
profile before hardware launch; already running work retains its resources until
drain. Other codecs/devices survive. Diagnostic state records the removal and
original profile under `runtime_demotions`. Restart obtains fresh probe evidence;
the diagnostic file is not loaded as executable capability proof.

CPU fallback retains the existing bounded, cancellation-aware slot wait. This now
also applies when geometry selected CPU before hardware launch. Poster/image work
does not acquire unrelated video fallback slots. Protocol, offers, BEGIN/lease,
publication fencing, admission budgets, outbox and process containment are retained.

## Behavior evidence

The first actual regression run failed because a successfully sampled 640x360
profile could not admit 1080p: 47/48 adaptive contracts passed, one failed.
New contracts cover all three GPU backends, genuine ceiling/odd-axis rejection,
unknown dimensions, separate measurement scope, realistic/equal comparisons,
saved resolution, strict output geometry, atomic codec-specific demotion,
already-reserved capacity, clean CPU retry after either encode failure or failed
validation, and readable console reasons. Five adaptive and two console contracts
were added: 163 became 170 total contracts.

Some intermediate failures were test construction issues, not product defects:
log 09 used a signed JSON accessor for unsigned dimensions, corrected to unsigned;
log 14 tried admitting a second codec while its device's sole reserved VRAM slot
was still occupied. The corrected test first verifies ownership, then drains
before checking the surviving codec. Compile-red stages are not executed behavior
failures. Logs 07 and 12 ran the newly rebuilt progress target despite the
independent adaptive target failing compilation; they are not stale binaries.

## Gate ledger

All commands ran in this repository with already-installed tools. CMake retained
`OW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0`, the existing reviewed 1.89.0
source tree. No dependency download, installation, lock update or PATH change.
Each numbered filename is under this report's directory. Compiler-log trailing
whitespace was trimmed after gate 34; diagnostic text and results are unchanged. Exit values below are
those of the gate command itself, separately captured before shell reporting.

| Log | Command | Exit | Counts/result |
|---|---|---:|---|
| 01 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 02 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | 47/48 pass; intended geometry regression |
| 03 | `cmake --build build-tests --parallel 2` | 2 | measurement API rename; no tests executed |
| 04 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 05 | `build-tests/ow_adaptive_hardware_contract_tests` | 0 | 51/51 pass |
| 06 | `cmake --build build-tests --parallel 2` | 2 | evidence API absent; no tests executed |
| 07 | `build-tests/ow_progress_contract_tests` | 1 | 28/29 pass; intended demotion wording regression |
| 08 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 09 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | 51/52 pass; unsigned JSON accessor corrected |
| 10 | `build-tests/ow_progress_contract_tests` | 0 | 29/29 pass |
| 11 | `cmake --build build-tests --parallel 2` | 2 | geometry validation API absent; no tests executed |
| 12 | `build-tests/ow_progress_contract_tests` | 1 | 29/30 pass; intended fallback warning-color regression |
| 13 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 14 | `ctest --test-dir build-tests -V` | 8 | 4/5 suites; 169/170 contracts pass; occupied-slot test corrected |
| 15 | `./scripts/test.sh` | 0 | 5/5 suites pass; 170 contracts |
| 16 | `./scripts/test.sh` | 0 | 5/5 suites pass; 170 contracts; smoke helper built |
| 17 | `python3 tests/adaptive_probe_smoke.py --evidence-probe build-tests/ow_adaptive_evidence_probe --rendition-probe build-tests/ow_rendition_probe` | 0 | 2/2 real 1080p CPU outputs encoded/decoded and strictly validated |
| 18 | `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 0 | configure only |
| 19 | `cmake --build build-sanitize --parallel 2` | 0 | build only |
| 20 | `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o /tmp/ow-gpu-dimensions-progress` | 0 | strict instrumented build only |
| 21 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/ow-gpu-dimensions-progress` | 0 | 30/30 progress/console/encoder contracts pass |
| 22 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V` | 0 | 5/5 suites; 170/170 contracts pass |
| 23 | `ctest --test-dir build-tests -V` | 0 | 5/5 suites; 170/170 contracts pass |
| 24 | `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe` | 0 | 5/5 synthetic video/image artifacts; H.264, HEVC and WebP decode |
| 25 | `./scripts/test.sh` | 0 | 5/5 suites; 170 contracts pass |
| 26 | `cmake --build build-sanitize --parallel 2` | 0 | build only |
| 27 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V` | 0 | 5/5 suites; 170/170 contracts pass |
| 28 | `rtk git diff --check` | 0 | 0 whitespace errors |
| 29 | `./scripts/test.sh` | 0 | final contracts: 5/5 suites; 170 pass, 0 fail |
| 30 | `ctest --test-dir build-tests -V` | 0 | final count capture: 170 pass, 0 fail |
| 31 | `cmake --build build-sanitize --parallel 2` | 0 | final build only |
| 32 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V` | 0 | final contracts: 5/5 suites; 170 pass, 0 fail |
| 33 | `rtk git diff --check` | 0 | 0 whitespace errors |
| 34 | `git diff --cached --check` | 2 | one compiler diagnostic line had trailing whitespace |
| 35 | `git diff --cached --check` | 0 | final staged tree: 0 whitespace errors after trimming diagnostic whitespace |

The sanitizer CMake option instruments core/adaptive and their callers, not the
standalone pipeline/rendition/progress targets. All 170 contracts executed there;
128 core/adaptive contracts were instrumented. The separate strict sanitizer run
instruments all 30 changed progress/console/encoder contracts. No native Windows
sanitizer coverage or physical GPU verification is claimed.

| Final portable suite | Pass | Fail |
|---|---:|---:|
| worker_contracts | 75 | 0 |
| pipeline_contracts | 9 | 0 |
| rendition_contracts | 3 | 0 |
| adaptive_hardware_contracts | 53 | 0 |
| progress_contracts | 30 | 0 |
| Total | 170 | 0 |

## Windows gates — unrun; exit codes unavailable

Zero native checks executed. MSVC compilation, all eight registered Windows CTest
suites, PowerShell, approved-FFmpeg Doctor, actual QSV/AMF/NVENC at 1080p/larger,
VRAM behavior, tray/console and real coordinator job remain operator gates. Their
exit codes and counts must be recorded on Windows; Linux passes cannot substitute.
No source here establishes a particular encoder will win on the operator's PC.

After the operator builds and installs using their own scripts, inspect a real
1080p video's job label. Selection names each codec's backend, for example:

```text
job abcd selected amf/hevc 1920x1080; sampled; guarded
job abcd selected cpu/h264
```

These are conditional examples, not observations from this run. `selected` is a
plan, not proof the encoder succeeded. If a functional profile loses the 1080p
comparison, startup now visibly says one of:

```text
CPU fallback: qsv/h264 measured slower than CPU at 1920x1080
CPU fallback: qsv/h264 measured gain below 5% at 1920x1080
```

Geometry and runtime explanations stay distinct:

```text
job abcd CPU fallback: amf/hevc unsupported geometry 4096x2160
job abcd selected amf/hevc 3840x2160; not yet proven; guarded
encoders now: cpu; amf/hevc failed; demoted; retrying cpu
```

Unknown dimensions report `GPU size not yet proven unknown size` and take CPU.
Larger known geometry inside the ceiling gets guarded execution, not automatic
rejection. On controlled failure the demotion line appears once for that
profile; later jobs use surviving hardware/CPU. CPU retry success still needs its
own succeeded result/ACK; ACK alone does not prove publication. The saved file
must show `measured_width:1920`, `measured_height:1080`, an independent maximum,
and a geometry source; runtime failure removes that profile into diagnostic
demotion history. Closing/reopening the console must continue to leave jobs and
the tray running. Physical quality parity and 4K throughput remain unmeasured.
