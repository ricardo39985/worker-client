# fix/worker-next finish attempt — October 17, 2026

Publication status below is historical. The operator subsequently authorized
publication of the recovered source/build materials together with the binary.
See [the new publication report](../ffmpeg-publication/REPORT.md) for the release,
lock URL changes, actual public-download verification and source-scope limitation.

## Outcome and boundary

Item 1 is implemented and verified with portable behavior contracts. Item 2 is
prepared but **not completed**: no release or asset was created, neither lock URL
was filled, and the real publication/download gate still fails. Publication is
explicitly authorized; missing required redistribution material is the blocker,
not permission or GitHub authentication.

The repository remains on `fix/worker-next`. No git push, merge, installation,
startup change, deployment, restart, Windows-host or production access occurred.
The installed `gh` authenticated account is ricardo39985; the target repository
is public. No credentials or signed transport URLs are recorded here.

## Physical cards

Read the complete finish brief and its physical-adapter evidence. The evidence's
RTX 3070 and RTX 3050 have different complete PCI device instance IDs. The Meta
software display is ROOT-enumerated. The new Windows boundary enumerates present
Display-class PnP devices, resolves each DXGI LUID through its Windows adapter
interface and SetupAPI, and requires that exact instance to exist in the present
Display inventory. It never matches by name, product tuple or guessed ordinal.
PnP-only displays remain visible diagnostic exclusions.

Only complete PCI instances can feed profiles/capacity. Case is normalized before
comparing instances. ROOT, missing mappings and ambiguous verified aliases stay
withheld. An unproven/software alias cannot poison a real card. The scheduler's
VRAM/session accounting uses the PCI instance key; DXGI identities remain for
telemetry, profile freshness and command mapping. Each card keeps one session,
with the existing shared two-session worker NVENC ceiling, CPU fallback reservation,
CPU/RAM/scratch budgets and drain-bound ownership unchanged.

The evidence-shaped fixture now reports exactly two schedulable cards and admits
two independent placements. Six new contracts cover ROOT exclusion, all-backend
missing PCI identity, that two-card fixture, case-insensitive PCI aliases,
exclusion reasons and forged saved ROOT profiles. Existing concurrency/cancellation/
fallback contracts remain green. This is fixture evidence, not a Windows result.

Actual child-launch logs already included `physical_id`; it now contains the
complete PCI instance, rather than a CUDA UUID. Inventory/profile diagnostics
include explicit software/unmappable/ambiguous reasons. Admission logs name the
physical card and session, VRAM or CPU/RAM/scratch exclusion. The raw inventory
count is labelled display inventory, separate from schedulable GPUs.

Files: `windows/physical_adapters.{hpp,cpp}`, `windows/hardware.cpp`,
`windows/worker.cpp`, `include/ow/{adaptive_hardware,device_identity}.hpp`,
`src/adaptive_hardware.cpp`, `CMakeLists.txt`, adaptive/native contracts and
`docs/architecture.md`.

Windows API references:
- https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_adapter_name
- https://learn.microsoft.com/en-us/windows/win32/api/setupapi/nf-setupapi-setupdigetdeviceinterfacedetailw

## Retained FFmpeg

The approved executable was read locally, never executed:

```text
sha256=f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4
bytes=165167104
embedded version=n9.0.2-22-g46d8f462ee-20261001
local artifact integrity check exit=0
```

`20-local-artifact.log` is an explicitly source-only/pending-publication check.
It is not verification of a published download. There is **no published asset URL**
and **no downloaded-publication hash/size result** to report.

`docs/ffmpeg-distribution.md`, publication procedure step 3, requires matching
source, patches/build instructions and included-dependency notices, and says:

> If matching source/notices cannot be established, publication remains blocked.

Recovered exact FFmpeg source commit
`46d8f462eeb87ee1f704d8c44a0ee24fca471ad1`, its GPLv3 text/license description,
the embedded configure flags, and BtbN recipe commit
`e88e49f624457c455700b058f0a84ca87d499cc2`. Upstream successful build run
36860751422 uses that recipe revision. The deleted release returns HTTP 404;
its action artifacts list is empty and the relevant job logs return HTTP 410.
The recipes provide 125 dependency-stage references, including secondary source
repositories; that is not a complete matching included-library/transitive source
and notices bundle. No existing written offer was supplied or found. No new
promise of future source delivery was invented on the operator's behalf.

