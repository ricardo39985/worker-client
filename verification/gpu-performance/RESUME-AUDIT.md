# Optimization resume audit — October 18, 2026

The requested decode changes were already committed as `273441faf13a9efdb852a0cc44c5da3b4221d1f7`
when this continuation inspected the branch. No additional runtime optimization
was introduced. The measured default-priority complete-job gain remains
**1.123× at 1080p and 1.047× at 4K** on the RTX 3050. Complete jobs produce both
H.264 and HEVC plus the poster; their medians are 17.794 → 15.841 seconds and
55.631 → 53.137 seconds. CPU time is 17.844 → 13.672 seconds and
56.031 → 38.500 seconds. See [MEASURED.md](MEASURED.md) for every stage,
repetition range, Low-priority comparison and quality result.

All eight code/test/build file hashes in `tested-source.json` match that commit.
An isolated `git archive` snapshot also matches after the declared PowerShell
CRLF-to-LF normalization. Existing native XML records ten suites with zero
failures or skips. No new PC connection, measurement, directory or write was
needed. The prior fresh-connection cleanup result remains recorded in
`54-cleanup.log`: the sole authorized PC test directory no longer existed.

Fresh local checks ran against that isolated optimization commit:

- Release: `scripts/test.sh` with the existing Boost 1.89.0 tree, exit 0.
  Six CTest suites / 245 cases; see `55-resume-release.log`.
- Debug: `OW_SANITIZE=ON`, `OW_ENABLE_IPO=OFF`, and explicit global
  `-fsanitize=address,undefined` so the standalone rendition, pipeline and
  progress targets are instrumented as well. Build exit 0; six suites / 245
  cases with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1`, CTest exit 0. See `55-resume-sanitize.log`.

An unrelated process integrated the separately owned memory-budget work into
this worktree during the audit and continued editing it. This audit neither
performed that integration nor altered its files. The fresh checks above and
the recorded Windows measurements establish `273441f`, not the later integrated
source or another process's uncommitted edits. No production acceptance is
claimed. No push, installation, release, deployment, production or Cloudflare
action was performed by this continuation.

No new performance claim is added: shared two-rendition decoding remains an
offline rejected experiment (about 2% extra gain with unresolved session
admission requirements); validation remains unchanged; full-duration HEVC-input
throughput, representative footage and actual admitted/networked jobs remain
unproven. H.264/HEVC output parity is SSIM 1.000000, PSNR infinity at both sizes.
