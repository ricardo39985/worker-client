# Permission request — isolated Windows native gates only

**Not executed.** SSH has remained read-only. These are proposed commands for
operator approval, not standing authorization for later measurements/installation.

## Exact commands requested

Run from `/home/rick/Downloads/worker-client-flex`, **Danny first**, inspect the
result, then Rick:

```bash
bash verification/worker-flex/run-native-gates.sh danny
bash verification/worker-flex/run-native-gates.sh reald
```

The complete wrapper is beside this document for review. It executes only:

1. Verify local source archive and native gate script hashes.
2. SSH `New-Item` to a new, otherwise nonexistent
   `%LOCALAPPDATA%\Temp\ow-worker-flex-20261018` on the selected PC; refuse reuse.
3. SCP `.cache/flex/source.tar.gz` and `native-gates.ps1` into that directory.
4. SSH:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:/Users/danny/AppData/Local/Temp/ow-worker-flex-20261018/native-gates.ps1 -ArchiveSha256 dbd521ead6bd39a8d59af2f841a0795a5a7f68ad4b507be56cb49e0184f0f8e0
```

For Rick the exact path is `C:/Users/reald/AppData/Local/Temp/ow-worker-flex-20261018/native-gates.ps1`
with the same arguments. ExecutionPolicy Bypass is process-local; no machine or
user execution policy is set. Local stdout is retained in
`.cache/flex/windows-danny.log` / `windows-reald.log`.

Archive: **119 source files, 284,979 bytes**; SHA-256:
`dbd521ead6bd39a8d59af2f841a0795a5a7f68ad4b507be56cb49e0184f0f8e0`.
Individual source hashes are in `proposed-source-manifest.json`.
Native script SHA-256:
`8df53ffab027ca9de23edd24bd95eedca20b73a047f11542d3d346261105466e`.
Any source/script change requires a fresh archive/hash and permission review.

## Reason / effects / boundaries

This is necessary to compile the changed controller with actual MSVC and run all
registered native suites. Linux cannot prove Windows correctness. It reuses the
existing reviewed Boost 1.89.0 and VS2022/CMake installations, builds with
`--parallel 1`, discovers suites and runs each, preserving numeric exits/JUnit.
The script refuses if FFmpeg is already running. Test-generated files, source,
binaries, transcript and process-local TEMP/TMP/module cache live below the
single candidate directory. Tests exercise synthetic child processes, DPAPI,
file persistence and mocked setup/transport; no real accounts/media are used.

The script fingerprints `current.json`, `worker.json`, `credential.dpapi` and
`pairing-key.dpapi` before/after, without copying secret contents. It does not
run setup, install, launch the tray/Doctor, restart/pause the worker, edit worker
settings, change startup/PATH globally, download dependencies or call production.
Compilation/tests still consume resources; this approval is specifically for
that bounded gate activity, **not** authorization to saturate either desktop.
The proposed PowerShell gate itself is unrun until approved.

## Exact cleanup

Preserve the local console log and inspect failures first. Once the gate has
returned and no candidate children/build tools remain, run this read-check and
removal on each approved host (shown for Danny; replace only the SSH host with
`reald@192.168.1.10` for Rick):

```bash
ssh -o BatchMode=yes danny@192.168.1.189 'powershell.exe -NoProfile -Command "$p=Join-Path $env:LOCALAPPDATA '\''Temp\ow-worker-flex-20261018'\''; $busy=@(Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -ne $PID -and ($_.ExecutablePath -like ($p+'\''\*'\'') -or $_.CommandLine -like ('\''*'\''+$p+'\''*'\'') -or $_.Name -in @('\''cmake.exe'\'','\''ctest.exe'\'','\''MSBuild.exe'\'','\''cl.exe'\'','\''link.exe'\'')) }); if($busy.Count){throw '\''Build/test processes remain; do not delete or kill blindly'\''}; Remove-Item -LiteralPath $p -Recurse -Force; Write-Output ('\''STAGING_EXISTS='\''+(Test-Path -LiteralPath $p))"'
```

This deletes only the candidate directory, never the installed worker, its job
journal or existing dependencies. A remaining process is a permission stop:
inspect it and ask before any termination command. No automatic cleanup hides
a failed gate or deletes files under active children.

## Not included — still need separate commands/approval

- Approved-FFmpeg synthetic encodes / native media readback.
- Per-machine sustainable CPU/card/session/VRAM/RAM/disk capacity sweeps,
  foreground-interaction measurements, before/after throughput/quality trials.
- Any increase/change to installed resource ceilings or runtime settings.
- Installing the candidate, restarting it, deploying coordinator changes,
  committing a release, pushing, merging or publishing.

Those experiments need a separate, reviewed executable/script with explicit
workloads, run durations, resource bounds, cancellation/drain and cleanup.
No unmeasured 90% policy or peak-throughput claim is being substituted for them.
