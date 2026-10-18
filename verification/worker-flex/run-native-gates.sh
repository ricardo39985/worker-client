#!/usr/bin/env bash
# DO NOT RUN without explicit operator approval. This writes to the named PC.
set -euo pipefail
cd "$(dirname "$0")/../.."
case "${1:-}" in
  danny) host=danny@192.168.1.189; user=danny ;;
  reald) host=reald@192.168.1.10; user=reald ;;
  *) echo 'Usage: bash verification/worker-flex/run-native-gates.sh danny|reald' >&2; exit 2 ;;
esac
archive=.cache/flex/source.tar.gz
sha=dbd521ead6bd39a8d59af2f841a0795a5a7f68ad4b507be56cb49e0184f0f8e0
script=verification/worker-flex/native-gates.ps1
script_sha=8df53ffab027ca9de23edd24bd95eedca20b73a047f11542d3d346261105466e
printf '%s  %s\n' "$sha" "$archive" "$script_sha" "$script" | sha256sum --check
# No host-wide policy/environment changes, install, Doctor or tray launch.
ssh -o BatchMode=yes "$host" 'powershell.exe -NoProfile -Command "$p=Join-Path $env:LOCALAPPDATA '\''Temp\ow-worker-flex-20261018'\''; if(Test-Path -LiteralPath $p){throw '\''Candidate path already exists; refusing overwrite'\''}; New-Item -ItemType Directory -Path $p | Out-Null"'
destination="C:/Users/$user/AppData/Local/Temp/ow-worker-flex-20261018"
scp "$archive" "$script" "${host}:$destination/"
# Bypass applies only to this child PowerShell process, not machine/user policy.
ssh -o BatchMode=yes "$host" "powershell.exe -NoProfile -ExecutionPolicy Bypass -File $destination/native-gates.ps1 -ArchiveSha256 $sha" \
  | tee ".cache/flex/windows-$user.log"
# Preserve evidence even on failure. Cleanup is a separate reviewed command;
# never automatically delete a path that may still contain active test children.
