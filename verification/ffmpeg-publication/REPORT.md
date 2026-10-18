# Retained FFmpeg publication — October 17, 2026

Published with the operator's explicit authorization on `fix/worker-next`.
Both lock URLs are set; the previously failing public-download gate now exits 0.

Release:
https://github.com/ricardo39985/worker-client/releases/tag/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4

## Published artifacts and scope

The release carries 12 assets: the unchanged raw executable; a redistributable
binary ZIP with notices/licenses; the exact FFmpeg source archive; the exact
BtbN build-recipe archive; a combined source/notices bundle; NOTICE; rebuild
instructions; embedded configure options; dependency-recipe source references;
license texts ZIP; manifest; and SHA256SUMS.

FFmpeg revision: `46d8f462eeb87ee1f704d8c44a0ee24fca471ad1`.
BtbN recipe revision: `e88e49f624457c455700b058f0a84ca87d499cc2`, independently
confirmed from successful upstream action run `36860751422`. Both exact-commit
archives were freshly downloaded and matched the previously recovered files.
Today's BtbN master and the FFmpeg `n9.0.2` tag are not substituted for the
historical recipe/source revisions. The tag alone omits 22 subsequent commits.

The combined bundle contains both source archives, all four FFmpeg GPL/LGPL
license texts, FFmpeg's licensing description, BtbN's MIT license, full embedded
configure flags, NOTICE, rebuild procedure and dependency recipe inventory.
The binary ZIP contains the identical approved executable and these notices.

**Source completeness limit:** additional statically linked dependency sources,
submodules and their individual notices are fetched by the attached recipes and
are not vendored here. The published NOTICE and release body state this plainly.
This operation establishes publication of the requested recovered materials and
download integrity; it does not independently certify full GPL compliance for
the entire binary. No written offer was invented. The documented reconstruction
has not been executed; historical container digests/build logs were unavailable,
and upstream base/toolchain inputs float. Byte-for-byte reproducibility remains
unverified. See [NOTICE](../../docs/ffmpeg/NOTICE.md) and
[REBUILD](../../docs/ffmpeg/REBUILD.md).

## Pin and public download evidence

`09-published-download.log` records the existing checker against the actual new
lock URL, anonymously and with verified HTTPS:

```text
PASS FFmpeg dependency policy and artifact integrity
exit=0
```

`10-hosted-assets.log` records independent HTTP 200 downloads, hash/size checks
against `manifest.json`, 12/12 internal bundle checksum checks, ZIP CRC checks
and equality of the ZIP executable to the downloaded raw executable:

```text
ffmpeg.exe
bytes=165167104
sha256=f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4
MATCH

ffmpeg-source-and-notices.tar.gz
bytes=17354442
sha256=2c548177e23b942235bed6d4dd7b49324e09f3fb5c3e7b9cd7cf6df0a62119eb
MATCH

ffmpeg-n9.0.2-22-g46d8f462ee-win64-gpl-9.0-retained.zip
bytes=63978318
sha256=f8c93fba344f9d3e65f085bcce58566b60697bae877d15feff707ba690247bd9
MATCH; contained executable matches the approved pin
```

The executable's existing pin applies to raw and extracted executable bytes,
not to either new archive. Bundle/archive hashes are recorded separately.
`11-hosted-metadata.log` verifies the hosted manifest and SHA256SUMS exactly
match local records, completing anonymous verification of all 12 release assets.
`release.json` records the public non-draft release and uploaded asset sizes.
No authenticated download, execution, TLS exception or signed CDN URL recording
was used. `verify_assets.py` reproduces the 10 payload downloads and bundle/ZIP
checks using only permanent asset URLs in the committed manifest.

## GitHub tag correction and unchanged installer

GitHub rejected publication of the initial draft with HTTP 422:
`branch or tag names consisting of 40 or 64 hex characters are not allowed`.
No public release existed at that stage. The same draft was renamed to
`ffmpeg/<full SHA-256>`; notices/manifests/archives were corrected before
publication. Draft-only replaced assets are not additional public releases.

This namespace preserves the exact full-checksum path component required by
both existing lock validators. The Python checker needed one narrow redirect
regex extension for the literal `ffmpeg/` namespace. Its new behavior test
failed first, then passed. Arbitrary namespaces, truncated digests, hyphenated
tags, extra redirects, foreign CDN hosts, TLS downgrades and credentials remain
rejected by the relevant contracts. The full raw checksum remains mandatory.

Only `url` and `redistribution_url` changed in the lock. The approved SHA-256,
byte count, executable format, version, license metadata, Boost/compiler pins
and provenance are identical to HEAD before this task. All PowerShell installer,
consent/receipt, cache, atomic-file and probe code is unchanged. The existing
Windows validator accepts the complete digest directory without modification.
`08-lock-boundaries.log` records those change-boundary checks.

## Gates

All commands ran in this checkout on Linux. Logs are in this directory.

| Gate | Exit | Result / evidence |
|---|---:|---|
| Initial `python3 scripts/check_ffmpeg_dependency.py --download` | 1 | Expected pending-publication red; 01-download-red.log |
| Initial distribution contracts | 0 | 21/21; 02-policy.log |
| Initial `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 0 | 6/6 suites; 03-portable.log |
| `python3 scripts/check_ffmpeg_dependency.py --artifact-file /tmp/ffmpeg.exe` | 0 | Exact local approved bytes; 04-local-artifact.log |
| Namespace behavior red | 1 | 23 cases, expected single redirect error; 05-tag-redirect-red.log |
| Namespace behavior green | 0 | 23/23; 06-tag-redirect-green.log |
| Final `./scripts/test.sh -DOW_BOOST_ROOT=/tmp/ow-boost.mxOndo/boost_1_89_0` | 0 | 6/6 suites, 212 contracts; 07-final-portable.log |
| Lock/installer/pin boundary check | 0 | Only two URLs changed; 08-lock-boundaries.log |
| Published `python3 scripts/check_ffmpeg_dependency.py --download` | 0 | Actual anonymous HTTPS/integrity pass; 09-published-download.log |
| `python3 verification/ffmpeg-publication/verify_assets.py` | 0 | 10/10 payloads, 12/12 bundle checks, ZIP pin equality; 10-hosted-assets.log |
| Hosted manifest/checksums comparison | 0 | 2/2; 11-hosted-metadata.log |
| Staged whitespace check | 0 | 12-staged-check.log |

The 212 contracts comprise Python distribution 23, core 75, pipeline 9,
rendition 3, adaptive 70 and progress/console 32. Existing C++ implementation
is unchanged. No extra sanitizer or media smoke was needed for this
publication/URL-policy change. The already-present locked Boost source was
used without installation or upgrade.

Windows PowerShell/MSVC/native tests, fresh empty-cache setup, actual FFmpeg
execution/Doctor, GPU probes and rebuild execution remain unrun. No Windows
machine or production system was contacted. Linux contracts and successful
downloads do not establish native installation/runtime acceptance.

## Authorized boundary

The dependency release tag points to existing remote main commit
`d1b9fea970a1aed9ad0885a1bb5b982c2946f863`; remote main stayed at that commit.
The local worker changes were committed only on `fix/worker-next`, without a git
push. The release was explicitly marked `latest=false` so a dependency release
does not replace the application release selection. No repository visibility or
workflow setting was changed. No merge, worker release, deployment, installation,
startup action, host restart or Windows/production access occurred.

The operator owns retention of this digest namespace and its backup. Keep the
source/notices and binary assets available together for supported worker pins.