`publication-recovery.json` records exact staged recovery paths, hashes and
sizes under `/tmp/worker-next-publication`. Those are partial recovery materials,
not an approved redistribution bundle. The operator was asked where the complete
matching bundle or existing written offer is retained. Binary publication and lock
URLs must wait for that prerequisite.

The Python publication gate is prepared for the explicitly requested GitHub
release origin: its full-hash-tag URL may redirect exactly once to GitHub's HTTPS
asset CDN. Credentials, foreign hosts, downgraded TLS, extra hops and unsafe stored
URLs remain rejected. CDN query URLs are transient and never stored/logged. Final
HTTP 200, exact byte count and SHA-256 are still mandatory. Generic origins still
cannot redirect. Four new distribution contracts reproduce and cover that policy.
The PowerShell installation, consent, checksum and atomic-publish behavior and
approved pin/size were not changed.

Files: `scripts/check_ffmpeg_dependency.py`, `tests/ffmpeg_dependency_test.py`,
`docs/ffmpeg-distribution.md`, `docs/{status,testing}.md` and this ledger. The
`config/dependencies.lock.json` file remains unchanged, with two null URLs.

## Final local gates

All log paths below are relative to this directory. The Boost source root used
for these local checks already existed; no dependency/compiler was installed.
`B` below means `/tmp/ow-boost.mxOndo/boost_1_89_0`.

| Command | Exit | Counts / evidence | Log |
|---|---:|---|---|
| `scripts/test.sh -DOW_BOOST_ROOT=B` | 0 | 6/6 suites, 210 contracts | 21-final-portable.log |
| `cmake --build build-sanitize --parallel 2` | 0 | build completed | 22-final-sanitizer-build.log |
| `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-sanitize --output-on-failure -V` | 0 | 6/6 suites, 210 contracts | 23-final-sanitizer.log |
| `ctest --test-dir build-tests --output-on-failure -V` | 0 | 6/6 suites, 210 contracts | 24-final-release-details.log |
| `python3 tests/media_rendition_smoke.py --probe build-tests/ow_rendition_probe` | 0 | 3/3 synthetic H.264/HEVC/WebP outputs decoded | 25-media-smoke.log |
| `python3 tests/adaptive_probe_smoke.py --evidence-probe build-tests/ow_adaptive_evidence_probe --rendition-probe build-tests/ow_rendition_probe` | 0 | 2/2 1080p tuning outputs, 72 frames/AAC/codec/geometry/decode | 26-adaptive-smoke.log |
| `python3 scripts/check_ffmpeg_dependency.py --allow-unpublished --artifact-file /tmp/ffmpeg.exe` | 0 | 1/1 local file matches; publication explicitly pending | 20-local-artifact.log |
| `python3 scripts/check_ffmpeg_dependency.py --download` | 1 | 0 downloads; missing publication/materials | 27-publication-download.log |
| `python3 tests/ffmpeg_dependency_test.py` | 0 | 21/21 (rerun after final URL-port guard) | 29-distribution-final.log |
| `git diff --cached --check` | 0 | 0 whitespace errors after staging all files | 32-staged-check.log |
| Windows MSVC/native/PowerShell | unrun | 0 executed; no Windows host contacted | operator checklist below |

Contract breakdown: 75 worker/journal/protocol/pairing + 9 pipeline + 3 rendition
+ 70 adaptive + 32 progress/console + 21 Python distribution = **210**. Native
MSVC and the actual pinned Windows executable are not covered by Linux FFmpeg
smokes. ASan/UBSan instruments the C++ suites; Python policy tests also register
in CTest but are not sanitizer-instrumented.

## Red and intermediate gate ledger

The exact command is in the named log where available; the commands below also
cover initial logs captured without a command preamble. These are historical
stages, not extra final failures.

