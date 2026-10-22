# Manual code-artifact transport review

Reviewed2026-10-21. No automatic latest upgrade, push trigger, release/deployment,
production secret, PC installation or new GitHub contents-write permission.
`config/dependencies.lock.json.ci` records both immutable action pins.

Existing checkout pin remains11bd71901bbe5b1630ceea73d27597364c9af683 (v4.2.2).
The only added CI transport is official `actions/upload-artifact` v4.6.2 at
**ea165f8d65b6e75b540449e92b4886f43607fa02**. GitHub's official refs API returned
that tag directly as that commit (not a guessed/moving latest). MIT license,
Copyright2018 GitHub Inc/contributors; upstream action retains its license.

Primary reviewed files, all from that exact official commit:

- `action.yml`: Node20 runner-provided runtime, `dist/upload/index.js` entry;
  explicit retention, hidden-file and overwrite controls.
- `src/upload/upload-artifact.ts`, input/schema and shared upload/search paths:
  no deletion unless `overwrite=true`; transport uses platform artifact SDK.
  Shared upload emits a normal authenticated GitHub artifact URL/digest/id,
  not transient signed CDN query strings.
- Search follows symlinks by default. Therefore caller must not rely on hidden
  exclusion as a secret boundary: our producer rejects reparse source/output,
  only copies seven fixed code names, final tree rejects reparse objects, and
  uploader receives FOURTEEN explicit code/manifest/source/notice/proof paths,
  not a profile/Temp/repository-root wildcard.
- Committed dependency lock Git blob75e253f2f531817d4f79b79653e5155d58c549ca
  (531417bytes), bundled executable Git blob89238fa3eb49937ea82c5d82006ee1fc6c6abaae
  (5051718bytes). The immutable official bundle is used; no npm install/update.

API source:
`https://api.github.com/repos/actions/upload-artifact/git/ref/tags/v4.6.2`
and official contents API at the exact commit. Retained local review text SHA256:

| File | SHA256 |
|---|---|
|action.yml|29b21270ff44638a165ea12abb4481796770c629e513217af7b4df1aa3821177|
|LICENSE|3e855ffa704114a51628ef8f0bf3aeb41728adf9d9070e263bf58aa5640b0eb5|
|upload-artifact.ts|3a8ccc15a820b4db83458cce1aa54de50702eb36c0e1f134d2a4718a089132ae|
|shared upload|bfab1f3c0e5ada3dd46e64addb583730dd84e17a47f06e24f9bda3cd50b9153b|
|shared search|4cbd489c239640073ee1bd17b9f941aabcf34cb7f41d68edd2264ee0438174e9|

Artifact publication is opt-in `package_worker=true` AND
`run_unattended_media=true`, after ALL groups succeed. Manual workflow remains
contents:read, persist-credentials:false. Retention3days, overwrite:false,
include-hidden-files:false, missing files are error. No release/tag/production
publish. Native CMake receives the exact full workflow SHA; receipt includes
source tree/archive SHA, package manifest and every code-file hash plus explicit
password-task/cold-boot/production-job/install **false** fields.

The artifact contains no FFmpeg executable, Windows password, user-DPAPI profile,
pairing key/credential, journal, runtime/configuration or generated job workspace.
Source ZIP contains committed reviewed source only. Boost BSL1.0 license and
THIRD_PARTY notice accompany code; the existing approved FFmpeg lock/consent is
unchanged. Packaging is byte copying/hashing only; it never executes a worker or
changes startup. Artifact retention is not a deployed or cold-boot-accepted PC.
