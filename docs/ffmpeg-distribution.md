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

The operator authorized publication of the retained binary with the recovered
source and build materials on October 17, 2026. Both lock URLs now point at the
[retained release](https://github.com/ricardo39985/worker-client/releases/tag/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4):

- `url`: the unchanged raw `ffmpeg.exe`, preserving `format: exe` and the pin.
- `redistribution_url`: `ffmpeg-source-and-notices.tar.gz`, containing the exact
  FFmpeg source, exact BtbN recipes/patches, licenses, configure flags and rebuild
  instructions. The release also carries individual source/recipe archives,
  license texts, NOTICE, checksums and a redistributable binary ZIP.

See [NOTICE](ffmpeg/NOTICE.md), [rebuild instructions](ffmpeg/REBUILD.md) and
[the publication evidence](../verification/ffmpeg-publication/REPORT.md).
The attached FFmpeg revision is
`46d8f462eeb87ee1f704d8c44a0ee24fca471ad1`; the `n9.0.2` tag alone is not the
matching source. BtbN recipes are retained at
`e88e49f624457c455700b058f0a84ca87d499cc2`, the successful upstream run's revision,
instead of today's moving `master`.

The recipes fetch additional third-party dependency sources/submodules and build
tools. Those source trees and individual notices are not vendored in the bundle.
The NOTICE explicitly records this source-scope limitation; publication and a
successful integrity gate do not independently certify complete GPL compliance.
The reconstruction has not been executed and does not establish byte-for-byte
reproducibility of the original toolchain environment.

## Retention and verification

Retain all assets under the full executable-digest tag for every supported
worker package that refers to it. Do not replace files under this tag or prune
this dependency release with daily autobuilds. Keep the source and binary
available together and preserve an independent backup. No repository visibility,
CI trigger, production storage or machine settings change is needed.

```sh
python3 scripts/check_ffmpeg_dependency.py --artifact-file /path/to/approved/ffmpeg.exe
python3 scripts/check_ffmpeg_dependency.py --download
python3 tests/ffmpeg_dependency_test.py
```

The checker validates the permanent HTTPS/443 URL, exactly one allowed GitHub
release-to-CDN redirect, final HTTP 200, byte count and SHA-256. It never executes
the downloaded binary. Signed CDN queries are transient and never recorded.
The source/notices bundle is separately downloaded and verified against the
release manifest and its internal source checksums in the publication evidence.
The checker and unchanged consent behavior still reject a missing/partial lock,
changed bytes, unsafe URLs and expiring upstream replacements. The existing
Windows transport retains its bounded HTTPS-only redirects.

Repeat the anonymous download/integrity checks at future release reviews. The
Windows setup/consent/native/Doctor gates below remain separate; this publication
is not a compiled worker release or a fresh Windows installation test.

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