| Logs | Command | Exit(s) | Counts / reason |
|---|---|---|---|
| 01-gpu-red, 04-build-green, 06-build | `cmake --build build-multigpu --parallel 2` | 0, 0, 0 | builds |
| 02-gpu-red | `build-multigpu/ow_adaptive_hardware_contract_tests` | 1 | 68 contracts, 4 intended regressions fail |
| 03-publication-red | `python3 scripts/check_ffmpeg_dependency.py` | 1 | publication pending, 0 downloads |
| 05-gpu-green | `build-multigpu/ow_adaptive_hardware_contract_tests` | 1 | 68 contracts, 3 canonical-fixture comparisons fail during implementation |
| 07-adaptive | same adaptive executable | 0 | 69/69 |
| 08-portable | `scripts/test.sh -DOW_BOOST_ROOT=B` | 8 | 5/6 suites pass; new redirect contracts ran before handler implementation |
| 12-github-download-red | `python3 tests/ffmpeg_dependency_test.py` | 1 | 21 cases, 7 missing-constructor errors |
| 13-github-behavior-red | same Python command, constructor present | 1 | 21 cases, 2 expected rejected-GitHub-redirect errors |
| 14-github-green | same Python command | 0 | 21/21 |
| 15-portable | `scripts/test.sh -DOW_BOOST_ROOT=B` | 0 | 6/6 suites, 210 contracts |
| 16-sanitizer-configure | `cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DOW_SANITIZE=ON -DOW_ENABLE_IPO=OFF -DOW_BOOST_ROOT=B` | 0 | configured |
| 17-sanitizer-build | `cmake --build build-sanitize --parallel 2` | 0 | built |
| 18-sanitizer | same sanitizer CTest command as final gate | 0 | 6/6 suites, 210 contracts |
| 19-release-details | same Release CTest command as final gate | 0 | 6/6 suites, 210 contracts |
| 28-diff-check | `git diff --check` | 2 | 1 trailing blank line, corrected before final check |
| 30-diff-check | `git diff --check` | 0 | clean tracked-source diff |
| 31-staged-whitespace | `git diff --cached --check` | 2 | 4 CTest log trailing-space lines, trimmed before commit |

Every above log has a `.log` extension. Logs 05 and 08 retain their initial
filenames despite recording intermediate failures; their exit lines are the
source of truth. Final portable/sanitizer gates were rerun after PCI-keyed
accounting changed. The final Python-only change added an explicit original-port
443 check/docstring and removed an extra blank line; its 21 cases were rerun.

## Checklist requiring a real two-GPU Windows machine

Only perform these later in a separately authorized isolated normal-user test
workspace, without changing the running worker or production:

- [ ] MSVC + Windows SDK + locked Boost compile succeeds; run all registered native
      CTest suites (10 with existing Python, 9 without) and Windows PowerShell 5.1
      setup/consent/integrity/atomic-publication suites. Record exits and counts.
- [ ] Compare `Get-PnpDevice -Class Display` with the isolated fresh probe. The
      3070 and 3050 retain the two complete PCI instance IDs from the evidence;
      Meta `ROOT\DISPLAY\0000` has an explicit software exclusion. Unmappable
      DXGI aliases remain withheld. Distinguish raw display inventory from two
      schedulable GPU cards after successful per-card/codec probes.
- [ ] Verify CUDA ordinal/LUID mapping and per-child PID/LUID telemetry on each
      PCI card; encode and decode H.264 and HEVC with the approved FFmpeg. Record
      driver/build, profile limits, sample resolution and live VRAM evidence.
- [ ] Run two isolated jobs concurrently within CPU/RAM/scratch/VRAM limits. Full
      child-launch logs must show one distinct PCI card for each job, plus encoder
      ordinal/PID. Confirm both outputs decode, preserve geometry and complete.
- [ ] A third job cannot open a third worker NVENC session; another codec cannot
      bypass the one-session-per-card reservation. Check explicit session/VRAM
      exclusion reasons and the guarded CPU retry if a real encoder fails.
- [ ] Cancel or expire one job. Capacity remains owned until its full child tree
      drains; the other card/job continues. Pause rejects reserved BEGINs; closing
      and reopening the separate viewer leaves controller/jobs alive.
- [ ] Test live per-card VRAM pressure, adapter/driver identity drift and targeted
      codec failure. No stale/unmapped/ROOT profile becomes capacity; failed or
      cancelled GPU output never uploads, and valid CPU fallback still works.
- [ ] After the matching redistribution bundle is available and publication is
      completed, verify anonymous release download with an empty cache, exact
      pinned hash/size, explicit license consent, denied consent, interrupted/
      corrupt transfer cleanup, offline reuse and real Doctor decode/probes in
      a separately authorized fresh Windows profile. Do not infer installation
      readiness from the local Linux file hash.

Committed gate logs normalize trailing whitespace only; command output content,
counts and exit lines are retained.
