# Windows gate corrections — October 17, 2026

Scope: `fix/worker-next`, isolated build access to `reald@192.168.1.10` only.
No push, merge, release, deployment, installation, startup/configuration change,
running-worker restart, or production access. The installed-version/process
mismatch mentioned in the brief was left alone. Existing MSVC/CMake and the
reviewed Boost 1.89.0 tree were reused without installing or upgrading anything.
PowerShell execution-policy overrides apply only to the invoked build/test
processes; no machine or user policy was changed.

## Inventory decision: wording changed; reporting was retained

The baseline native test was rebuilt with a diagnostic printing the actual
`probe_ffmpeg` reason. On this PC it contains exactly:

```text
converter not configured: capabilities disabled; display inventory entries=3; configured_global_VRAM_override_MB=not_set
```

The same reason lists:

| Adapter | Physical identity | Dedicated VRAM | Mapping / exclusion |
|---|---|---:|---|
| Radeon RX Vega M GL Graphics | `PCI\VEN_1002&DEV_694E&SUBSYS_080D1028&REV_C0\4&DFEC518&0&0008` | 4043 MiB | encoder mapping verified |
| Intel(R) HD Graphics 630 | `PCI\VEN_8086&DEV_591B&SUBSYS_080D1028&REV_04\3&11583659&0&10` | 128 MiB | encoder mapping verified |
| Microsoft Basic Render Driver | empty/unmapped | 0 MiB | unsupported or unmappable display adapter; GPU profile withheld |

Verified mapping is inventory evidence only. NoConversion produces no profiles
and enables no converter capabilities. The old assertion required the obsolete
`DXGI inventory entries=` label. The PCI change deliberately includes present
PnP-only displays, so `display inventory entries=` accurately describes this
larger diagnostic inventory. Production reporting did not need restoration.

The updated contract checks the exact label and observed numeric count, every
adapter's full reason entry, persisted identity/name/driver/VRAM/mapping/exclusion
fields, empty profiles and all disabled converter capabilities. It independently
enumerates present Display-class instances and requires each in saved/reported
inventory. Any ROOT instance must have the exact software-adapter exclusion and
unverified encoder mapping. Existing unconditional adaptive contracts exercise
ROOT exclusion even with successful encoder evidence, two separate PCI cards
plus a ROOT alias, and rejection of saved ROOT profiles. No ROOT-enumerated
display or NVIDIA card was present on this PC; no physical ROOT/NVIDIA or
RTX 3070/3050 test is claimed.

## Converter decision: fix recursive callback lookup

The first offline converter case never reached its injected file-copy transport.
In Windows PowerShell 5.1, the wrapper passed to `Get-WorkerVerifiedFile` resolves
`$Download` dynamically to that helper's own `$Download` parameter. It calls
itself recursively instead of the caller's transport. This is CPU recursion,
not a slow download, blocking read or input prompt.

A bounded diagnostic on the PC printed `CALLBACK_DEPTH=3 TRANSPORT_IS_SELF=True`
and consecutive wrapper frames above `Get-WorkerVerifiedFile` and
`Install-WorkerLockedConverter`; see `converter-trace.log`. That diagnostic
deliberately stops recursion by throwing and is not a passing contract suite.

The installer now retains the supplied transport as `$converterTransport`,
which the wrapper invokes. Consent, reviewed URL, exact byte count, checksum,
partial-file cleanup, atomic publication, receipts and all three retry attempts
remain enforced. The suite adds corrupt/corrupt/good transport recovery and
asserts three attempts on exhaustion, correct URL/private partial paths,
verified publication and cleanup. Every transport is injected filesystem work
or a deliberate failure; the synthetic executable is never run. Unexpected
`Read-Host` calls fail, and CTest launches this suite with `-NonInteractive`.
The registered timeout remains **60 seconds**, with real retry backoffs retained.

## Execution evidence

The source was shipped as a Git bundle and built in
`C:\Users\reald\worker-gates-20261017\{source,build}`. The helper records
`$LASTEXITCODE` immediately after each native command, before any output or
exception can replace it. Final suites run individually through CTest, so each
suite has its own numeric `CTEST_SUITE_EXIT` marker. All registered suites are
discovered through CTest JSON; none are omitted.

