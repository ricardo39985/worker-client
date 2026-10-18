# Worker-console log content — local source gate

Date: 17 October 2026. Branch: `fix/worker-next`. Starting commit:
`2f09c69` (retained FFmpeg publication, physical-GPU placement already present).
The complete `/home/rick/.hermes/cache/scratch/log-content-spec.md` was read;
SHA-256: `d25c861b40a2e077eb8de64de1271e3d944c65d945ca904226679fbfe97423e9`.
Only its worker-console scope is implemented here. No Organizer server/TUI file,
Windows machine or production account was accessed. No push, merge, deployment,
installation, restart, release or startup change was performed.

## Mechanisms

`include/ow/worker_status.hpp` is an injected-clock reporting model. The controller
serializes it with a reporting mutex separate from scheduler/profile state. Local
outcomes increment once after attempt execution/cleanup, with a bounded ten-job
history, monotonic durations/idle/uptime, recorded wall-clock finish/since times,
and optional readings. Counters are process-scoped and ACK-independent. No
policy-rejection terminal state exists to count; abandoned/unstarted work is not
invented into another outcome. Protocol cancellation representation is unchanged.

The controller clears the idle frame before START/thread creation; publishes a
plain atomic idle frame every 30 seconds or immediately on idle changes; appends
the table body to its existing plain rotating log every five continuous idle
minutes; and emits complete one-line stats after each job and on idle ticks. The
viewer reads only the log and bounded shared-delete status/history files, redraws
with VT or legacy Win32 operations, and independently refuses an idle frame while
jobs run. H shows the current process's last ten terminal outcomes. Closing the
viewer has no controller/process-tree control path.

Each PCI physical identity appears once, including cards without usable encoder
profiles, plus CPU. Software/unmappable displays cannot become physical cards.
Names truncate and cells/labels retain fixed positions; RTX model names survive
vendor-prefix removal. Per-job progress retains the actual card label/CPU, asset,
measured percentage and remaining time. GPU retry lines capture encode/validation
reasons; terminal success/failure/cancellation lines include duration once.

Actual advertised priority and effective accepting/paused/stopping state carry
since times/reasons. Optional OS memory/disk readings, unassessed encoders,
missing inventory, queue and pre-first-job history remain `unknown`. There is no
coordinator queue-depth field in v1; pending ACKs are not renamed queue entries.
Table text is neutral white with green accepting, orange paused/stopping, and
only a nonzero failed count colored red. Redirected tables remain plain; NO_COLOR
uses uncolored Win32 redraw. Full stats retain all fields even beyond the prior
80-column event cap. Durable events retain the existing UTC/millisecond envelope
and complete job IDs; live timestamps are `dd/mm hh:mm:ss` local time.

The start record explicitly says `outcome unknown`: a future terminal outcome
cannot truthfully be asserted at job start. The terminal line supplies the actual
outcome. See `docs/worker-console.md` for formatting and integration details.

## Executed gates

All commands ran on Linux, with already available tools and the existing reviewed
Boost root `/tmp/ow-boost.mxOndo/boost_1_89_0`; no dependencies were installed or
updated. Logs retain command output; trailing whitespace is normalized for Git.

| Gate | Command / evidence | Exit | Result |
|---|---|---:|---|
| Initial API red | strict GCC status build; `01-red.log` | 1 | missing reporting API/header, before implementation |
| First behavior pass | `03-focused.log` | 1 | 9 contracts, 3 failures: fixed shape, cadence, durable-block suppression |
| Initial behavior green | `04-focused.log` | 0 | 9 contracts, 0 failures |
| Live guard API red | `08-live-red.log` | 1 | missing live running-job guard API |
| Card-in-progress behavior red | `25-placement-red.log` | 1 | 18 reporting contracts, 1 failure: physical card missing from running row |
| Card-in-progress green | `27-placement-green.log` | 0 | combined 50 contracts, 0 failures |
| Outcome-colour behavior red | `34-outcome-colour-red.log` | 1 | 19 reporting contracts, 1 failure: a cancellation reason containing failed changed its history colour |
| Final strict build | GCC C++20, `-Wall -Wextra -Werror -pedantic -pthread`; `35-final-strict-build.log` | 0 | progress, encoder-run and status sources compile |
| Final strict contracts | `.cache/ow-progress-contracts`; `36-final-strict.log` | 0 | 51 contracts, 0 failures |
| Final portable repository | `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0`; `37-final-portable.log` | 0 | 6/6 CTest suites; 231 contracts/cases, 0 failures |
| Final sanitizer build | same strict sources plus `-fsanitize=address,undefined -fno-omit-frame-pointer -g`; `38-final-sanitizer-build.log` | 0 | compiled |
| Whitespace check | `git diff --check`; `41-diff-check.log` | 0 | no whitespace errors |
| Final sanitizer contracts | `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 .cache/ow-progress-sanitize`; `39-final-sanitizer.log` | 0 | 51 contracts, 0 failures; no ASan/UBSan/leak report |

`40-final-suite-details.log` records all repository counts: 23 dependency cases,
75 core contracts, 9 pipeline, 3 rendition, 70 adaptive hardware, 51 combined
progress/console/encoder-run/status. Of the latter, 19 are new reporting contracts.
The separate sanitizer gate instruments all changed portable reporting/rendering
code; it is not a claim that Windows controller/viewer code was instrumented.

Tests cover independent counters; no increment on duplicate completion; counters
across pause/resume/new jobs; fixed-clock exact summary/table; state-since
stability; wall-clock correction with monotonic durations; bounded history;
30-second live/300-second durable cadence; continuous-idle reset; no busy table;
unknown readings/queue/history/priority/encoders; physical deduplication and long
name truncation; fixed total columns; semantic color spans and plain roundtrip;
redirected durable blocks; job start/progress/terminal tracking; explicit retry
reason; full summary placement/priority/state; and GPU-to-CPU progress labels.

## Unrun native gates

MSVC, Windows SDK compilation, Windows CTest and PowerShell suites: **not run**,
no native exit code or passing count claimed. Portable contracts cannot establish
native controller integration, physical GPU support, or console rendering.
A later, separately authorized Windows verification must confirm:

- Build with MSVC and locked dependencies, then run all registered native CTest
  suites; record exits and actual per-suite case counts.
- Watch a real idle console for at least 30 seconds and its plain durable log for
  five minutes; verify stable column positions and absence of idle scrollback.
- Test VT and legacy consoles, narrow/resized windows, light/dark 16-color palettes,
  non-UTC local time and NO_COLOR. Only the real console can establish cursor
  erasure, wrapping/scrollback and palette legibility.
- Start concurrent synthetic jobs on the actual cards; confirm one inventory entry
  per physical PCI instance plus CPU, correctly retained running card labels,
  immediate table removal and no table while any job runs. Validate real encoder
  timelines and transfers, rather than treating fixtures as hardware evidence.
- Exercise success, failure, cancellation, lease expiry, pause/resume, priorities,
  offline/reconnect, stop/drain, GPU CPU-retry, H history, log rotation and viewer
  close/reopen. Verify controller/jobs survive closing the viewer and counters do
  not change on ACK retries or reset on new jobs.

The Organizer server's per-worker tables and production integration remain outside
this worker-only change. Nothing here establishes a compiled release, installation,
production publication, or real Windows job outcome.
