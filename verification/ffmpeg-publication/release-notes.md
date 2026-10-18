Retained FFmpeg dependency: unchanged BtbN Windows x64 static GPL build
`ffmpeg-n9.0.2-22-g46d8f462ee-win64-gpl-9.0`.

The approved executable is supplied directly for the existing installer and in
`ffmpeg-n9.0.2-22-g46d8f462ee-win64-gpl-9.0-retained.zip` with notices/licenses.

- Executable SHA-256: `f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4`
- Executable size: **165167104 bytes**
- Exact FFmpeg source: `46d8f462eeb87ee1f704d8c44a0ee24fca471ad1`
- Exact BtbN recipes: `e88e49f624457c455700b058f0a84ca87d499cc2`

[Source/notices bundle](https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/ffmpeg-source-and-notices.tar.gz),
[NOTICE](https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/NOTICE.md), [rebuild instructions](https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/REBUILD.md),
[license texts](https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/licenses.zip), [checksums](https://github.com/ricardo39985/worker-client/releases/download/ffmpeg/f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4/SHA256SUMS).
The FFmpeg source and BtbN recipes are also individual release assets.

This GPLv3-or-later configuration enables GPL and version3 and disables fdk-aac.
No binary modification or rebuild was performed for this publication.
The attached source covers FFmpeg and BtbN recipes/patches. The recipes fetch
additional third-party dependency sources and build tools from upstream; their
source trees and individual notices are not vendored in this bundle. See NOTICE
for this concrete source-scope limitation. No independent certification of full
GPL compliance or byte-for-byte rebuild reproducibility is claimed.

This is a dependency retention release, not a Windows worker application release
or native acceptance result. The release tag targets the existing remote main
commit; no local worker branch was pushed. Retain these assets while supported
worker packages use this digest. Do not overwrite them or replace this pin with
a floating upstream build. Windows installation/probe gates remain separate.