Verified source commit: `181be4e42dbcdd242300be30241e3d7424a9d43f`.
The subsequent evidence commit adds documentation, captured logs and the
build-only gate helper; tested application and contract-test sources are unchanged.

| Suite | Linux passed / failed | Linux CTest exit | Windows passed / failed | Windows CTest exit |
|---|---:|---:|---:|---:|
| ffmpeg_dependency_contracts | 23 / 0 | 0 | not registered (no Python interpreter found) | — |
| worker_contracts | 75 / 0 | 0 | 75 / 0 | 0 |
| pipeline_contracts | 9 / 0 | 0 | 9 / 0 | 0 |
| rendition_contracts | 3 / 0 | 0 | 3 / 0 | 0 |
| adaptive_hardware_contracts | 70 / 0 | 0 | 70 / 0 | 0 |
| progress_contracts | 51 / 0 | 0 | 51 / 0 | 0 |
| windows_contracts | not applicable | — | 12 / 0 | 0 |
| setup_contracts | not registered | — | 25 / 0 | 0 |
| setup_progress_contracts | not registered | — | 15 / 0 | 0 |
| setup_converter_contracts | not registered | — | 17 / 0 | 0 |
| Total | **231 / 0; 6/6 suites** | **0** | **277 / 0; 9/9 suites** | **each 0; helper exit 0** |

The converter suite took **26.83 seconds**, including actual 2s/4s retry
backoffs for checksum rejection, truncation, transport exhaustion and successful
repair. No timeout was raised and no integrity, consent or safety check removed.
Windows configure/build exited 0/0. The Linux `scripts/test.sh` configure, build
and combined CTest run exited 0; individual numeric CTest exits were also saved.
The Linux check reused `/tmp/ow-boost.mxOndo/boost_1_89_0`.

| Red/diagnostic run | Result | Numeric exit | Evidence |
|---|---|---:|---|
| Baseline MSVC configure/build | success / success | 0 / 0 | `windows-red.log` |
| Baseline native inventory suite | 11 passed, 1 failed (obsolete DXGI label) | combined CTest 8 | `windows-red.log` |
| Converter suite before transport fix | timeout at 60.02s in first case; 0 cases completed | combined CTest 8 | `windows-red.log` |
| Bounded recursive-wrapper diagnostic | expected diagnostic throw; one case printed FAIL | harness 0; not a green suite | `converter-trace.log` |

The red CTest invocation selected both failing suites: **0/2 suites passed,
CTest exit 8**. It ran the original inventory assertion with a reason-printing
diagnostic, plus the strengthened converter test file before implementation.
`windows-red.log` ends at the saved CTest exit marker. An initial post-gate
diagnostic helper failed due to an uninitialized diagnostic counter; that
discarded diagnostic established no behavior. `converter-trace.log` is the
separate corrected, bounded trace and preserves its deliberately thrown error.
The original untrimmed helper transcript remains at
`/tmp/worker-gates-diagnostic.log` on the authoring machine.

Reproduction and logs:

- `run-windows-gate.ps1`: exact final build/CTest helper. Ship a bundle as
  `C:\Users\reald\worker-gates-final.bundle`, then invoke the helper using
  `powershell -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File
  C:\Users\reald\worker-gates-final.ps1 -Expected <source-commit>`.
- `windows-green.log`: final checkout verification, configure/build and all
  nine individual native CTest runs with numeric exits and contract counts.
- `windows-red.log`: executed failing gates and actual baseline reason string.
- `converter-trace.log`: bounded recursive call stack.
- `linux-gate.log`: `scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0`.
- `linux-suites.log`: six individual verbose CTest runs with numeric exits.

Captured logs normalize trailing whitespace only; the red log's documented
post-gate diagnostic portion is omitted. No protected configuration, credentials
or signed URLs were read or recorded.

This verifies the isolated build and behavioral suites only. No installer,
real approved-FFmpeg conversion, tray/UI, startup, coordinator interoperability,
production job, physical ROOT display or NVIDIA/two-card acceptance was run.
No installed worker, selected version, running worker process, account, firewall,
SSH setting, host PATH or execution policy was changed.
