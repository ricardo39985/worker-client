# FFmpeg retention correction — local source evidence

Date: October 17, 2026. Base: `d1b9fea`. Branch:
`fix/retained-ffmpeg-dependency`. Local commit only; no push, merge, release,
upload, deployment, startup change, host connection or restart.

## Diagnosis and ordered work

1. Read the full supplied brief and the repository entry/architecture/protocol/
   testing/contribution/status documents. The toolchain absence fix (`45f4112`)
   and Boost long-path extraction correction (`d1b9fea`) are already in the base.
   They are not reverted, reimplemented or claimed newly Windows-verified here.
2. Root problem: upstream retention is shorter than the supported lifetime of
   the locked dependency. Integrity and availability are separate promises. A
   replacement daily autobuild would recreate this failure; `latest` would also
   change bytes. The old tag returned 404 during this pass. The current
   [BtbN policy](https://github.com/BtbN/FFmpeg-Builds#release-retention-policy)
   keeps 14 daily builds and two years of month-end builds. We do not depend on
   claims about version-tag availability or Gyan access to solve this problem.
3. Define offline publication/integrity contracts, capture the missing-gate red
   result, implement a read-only checker, then capture the old-ZIP-pin red result.
   Replace the download contract with the exact executable hash/size supplied by
   the brief. No binary or new FFmpeg version is obtained, run or bundled.
4. Add a small PowerShell converter owner using the existing verified atomic-file
   primitive, consent receipt and bounded HTTPS transport. Keep existing approved
   converter reuse, custom path/hash overrides, explicit NoConversion and mandatory
   native CTest/Doctor gates. Test failure never enables conversion or implicitly
   chooses pairing-only operation. No upstream fallback, PATH trust or TLS bypass.
5. Document the one-time retained-artifact publication and matching source/license
   review. Both publication URLs remain null; source policy can pass while the
   publication gate correctly fails. Nothing claims fresh automatic conversion
   works before the operator publishes.

See [the mechanism and exact operator steps](../../docs/ffmpeg-distribution.md).
The executable pin is
`f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`,
165,167,104 bytes. Its measured identity is supplied evidence, not a new local
hashing or native execution result. The old archive digest stays provenance only.

## Files

- `config/dependencies.lock.json`: direct-executable pin, size, retained-distribution
  intent, pending binary/source-notices URLs and old archive provenance.
- `scripts/setup-converter.ps1`, `scripts/setup.ps1`: explicit pending failure,
  reviewed lock validation, per-digest consent, hash-directory download, unique
  partial files, exact size/hash checks before atomic publication, three bounded
  attempts, approved existing-file reuse and unchanged functional gates.
- `scripts/check_ffmpeg_dependency.py`: read-only source/publication/integrity
  checks, optional local-file or actual HTTPS verification; no upload or execution.
- `tests/ffmpeg_dependency_test.py`, `tests/setup_converter_test.ps1`,
  `CMakeLists.txt`: offline portable distribution contracts and native converter
  contracts; Python tests register only if an interpreter is already available.
- `README.md`, `THIRD_PARTY.md`, `docs/setup.md`, `docs/status.md`, `docs/testing.md`,
  `docs/ffmpeg-distribution.md`: current blocker, operator steps and accurate gates.

## Isolation

The shared checkout started clean at `main`/`d1b9fea`. During this task, concurrent
GPU edits appeared, and another process switched it to `fix/physical-gpu-placement`.
Those edits and that checkout were preserved. The initial shared-tree portable
run captured their in-progress failures and is **not** a gate for this branch.

An independent source snapshot was created with `git archive HEAD` at `d1b9fea`
plus only this task's CMake, dependency, installer, checker, tests and documentation
files: `/tmp/ow-ffmpeg-only-d5te2z9n`. The accepted portable build uses that source.
The original task branch was then attached to the separate worktree
`/home/rick/Downloads/worker-client-ffmpeg-retained` for review and local commit.
No second task branch was created. No concurrent GPU edit is in this commit.
The four changed portable gate input files match the accepted snapshot byte for
byte (`10-tested-inputs.log`); subsequent native-only retry/line-ending edits are
explicitly unrun, like all other PowerShell changes here.

## Gate ledger

Commands below ran from the initial checkout unless a different working directory
is specified. Logs were redirected to `verification/ffmpeg-dependency/` and copied
into this branch's worktree. Counts distinguish initial setup/import errors,
contract outcomes and publication checks; none are native execution evidence.

| Command | Exit | Result / count | Evidence |
|---|---:|---|---|
| `python3 tests/ffmpeg_dependency_test.py` (before checker exists) | 1 | Expected missing-entrypoint red: 0 cases executed, 1 setup error; not a behavioral pass | `01-policy-red.log` |
| `python3 tests/ffmpeg_dependency_test.py` (checker present, old lock) | 1 | Expected old-pin red: 17 cases, 16 passed / 1 failed; ZIP pin is not the approved executable pin | `02-old-lock-red.log` |
| `python3 tests/ffmpeg_dependency_test.py` (new lock) | 0 | 17/17 offline cases passed | `03-policy-green.log` |
| `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` (shared checkout; excluded) | 8 | 4/6 suites passed; in-progress concurrent adaptive suite had 59 cases / 5 failures and progress suite 31 / 1 failure | `04-portable.log` |
| `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` (cwd `/tmp/ow-ffmpeg-only-d5te2z9n`) | 0 | Configure/build succeeded; 6/6 suites, 187/187 contracts passed: Python 17, core 75, pipeline 9, rendition 3, adaptive 53, progress 30 | `05-isolated-portable.log`, `05-isolated-contract-details.log` |
| `python3 scripts/check_ffmpeg_dependency.py --allow-unpublished` | 0 | 1 source-policy check passed; explicitly reports publication pending | `06-source-policy.log` |
| `python3 scripts/check_ffmpeg_dependency.py --download` | 1 | Expected publication block: 1 lock rejected before network, 0 artifact downloads; not a release pass | `07-publication-blocked.log` |
| Python `platform.system()` / `shutil.which` inventory | 0 | Linux; Python/CMake/CTest/C++ present; `powershell.exe`, `pwsh`, `cl.exe` absent | `08-prerequisites.log` |
| `git diff --check` (isolated branch worktree) | 0 | No tracked-diff whitespace errors | `09-diff-check.log` |
| `git diff --cached --check` (all task files staged) | 0 | No staged-diff whitespace errors, including new files | Empty output observed |
| Byte comparison/SHA-256 inventory of CMake, lock, checker and Python tests against accepted snapshot | 0 | 4/4 changed portable gate inputs identical | `10-tested-inputs.log` |

No additional sanitizer or media-smoke run was needed: this branch changes no
C++/media implementation. Existing C++ behavioral contracts ran to detect
integration regressions. The reviewed Boost source root already existed locally;
it was not installed or changed by this pass. Linux development compilation is
not proof of locked Windows toolchain behavior.

## Unrun gates and publication work

No PowerShell runtime, MSVC, Windows SDK or Windows kernel is available here.
The 16 new converter cases were authored before their implementation but neither
their red nor their green stage ran. Run on an authorized isolated real Windows
machine:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/setup_converter_test.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/setup_policy_test.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/setup_progress_test.ps1
```

Parse all scripts with Windows PowerShell 5.1, compile with MSVC and the locked
Boost as in `docs/testing.md`, and run every registered CTest suite. There are
nine native/PowerShell/core suites, or ten total when the optional installed
Python distribution suite registers. These counts describe registration, not
executed Windows passes.

The operator must seed the existing exact executable once into retained owned
HTTPS storage after matching-source/build/license review, publish source/notices,
set the two lock URLs, run the checker with `--artifact-file` and `--download`,
and test a fresh empty-cache normal-user Windows installation with real Doctor
encode/decode. Also test declined consent, corrupt/truncated transfer, cache reuse,
repair, old approved converter reuse and explicit NoConversion. Confirm failures
do not publish configuration/startup/launch. Availability/retention ownership and
license completeness cannot be proven by URL validation or unit tests.

## GPU ruling and deliberately unchanged scope

The duplicate RTX 3070 report is **unresolved physical identity**, not a proven
enumeration defect or a proven second card. Equal PCI model/configuration fields
with different LUIDs are insufficient for either conclusion. The Meta virtual
display is a hypothesis, not demonstrated causation. The unmapped entry's profile
must remain withheld. No deduplication by name/PCI model or guessed backend ordinal
is appropriate.

NVENC allocation errors are genuine failed functional attempts, but not enough
evidence to call a permanent capability/capacity limit or to blame an alias.
Free system RAM and nominal VRAM are not contemporaneous per-device allocatable
encoder-resource evidence. Passing synthetic NVENC results elsewhere on the host
do not validate this failed device/codec. Identity mapping, session/driver state,
actual memory pressure and process budgets require a separate Windows diagnostic
change with physical UUID/PCI-bus/LUID/ordinal correlation and controlled serial
per-device encode/decode/telemetry. Preserve the fail-closed mapping/probe rules.

No GPU implementation, resource limits, leases, reservation/drain semantics,
startup logic, network protocol, inference support or production integration was
changed here. Neither Windows PC nor the production server was contacted.
