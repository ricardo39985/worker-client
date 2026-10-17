# Declared VRAM fallback — local source verification

October 17, 2026. Linux only; no push, install, deployment or worker restart.

The reported Intel HD 630 (128 MiB) and Radeon RX Vega M GL (4043 MiB)
were rejected before any hardware encode because adapter usage telemetry was
absent. Child probing also required a nonzero dedicated-memory counter, and
runtime admission converted missing telemetry into zero headroom.

Admission now uses live headroom (declared capacity minus usage rounded up to
MiB) when present. Without usage telemetry, its budget is
`min(global ceiling, floor(declared dedicated MiB / 2), 1024 MiB)`.
Zero capacity/headroom/ceiling refuses admission with a reason. Explicit zero
VRAM still disables all GPU probing. No config or wire protocol changed.

Missing child samples use that same fallback and reserve the entire budget,
not an assumed measured footprint. Probe polling and runtime polling retain
child-allocation and total-adapter checks whenever samples exist. Missing
samples permit only reservations within the fallback bound. Observed violations
abort/drain the contained process tree before validated CPU retry. Runtime
retains the tested dimensions, identity/hash checks, cancellation, lease, CPU/RAM/
scratch limits and output validation. Existing atomic device/global scheduling,
one encode per device and drain ownership remain in place. Human profile output
adds `(declared-VRAM estimate)` or `(live VRAM telemetry)`.

This cannot oversubscribe the worker's declared reservation budget: fallback
reservations never exceed half the declared memory, 1024 MiB or the global cap,
and concurrent work cannot reuse a held reservation. These are admission limits
and sampled watchdogs, not a hard GPU allocation quota. Absent telemetry cannot
prove actual external-app usage or bound an encoder's unobserved allocation.
No such physical memory guarantee is claimed.

## TDD and gates

The regression harness was compiled against a telemetry-required policy first:
`g++ -std=c++20 -Wall -Wextra -Werror -pthread -Iinclude -Itests tests/main.cpp tests/adaptive_hardware_test.cpp src/adaptive_hardware.cpp -o /tmp/ow-vram-contracts && /tmp/ow-vram-contracts`
exited 1: 37 contracts, 2 expected failures (Radeon and Intel missing-telemetry
budgets). After implementation it exited 0: 37/37. A further contract verifies
that estimates never replace encode, decode, validation, tuning or identity
proof; the final adaptive suite is 38/38.

Final gates, all exit 0:

| Command | Result |
| --- | --- |
| `rtk test ./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 5/5 suites, 142/142 contracts |
| `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | configure succeeded |
| `cmake --build build-sanitize --parallel 2` | all targets built |
| `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V` | 5/5 suites, 142/142 contracts; output captured in /tmp/ow-vram-sanitize.log |
| `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe` | 3/3 synthetic H.264, HEVC, WebP profiles encoded and decoded |
| `rtk git diff --check` | no whitespace errors |

Suite counts: core 75, pipeline 9, rendition 3, adaptive 38, progress/console 17.
Portable suite counts were read from build-tests/Testing/Temporary/LastTest.log.
The additional verbose portable CTest count capture also exited 0 (5/5).

## Prediction and native gates

After an operator rebuild, the two adapters should remain in adaptive-hardware.json.
Missing telemetry will no longer create the reported preflight refusals. Assuming
unchanged healthy drivers and sufficient configured resources, both will attempt
real H.264/HEVC probes with fallback ceilings of 64 MiB (Intel) and 1024 MiB
(Radeon). Successful profiles retain max_concurrency=1 and the tested 640x360
envelope. With no child samples, vram_mb is the whole fallback reservation.
CPU profiles remain available. Expect a Radeon AMF profile if its real encode,
decode and repeated tuning clear the existing 5% CPU performance floor. Intel
may still be refused by observed allocation, driver/encoder failure or throughput.
Neither codec success nor a real GPU job is proven by portable fixtures.

Unrun: actual MSVC compilation, all Windows native suites, Doctor on these two
adapters, actual memory/driver behavior, and an operator-run real GPU encode.
Larger/unknown input frames continue to use CPU. No Windows host was contacted.
