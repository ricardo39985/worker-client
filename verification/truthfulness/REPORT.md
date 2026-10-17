# Operator truthfulness audit — 17 October 2026

Branch: `feat/gpu-on-logs`. Base: `aa55b04`. Linux source and portable evidence only.
No push, installation, deployment, restart, Windows connection or production access.
No encoder commands, media outputs, device choice, budgets, reservations, fallback policy
or scheduler implementation changed. Runtime failure suppression is a reporting ledger,
not a scheduler blacklist. A later job can still select that profile under existing policy;
its failure stays excluded from the summary until a fresh probe. Profile removals continue
through the existing invalidation policy.

## Findings (before / missing evidence / after)

1. **Invented CPU backend.** `encoder_tokens({})` returned `cpu`, and hardware-only
   sets also gained CPU. Neither established CPU evidence. Tokens now describe only
   supplied profiles; the report separately adds CPU when a diagnostic/image/app CPU
   sample actually passed. No converter success means `encoders=none`.
2. **Invented capability success.** The formatter replaced explicit failed app probes
   with successful diagnostic probes; substring/prefix matching also accepted unrelated
   fields and values such as `trueish`. It now reads complete typed fields, keeps app and
   diagnostic scope distinct, and says `unknown` for absent/invalid evidence. Missing
   encoder detail still prints no backend. Unsupported vaapi/videotoolbox names and
   `not_encoders=cpu` no longer qualify as evidence from this Windows worker.
3. **Probe evidence overstated as readiness.** `ready: video+image` and unqualified
   verified/fps prose could suggest real-job assurance. The console now says
   `profiled: ...`, then `startup samples only; job support may vary`. Durable detail
   says sampled CPU fallback and synthetic fps at 640x360. Adapter identity verification
   remains specifically about identity mapping, not production encoding or quality.
4. **Stale encoders after failure.** The startup line never changed after a real
   hardware attempt failed or failed decode validation and entered CPU fallback.
   The existing fallback boundary now records that device/codec once and emits e.g.
   `encoders now: cpu; amf/hevc failed; retrying cpu`. This describes a retry attempt,
   not a successful CPU result. Repeated failures do not repeat the update; another
   surviving codec/device can legitimately keep its backend in the union of names.
5. **Stale encoders after invalidation.** Adapter/driver/profile removal and approved
   FFmpeg invalidation did not update the ready report. The same reporting ledger now
   emits a single changed-evidence record; binary invalidation retracts CPU too and
   reports `encoders now: none`. The existing separate invalidation reason remains.
6. **Selection described as execution.** `using GPU profiles` was logged before the
   child existed. It now says `selected GPU profiles`; selection proves only the plan.
7. **Stale heartbeat/Doctor app-video claim.** A passed initial app sample could
   survive an exception before adaptive CPU fallback profiles existed. These reports
   now require current H.264 and HEVC CPU fallback profiles as well as the app sample.
   Removing hardware alone leaves valid CPU capability evidence intact. Admission
   flags and encoder-selection policy were not rewritten.
8. **Unrun Doctor probe represented as false.** `processing_ready:false` appeared
   even without a probe. It is now JSON null with `probe_evidence:not_run` until checked;
   a run is labelled `startup_samples`. A genuinely failed probe remains false.
9. **Invented priority/routing guarantee.** Missing priority text became `normal`;
   the tray said `Backup only` without controlling coordinator routing. They now say
   `priority unknown` and `Backup (routing hint)` respectively.
10. **Zero substituted for missing inventory.** Failed adapter inventory could become
    `detected adapters=0` / “adapter not detected”; metrics skipped on failed app probes
    or failed OS queries printed zero. Adapter absence is asserted only after a
    successful inventory. Unobserved counts now say `unknown`; saved RAM/disk totals
    use JSON null on query failure. No-converter text describes configuration rather
    than assuming an operator explicitly chose NoConversion. The native wording
    contract was updated but has not run here. An unset VRAM override is explicitly
    `not_set`, rather than reporting the default zero as an operator override.

## Probe presentation

The worker updates a thread-safe phase record while doing the same probe work.
Elapsed time begins at the first probe phase, excluding prior controller initialization.
The existing watchdog samples it at most once per five seconds. It shows adapter
inventory, converter hash, CPU sample stage (current/5 and stages left), and tuning
backend/codec/adapter plus encode/decode or serial/parallel repeat. Tuning counts are
candidate position/total and candidates left, including candidates that may be skipped;
they are not an ETA or a success count. Work without a known total has elapsed time
only. Final selection/persistence have named phases too.

