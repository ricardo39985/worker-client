# Worker console reporting

The controller owns reporting data. The viewer remains a separate, read-only
process; closing it cannot signal the controller or a job. This implements only
the single-machine parts of the operator's log-content specification. The
Organizer server's `worker_tui.py` is outside this repository.

- Counts are local terminal outcomes since this controller process started,
  initialized once and retained across jobs and pause/resume. A completed attempt
  increments exactly one of succeeded, failed or cancelled, after execution and
  cleanup. ACK retransmission cannot increment a counter. Unstarted offers,
  abandoned work from an earlier process, and coordinator publication outcomes
  are not relabelled as executed successes or failures. There is no distinct
  rejected-by-policy terminal state in this worker.
- A start says `outcome unknown`; the future outcome cannot truthfully be known
  at start. The terminal line says `succeeded`, `failed` or `cancelled`, with
  `done in <duration> (<subject>)` once and the actual failure/cancellation reason.
  Cancellation retains the existing protocol's failed result representation;
  reporting distinguishes the local cancellation without changing that protocol.
- Stage transitions name the actual local asset (`source.bin`, `feed.mp4`,
  `optimized.mp4`, `poster.webp`, etc.). Encoder media position, exact transfer
  bytes and hashes remain the progress sources. Unknown extents never become a
  percentage. GPU retry records carry the captured encode/validation reason.
- Live job labels use the last four attempt characters, with the existing live
  collision disambiguation. Progress rows retain the actual encoder's card label
  or CPU. Durable job records keep the complete attempt and PCI identities.
- The complete one-line `stats:` report preserves the specified counters, last
  finish time/duration/type, queue and idle fields, followed by current running
  placement, RAM/scratch, priority and acceptance state/since/reason. It is not
  truncated to the old 80-column event limit. Long reports may wrap visually on a
  narrow terminal, but remain one greppable record. Ordinary events/progress and
  table rows retain the 80-column presentation limit.
- Queue depth is `unknown`: protocol v1 does not supply it. Pending ACKs and local
  reservations are not a substitute for a measured coordinator queue. Memory
  and scratch use optional successful OS readings in decimal GB. Inventory
  failure prints `unknown`, rather than a zero-card claim. Encoders remain
  `unknown` until assessed; an assessed empty set is `none`.
- Effective acceptance distinguishes accepting, paused and stopping; operator,
  pairing, offline, journal and watchdog reasons are recorded when known.
  A connection failure cannot replace stopping, and reconnect does not erase a
  local pause. Priority uses the actual advertised enum, including saved settings.

## Idle table and cadence

The controller atomically publishes `worker-status.txt` at most every 30 seconds
while idle, and immediately on an idle state/priority change or final completion.
At job start it publishes an empty frame before the START record or job thread.
The viewer additionally suppresses an idle frame while its event stream has
running jobs, so a stale/unreadable frame cannot cover job progress.

The same plain table body is appended to `worker.log` at startup and every five
continuous idle minutes. Returning from work restarts this slower timer. Idle
summaries remain on the 30-second timer and are also emitted after every job.
The durable log retains its existing UTC/millisecond envelope; the console
renders local `dd/mm hh:mm:ss` timestamps, and the status/summary bodies carry
that same local timestamp. No terminal escape sequence is written to a file.

The table has a two-space indent, an eleven-column label field, a 25-column state
field, a 26-column last-job subject field, and 22-column card cells (three per
row). Card names truncate at twelve characters; common vendor prefixes are
removed so RTX 3070 and RTX 3050 remain distinguishable. Card order follows
physical PCI identity, deduplicated independently of encoder profiles and aliases;
software/unmappable display entries never become physical cards. CPU is always
present. Totals reserve eight columns for the success field and eleven for the
failure field; unusually large counts expand rather than silently lose digits.
Elapsed durations, idle time and uptime derive from monotonic timestamps on each
render; finish/since clock times derive from recorded wall-clock instants.

Header/ordinary table text is white. Only accepting is green, paused/stopping
are orange, and only a nonzero failure count/label is red. The live renderer
supports ANSI and legacy Windows cursor/color operations; `NO_COLOR` uses
uncolored in-place Win32 output. Redirected output has no cursor/color escapes
and receives the plain durable blocks at the slower cadence.

`H` prints the last ten outcomes from this process with local timestamps, four
character job labels, durations and reasons. `worker-history.txt` is an atomic,
bounded read-only viewer input, cleared on controller startup. Before a job has
completed, last-job information is `unknown`. Older durable log files remain the
historical evidence; they are never guessed into this process's counters.

Status/history files use bounded shared-delete reads so atomic replacement does
not depend on a reader closing at the right instant. A file-publication failure
is reported without making that file a scheduling prerequisite. The journal,
lease, reservation, encoder and publication behavior is unchanged.

See [the gate ledger](../verification/log-content/REPORT.md) for portable evidence
and the unrun Windows acceptance checks.
