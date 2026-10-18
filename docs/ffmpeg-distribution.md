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
The executable at `/tmp/ffmpeg.exe` was independently hashed on October 17,
2026: its size and SHA-256 match the pin above. Its embedded version string is
`n9.0.2-22-g46d8f462ee-20261001`; it has not been executed on Windows here.

`url` and `redistribution_url` are deliberately `null` until the operator
publishes. This is a blocked publication, not a working download URL or a release
candidate ready for fresh-machine conversion. The operator has authorized a
GitHub release asset on `ricardo39985/worker-client`, but the matching complete
included-dependency source/notices bundle remains unavailable. Exact FFmpeg
source and the BtbN recipe revision have been recovered; recipe references alone
do not establish complete corresponding source/notices. See
[the current gate ledger](../verification/worker-next/REPORT.md). A healthy configured converter still works;
`-FfmpegPath` plus its approved hash and explicit `-NoConversion` remain available.

## Operator publication procedure

1. Select an operator-controlled HTTPS/443 artifact origin that supports public
   unauthenticated GET, valid TLS and no expiration. A direct HTTP 200 origin is
   accepted. The explicitly approved GitHub alternative is the permanent release
   asset URL below: the read-only Python gate allows exactly one HTTPS/443
   redirect from that digest-tag URL to `release-assets.githubusercontent.com`,
   requiring a final HTTP 200 and verifying the unchanged byte count and hash.
   It rejects other redirect origins/hosts, downgrades, credentials and extra
   hops; signed CDN transport queries are neither persisted nor logged.
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

   For the authorized GitHub release alternative, create a dedicated release
   tagged with the complete SHA-256 and upload the unchanged asset `ffmpeg.exe`:

   ```text
   https://github.com/ricardo39985/worker-client/releases/download/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/ffmpeg.exe
   ```

   Use a source/notices index asset on that same release for `redistribution_url`.
   This is a proposed publication path, not evidence that an asset exists.
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
   final 200 and permits only the GitHub release/CDN exception above; the existing Windows transfer transport
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

## Physical GPU correction

The Windows device-tree evidence identifies two physical PCI instances (RTX 3070
and RTX 3050) and a `ROOT\DISPLAY\0000` software adapter. This branch now uses
present Display-class PnP instance identity through Windows APIs; ROOT adapters
and unproven mappings are withheld. The existing encoder-session budget remains
binding. See `verification/worker-next/REPORT.md` and `docs/architecture.md`.
This source policy and portable fixtures do not establish physical Windows
execution. An NVENC allocation failure remains a failed probe; a session limit,
external encoder use or other driver/resource state may cause one. The device
tree alone does not establish the cause of a particular failed allocation.

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