The interactive viewer maintains one neutral probe row beside the existing job rows.
A new probe, result or failure removes the preceding probe row. Redirected/NO_COLOR output prints bounded plain
progress lines, including elapsed time. Long phase names cannot truncate elapsed time.
The durable log remains plain UTC/millisecond text; the viewer retains local
`dd/mm hh:mm:ss`, four-character job labels, 80 columns and exactly five colors.

## Audit coverage and retained meanings

Reviewed formatter/progress records, worker startup/job/result/invalidation logs,
heartbeat fields, tray/console status, Doctor, inventory, and their source owners.
`verified` remains the protocol field name and means local successful startup sample
evidence, not quality parity, arbitrary input support, production success, current
free capacity, or operator preference. Current CPU fallback profiles additionally
qualify app-video reporting. Disabled preferences remain separate explicit vetoes.

Heartbeat `status:ready` is protocol admission state (not paused), not an encoder or
production-verification claim; it is paired with capabilities/preferences/resources.
Wire `available.vram_mb:0` is correct for protocol v1's no remote VRAM requirement,
not a physical inventory claim. CPU availability is logical resource capacity, not
measured CPU idle percentage. Tray connectivity/paused/draining states describe their
actual events. Result queued means durable outbox persistence; ACK means matching
coordinator receipt, not publication. None were relabelled as production success.
Doctor runtime/crypto/storage checks and configuration validation retain their actual
checked meanings. No model/inference capability was introduced.

## Gate ledger

All commands ran from `/home/rick/Downloads/worker-client-gpu-on-logs`.
`build-tests` already referenced `/tmp/ow-boost.mxOndo/boost_1_89_0` in CMakeCache;
`scripts/test.sh` retained it. This is the locked 1.89.0 version, not an upgrade.
No dependencies were downloaded or installed by this task.

