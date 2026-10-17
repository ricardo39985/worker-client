# Retained FFmpeg dependency

The conversion bootstrap needs both reviewed bytes and a source that retains
them for the lifetime of supported worker packages. A checksum establishes
integrity; it cannot keep an upstream release alive. BtbN keeps 14 daily builds
and month-end builds for two years, and its `latest` asset floats. Neither is a
permanent dependency source. The old October 1 archive/tag is gone (HTTP 404).
See the [upstream retention policy](https://github.com/BtbN/FFmpeg-Builds#release-retention-policy).

## Decision and current boundary

Keep the exact already-approved static Windows x64 executable, not a new build:

```text
version: n9.0.2-22-g46d8f462ee
bytes:   165167104
SHA-256: f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4
```

`config/dependencies.lock.json` now pins the **executable** rather than its old
ZIP. `format: exe` makes that distinction explicit. The deleted ZIP URL/digest
remain under `provenance` for audit only; setup never fetches that location.
The binary identity and size come from the operator's measured brief; the binary
is not present in this authoring environment and has not been independently
hashed or executed here. Its stated version also requires native confirmation.

`url` and `redistribution_url` are deliberately `null` until the operator
publishes. This is a blocked publication, not a working download URL or a release
candidate ready for fresh-machine conversion. No endpoint, credentials or upload
is invented by the source change. A healthy configured converter still works;
`-FfmpegPath` plus its approved hash and explicit `-NoConversion` remain available.

## Operator publication procedure (not performed by this change)

1. Select an operator-controlled HTTPS/443 artifact origin that supports public
   unauthenticated GET, valid TLS, direct HTTP 200 responses and no expiration.
   Use a dedicated dependency bucket/origin or retained release namespace, not
   the production worker service or a signed temporary URL. Set retention to
   exclude these objects from lifecycle deletion; deny overwriting/deleting the
   digest namespace, retain an independent backup and assign an owner. Keep every
   artifact referenced by supported source packages, even after upgrades.
2. Obtain the approved executable **once** from the existing approved offline
   artifact/cache through an authorized operator process. Verify the exact size
   and SHA-256 above. This initial seeding is required; no client-side fix can
   recreate missing bytes from a hash. Do not copy binaries from PC to PC for
   subsequent installs. If this file cannot be recovered, stop: a different build
   requires a separate version/hash/license review and fresh Windows probes.
3. Before redistribution, review this exact build's GPL configuration, included
   libraries and notices. Retain matching corresponding source, patches and build
   instructions for FFmpeg and included dependencies, and license/attribution
   texts. Publish them with a permanent explanatory index on the same retained
   origin. Link that index in `redistribution_url`. The generic FFmpeg legal page
   is guidance, not proof that this exact build is redistributable or that source
   is available. If matching source/notices cannot be established, publication
   remains blocked. See [FFmpeg's legal information](https://ffmpeg.org/legal.html).
4. Upload the unchanged executable to this exact path structure, substituting
   the real origin (this example is intentionally not a working endpoint):

   ```text
   https://<owned-artifact-origin>/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/ffmpeg.exe
   ```

   Never replace the content at an existing hash path. Publish source/notices
   first, then the binary. Nothing here uploads or makes this repository public.
5. Change only the two null URLs in the reviewed lock to the permanent artifact
   and source/notices-index URLs. Keep format, version, byte count and SHA-256.
   Before approving the URL change, verify local bytes and then anonymously fetch
   the published bytes on the review machine:

   ```sh
   python3 scripts/check_ffmpeg_dependency.py --artifact-file /path/to/approved/ffmpeg.exe
   python3 scripts/check_ffmpeg_dependency.py --download
   python3 tests/ffmpeg_dependency_test.py
   ```

   The checker never uploads, edits the lock or executes the binary. It rejects
   expiring/`latest` upstream paths, missing full hash directories, signed query
   URLs, unsafe TLS locations and changed bytes. The online gate requires a
   direct 200 and refuses redirects; the existing Windows transfer transport
   still permits its bounded HTTPS-only redirects. URL shape cannot prove
   ownership, retention or license completeness: those require operator review.
6. Run the real Windows checks below before distributing an approved worker
   package. Repeat the read-only `--download` availability/integrity gate during
   each future release review; add monitoring only through separately authorized
   infrastructure work. The manual-only CI policy is unchanged.

No publication has occurred in this branch. Fresh-machine automatic conversion
remains blocked until steps 1–6 are completed. Thereafter ordinary `setup.cmd`
needs no per-machine file relay or `-FfmpegPath` override.

## Setup mechanism

`scripts/setup-converter.ps1` owns converter selection and installation. A valid
existing stored path/hash is rehashed and retained; an explicit NoConversion
selection returns empty paths. Otherwise the lock is validated before consent
or network activity. A pending publication throws an actionable error; it never
silently chooses pairing-only mode.

After license consent (or a receipt matching this executable's digest), setup
downloads to a unique partial file under the per-user hash directory. It checks
exact byte count and SHA-256 before an atomic same-directory move to `ffmpeg.exe`.
Mismatch or interrupted transfer removes the partial file and never creates a
success receipt. Damaged generated files are replaced; healthy pinned files are
reused. The configured approval stays the reviewed digest, not a digest inferred
from an arbitrary new download. Transfers and integrity checks retain the existing maximum of three attempts
with bounded backoff; persistent failures stop setup. A pending publication fails
before this loop. No PATH lookup or upstream fallback is allowed.

`scripts/setup.ps1` uses that selection, retains the explicit override path and
runs the existing native CTest and real sample-conversion Doctor before publishing
configuration, startup or launch. Integrity success alone enables no capability.
Forced probe repair revalidates the locked executable; a healthy file will be
reused, not replaced with another build. Existing external overrides retain their
current operator approval semantics.

## Separate GPU finding

This dependency branch does not change adapter enumeration, device mapping,
session admission, VRAM budgets, tuning or encoder commands. The reported two RTX
3070 DXGI entries have distinct LUIDs; their equal PCI vendor/device/subsystem
values identify a hardware model/configuration, not a unique physical card. A
virtual-display alias is plausible, but the Meta driver is not proof of causation.
There is no evidence here to deduplicate them, nor to claim there are two physical
3070s. The mapping failure is evidence that that adapter is **unproven**; withholding
its profile is correct.

NVENC `CreateBitstreamBuffer`/`CreateInputBuffer` failures are real unsuccessful
functional trials for those attempts. Free system RAM and nominal 8 GiB VRAM do
not show free allocatable encoder/device resources at the time. Driver state,
other sessions, device mapping, per-process budgets and actual memory pressure
remain possibilities, not established causes. These errors do not establish a
permanent card capability limit or prove an enumeration defect caused them.
`encoders=cpu,nvenc` and synthetic fps mean some profiles passed, not that every
adapter/codec/concurrency combination did. Keep all failed/unmapped profiles off.

Resolve that independently on a real, explicitly authorized Windows test host:
correlate DXGI LUIDs with CUDA ordinals/LUIDs, NVIDIA physical UUID/PCI bus IDs and
Windows PnP device identity; capture driver/runtime/build details and concurrent
encoder use; then run isolated serial encode/decode trials per mapped card/codec
with child PID/LUID telemetry and recorded resource limits. Compare controlled
virtual-display states only with operator approval. Add a separate regression/fix
once evidence identifies the failure. Do not deduplicate on name or PCI model ID,
guess an ordinal, suppress the failure, or relax mapping to make NVENC appear on.

## Windows and publication gates

- Parse all PowerShell with Windows PowerShell 5.1; run
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/setup_converter_test.ps1`
  (16 new filesystem/consent/download contracts), plus both existing setup suites.
- Compile with MSVC/Windows SDK and the locked Boost; run every registered CTest
  suite. Python distribution contracts register when Python is already installed;
  no Python installation is needed for setup or native converter contracts.
- With isolated test data, exercise missing-publication failure, denied consent,
  fresh retained download, corrupt/truncated bytes, offline reuse, damaged-file
  repair, existing approved override and NoConversion. Verify failed setup never
  publishes candidate worker configuration or registers/launches a worker.
- After publication, use a fresh normal-user Windows profile with an empty cache
  to install from the real retained URL and hash the installed executable. Confirm
  `-version`/build configuration and dependency completeness, Doctor encode/decode,
  CPU capability reporting and fail-closed GPU behavior on available hardware.
  A raw static executable with matching bytes must still prove it runs on the
  supported Windows baseline. Native tests and a real NVIDIA probe remain open.

Linux policy and core tests cannot establish any of those Windows outcomes or
source/license completeness. Exact local commands and exits are in
`verification/ffmpeg-dependency/REPORT.md`.
