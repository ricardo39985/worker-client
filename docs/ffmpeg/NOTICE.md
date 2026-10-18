# Retained FFmpeg binary and source

Published by ricardo39985 for Organizer Worker on October 17, 2026.
FFmpeg is copyright its respective authors. BtbN's build scripts are copyright
their respective authors and distributed under the attached MIT license.
No license for Organizer Worker's own source is granted by this notice.

## Unchanged binary

- Upstream build: `ffmpeg-n9.0.2-22-g46d8f462ee-win64-gpl-9.0`
- Embedded version: `n9.0.2-22-g46d8f462ee-20261001`
- Platform/variant: Windows x86-64, static GPL, release/9.0 branch.
- File: `ffmpeg.exe`, 165167104 bytes.
- SHA-256: `f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`.
- Original archive SHA-256:
  `74827e097445a136b803febd4dcf80f7cba7f14a4c612fb80f7b5e5cedd3ffbe`.

The executable was recovered from the operator's existing approved artifact.
We repackaged it without modifying or rebuilding it. The original upstream
autobuild archive is no longer available. The new ZIP is a redistribution of
`ffmpeg.exe` with notices, not a reconstruction of that original ZIP; its archive
checksum is therefore different. The separate raw executable asset supports the
existing installer without changing its pin, format or license-consent behavior.

## Build identity and licensing

FFmpeg source revision:
`46d8f462eeb87ee1f704d8c44a0ee24fca471ad1`.
The attached `FFmpeg-46d8f462eeb87ee1f704d8c44a0ee24fca471ad1.tar.gz`
contains this revision, including its licensing files. The `n9.0.2` tag alone
does not match this binary: the version identifies 22 subsequent commits.

BtbN recipe revision:
`e88e49f624457c455700b058f0a84ca87d499cc2`.
The attached `FFmpeg-Builds-e88e49f624457c455700b058f0a84ca87d499cc2.tar.gz`
includes the recipes, patches, Dockerfiles, toolchain configuration and workflow.
This is the revision reported by the successful upstream build run:
https://github.com/BtbN/FFmpeg-Builds/actions/runs/36860751422

The workflow builds the `win64` / `gpl 9.0` image and then runs
`./build.sh win64 gpl 9.0`. It cross-compiles with MinGW-w64 and statically links
the enabled libraries. The binary contains the compiler identification
`GCC: (crosstool-NG 1.29.0.7_b1a94f6) 16.2.0`.
`configuration.txt` records its complete embedded configure options, including
`--enable-gpl --enable-version3`, `--disable-libfdk-aac` and
`--extra-version=20261001`. This configuration selects GPL version 3 or later;
see `licenses/FFmpeg-LICENSE.md` and `licenses/COPYING.GPLv3`.
All four FFmpeg GPL/LGPL texts are retained with the source for its component
licensing, along with BtbN's MIT text. There is no warranty; the full license
texts govern copying, modification and redistribution.

## Source access and rebuilding

Binary, source, recipes, license texts, this NOTICE and `REBUILD.md` are hosted
together without a source-request charge on this retained release:
https://github.com/ricardo39985/worker-client/releases/tag/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4

`ffmpeg-source-and-notices.tar.gz` groups the exact FFmpeg source archive,
exact BtbN recipe archive, licenses, configuration and rebuild instructions.
`dependency-source-references.json` identifies source revisions in those recipes,
including secondary repositories. It is a recipe inventory, not a list of
libraries proven linked into the binary. The recipes download third-party
dependency sources, submodules and build tools from upstream; those additional
source trees and their individual notices are not vendored in this bundle.
Consequently this publication is not represented as a complete offline mirror
of every statically linked dependency's corresponding source, nor as independent
certification of GPL compliance for the entire binary. The operator authorized
publication of these recovered materials together with the approved binary.

Follow `REBUILD.md` to compile the attached FFmpeg source using the attached
recipes. The original build logs and historical container image digests are not
available. Some toolchain/base-image inputs in the original recipes float.
The documented reconstruction has not been executed, and byte-for-byte
reproducibility of the approved executable is not claimed. These limits must
remain visible when redistributing this package.

Keep the release assets together, retain their published names/content, and keep
the source accessible for as long as this binary is distributed. Do not replace
files under this digest tag with different builds. Future dependency changes
require a new review, pin and source publication.
