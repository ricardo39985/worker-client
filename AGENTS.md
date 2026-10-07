# Organizer Worker: agent entry point

Read README.md, docs/architecture.md, contracts/worker-v1.md, and docs/testing.md
before changing this repository. Read CONTRIBUTING.md and docs/status.md to distinguish implemented
source from verified execution and production integration.

This is a separate Windows compute-worker repository, not the Organizer app or
Lightsail deployment repository. Do not merge, publish releases, enable startup,
install software on somebody's PC, change firewall/SSH settings, contact real
accounts, or deploy to Lightsail without the relevant user authorization.

## Development contract

Use contract-first test-driven development: define an observable outcome; write
and run a failing behavior test; implement it; refactor with contracts green.
Tests verify behavior and security/persistence guarantees, not helper names,
private fields, snapshots of current source, or arbitrary internal call order.
A refactor may replace an entire implementation without rewriting unchanged
behavioral contracts. Keep modules small and single-purpose.

Preserve these invariants:
- Closing the console cannot terminate the tray/controller or its jobs.
- Pause rejects new starts, including previously reserved offers; running jobs
  finish unless the operator explicitly chooses stop or a lease expires.
- Only typed, installed, successfully probed capabilities accept work. GPU
  detection is not proof of a usable GPU backend. Missing capabilities stay off.
- Worker preferences and resource reservations are per machine and per task.
- Accepted reservations consume budget atomically. Cancelling a running job
  cannot release capacity until its process tree has terminated.
- An offer is not permission to execute. A valid lease and separate BEGIN are
  required. Old attempts never publish over a newer attempt's output.
- Durable results are retried until a matching coordinator acknowledgment;
  restart does not silently rerun an interrupted attempt.
- No arbitrary commands from the network, shell evaluation, unsigned automatic
  executables, TLS verification bypass, public worker listener, or admin runtime.
- Keep local secrets and signed URLs out of logs, fixtures, commits and releases.

Windows-specific changes require actual MSVC compilation and native tests before
release. Linux core tests cannot establish Windows behavior. Record unrun checks
and missing prerequisites accurately; never mark a production service deployed
because a protocol document or mock exists. This repository has no actual
inference adapter yet. Verify model/runtime compatibility before adding one.

Use the reviewed dependency lock. No automatic upgrade to latest, no host-wide
PATH changes, no machine execution-policy changes, no silent license acceptance.
Manual-only CI is intentional. Keep production untouched until explicitly approved.