| Log | Exact command | Exit | Counts / result |
|---|---|---:|---|
| 01 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 02 | `build-tests/ow_progress_contract_tests` | 1 | 23 contracts: 17 pass, 6 expected failures |
| 03 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | 41 contracts: 39 pass, 2 expected failures |
| 04 | `cmake --build build-tests --parallel 2` | 2 | new observer APIs absent; no tests executed |
| 05 | `cmake --build build-tests --parallel 2` | 2 | missing algorithm include; no tests executed |
| 06 | `build-tests/ow_progress_contract_tests` | 1 | STALE binary after build failure: 17/23 pass, 6 fail; not new verification |
| 07 | `build-tests/ow_adaptive_hardware_contract_tests` | 1 | STALE binary after build failure: 39/41 pass, 2 fail; not new verification |
| 08 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 09 | `build-tests/ow_progress_contract_tests` | 0 | 26 pass, 0 fail |
| 10 | `build-tests/ow_adaptive_hardware_contract_tests` | 0 | 44 pass, 0 fail |
| 11 | `cmake --build build-tests --parallel 2` | 2 | additional presentation API absent; no tests executed |
| 12 | `cmake --build build-tests --parallel 2` | 2 | Priority belonged to Windows namespace; no tests executed |
| 15 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 16 | `build-tests/ow_progress_contract_tests` | 0 | 28 pass, 0 fail |
| 17 | `build-tests/ow_adaptive_hardware_contract_tests` | 0 | 47 pass, 0 fail |
| 18 | `./scripts/test.sh` | 0 | 5 suites pass, 0 fail; 162 contracts |
| 19 | `ctest --test-dir build-tests -V` | 0 | 5 suites pass, 0 fail; 162 contracts |
| 20 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 21 | `build-tests/ow_progress_contract_tests` | 1 | 27 pass, 1 expected unsupported-backend failure |
| 22 | `./scripts/test.sh` | 0 | 5 suites pass, 0 fail; 162 contracts |
| 23 | `ctest --test-dir build-tests -V` | 0 | 5 suites pass, 0 fail; 162 contracts |
| 24 | `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 0 | configure only |
| 25 | `cmake --build build-tests --parallel 2` | 2 | additional current-evidence API absent; no tests executed |
| 26 | `./scripts/test.sh` | 0 | 5 suites pass, 0 fail; 163 contracts |
| 27 | `ctest --test-dir build-tests -V` | 0 | 5 suites pass, 0 fail; 163 contracts |
| 28 | `cmake --build build-sanitize --parallel 2` | 0 | build only |
| 29 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize -V` | 0 | 5 suites pass, 0 fail; 163 contracts; core/adaptive instrumented |
| 30 | `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o /tmp/ow-truthfulness-progress` | 0 | strict instrumented build only |
| 31 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/ow-truthfulness-progress` | 0 | 28 progress/console/encoder contracts pass, 0 fail |
| 32 | `cmake --build build-tests --parallel 2` | 0 | build only |
| 33 | `build-tests/ow_progress_contract_tests` | 1 | 26 pass, 2 expected elapsed-origin/stale-row failures |
| 34 | `./scripts/test.sh` | 0 | final tree: 5 suites pass, 0 fail; 163 contracts |
| 35 | `ctest --test-dir build-tests -V` | 0 | final tree: 5 suites pass, 0 fail; 163 contracts |
| 36 | `g++ -std=c++20 -Wall -Wextra -Werror -pedantic -pthread -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Itests tests/main.cpp tests/progress_test.cpp tests/encoder_run_test.cpp -o /tmp/ow-truthfulness-progress` | 0 | final strict instrumented build only |
| 37 | `ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/ow-truthfulness-progress` | 0 | final 28 progress/console/encoder contracts pass, 0 fail |
| pre-staging | `rtk git diff --check` | 0 | repeated during review; 0 whitespace errors |
| final staging | `git diff --cached --check` | 0 | 0 whitespace errors |


Logs 13/14 do not exist: guarded runs were correctly skipped after build 12 failed.
Filenames 06/07 contain “green” from the attempted run naming, but their contents
are stale red binaries, explicitly excluded from green evidence. Compile-stage red
checks are not described as executed behavior failures. The initial behavior red
and unsupported-backend red failures were executed and reproduced the intended claims.

The CMake sanitizer option instruments core/adaptive via `ow_core`; it does not
instrument the standalone progress/pipeline/rendition targets. The separate commands
30/31 and final 36/37 explicitly instrument all changed portable console/progress code. No sanitizer
coverage of native Windows sources is claimed.

| Portable suite | Final pass | Final fail |
|---|---:|---:|
| worker_contracts | 75 | 0 |
| pipeline_contracts | 9 | 0 |
| rendition_contracts | 3 | 0 |
| adaptive_hardware_contracts | 48 | 0 |
| progress_contracts | 28 | 0 |
| Total | 163 | 0 |

## Windows gates — not run, zero native passes claimed

No Windows/MSVC/PowerShell build, native CTest, Doctor, physical GPU encode, tray,
legacy-console or real-job check ran. The operator must use their own build/install
scripts; this task did not install or restart anything. Before release, require MSVC
compilation and all eight registered native/portable/setup CTest suites. Their native
pass/fail counts and exit codes are **not available**, not zero-exit assumptions.

After the operator's next install, eyeball:

- During the roughly 50-second startup wait, one probe row changes phase and elapsed
  time about every five seconds. CPU sample stages and tuning candidates show current/
  total and how many remain; tuning identifies adapter/codec and repeats. No invented
  percent or time-remaining promise appears. The row disappears on result/failure.
- The final line says `profiled: ...` with only evidenced encoders, followed by startup
  sample scope. Failed probes do not borrow a diagnostic success. An old log record
  without encoder detail shows no backend.
- With an operator-controlled hardware failure, one orange encoder-change line names
  the failed backend/codec, surviving encoders and the CPU retry. Repetition of the same
  failure does not spam. The job's result still independently establishes retry success
  or failure. If another codec/device survived, its backend can remain listed.
- Existing adapter/driver or approved-binary invalidation produces one updated encoder
  line; binary invalidation says none and retains its converter-disabled reason.
- The tray says `PROBING` during background probing and offers `Backup (routing hint)`.
  Closing/reopening the viewer still leaves the controller and jobs running. Observe
  several jobs: suffix labels, bars, neutral probe row, local timestamps and five colors
  remain readable on light/dark and legacy 16-color consoles.
- Redirected output and NO_COLOR stay plain, with one probe observation per five seconds
  rather than a frozen start line. Doctor without the probe says null/not_run; Doctor
  with it says startup_samples and reports only current profile-supported app capability.

These checks do not establish GPU quality parity, larger input support, inference,
coordinator output publication or production acceptance.
