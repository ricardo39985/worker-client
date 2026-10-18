# Windows setup and acceptance

Run `setup.cmd` as a normal, signed-in Windows user. It asks for the coordinator
HTTPS origin when missing and defaults to both immediate verified launch and
interactive logon startup. There is no embedded production endpoint. Consult
README.md for the single-command flow and explicit maintenance/re-pair options.

The launcher verifies the installed worker hash, exact process image and process
creation time recorded in runtime.json. A stale PID alone cannot establish launch.
Its readiness marker means the controller started, not that pairing, converters,
models or job dispatch are ready. Closing the console does not exit the tray.
A real Windows sign-in is required; this is not a boot service running as SYSTEM.

The setup flow requests explicit Microsoft license acknowledgment before offering
elevation solely for missing build tools. Parent setup resumes as the original
normal user. It checks the installer signature and rechecks the toolchain; a
required reboot is reported rather than forced. No global execution policy, PATH,
firewall or SSH settings are changed. Existing pinned dependency/FFmpeg checks remain.

Setup holds the single-instance file lock while installing. Exit the worker's tray
first; existing jobs are not force-stopped. Registered CTest checks run before
publishing the installed version. Candidate runtime/configuration is checked in a
private staging directory before replacing the old config. Per-file configuration
replacement is atomic; the entire multi-file installation is not a transactional
rollback. Failures are reported, state is preserved or quarantined, and success is
not reported when startup registration/readback or process readiness fails.

For a new endpoint, explicit -RePair moves encrypted credentials, key, journal,
outbox and local job files into the old per-user pairing-backups scope before
publishing new configuration. A same-endpoint re-pair archives only credential and
pending enrollment, retains key/journal, and binds any legacy work using the old
protected origin first. Configuration is retained with full-scope backups. Do not
share those private backups or restore them into a different server scope.

## Version-only replacement of an existing paired worker

`scripts/update-worker.ps1 -Package <verified-directory> -PackageManifestSha256 <sha256>`
updates binaries without changing configuration, pairing or startup registration.
The operator must authorize stopping an idle worker and provide a Windows-gated
package. `package.json` records the full source revision and SHA256 of all three
worker executables; its hash is supplied separately from the transferred package.
No downloads, dependency changes or first-time setup occur in this path.

The updater stages and verifies a complete immutable version directory before
stopping the idle controller. It flushes the previous manifest to a recovery
file, then uses same-directory `File.Replace` with an explicit backup path.
Sharing/lock violations retry for at most ten seconds; other errors fail
immediately with the exact call, paths and HRESULT. Unknown processes such as
antivirus are never terminated. An operator-supplied holder callback is available
at the filesystem boundary; the normal updater stops only its installed worker.
The running executable is never overwritten.

The recovery copy remains until the selected process, readiness marker and fresh
connected/accepting status are verified through the existing limited interactive
startup task. A caught publication or launch failure restores the exact old
manifest and verifies the old version again. Power loss or forced installer
termination retains `current.rollback.json`; another update refuses to overwrite
it. Rerun `scripts/update-worker.ps1 -RecoverOnly` to restore and verify the old
version before attempting any further installation. If filesystem permissions or
a persistent holder prevent rollback, the recovery copy is retained and the
failure is explicit. Do not delete the live manifest or recovery copy to force
an update through. Old version directories remain available for rollback.

The registered Windows-only `setup_swap_contracts` exercises the PowerShell 5.1
null-backup regression, a real sharing violation, bounded exhaustion, failed
launch, and a killed installer after publication, including execution of the
restored fixture binary. Linux tests do not establish these Windows guarantees.

## Required native acceptance (not executed here)

Build the source with the locked Windows toolchain; run worker_contracts,
windows_contracts, setup_contracts, progress_contracts and setup_progress_contracts. Verify the first-run prompt, interrupted
setup recovery, Microsoft-tools consent/reboot path, failed config publication,
normal/elevated session handling, scheduled-task principal/settings, one active
worker, process-readiness detection, tray close/exit, sign-out/logon/reboot, and
endpoint changes with retained pending results. Test DPAPI reopen and P-256 proof
against the actual separate Lightsail gateway through Caddy TLS. Confirm revocation
and lost responses, and verify no old journal is transmitted after an endpoint
change. Only then call this a Windows release or a real paired-machine test.


## 0.1.2 repair and conversion additions

Rerun the same `setup.cmd` after a recoverable failure. Downloads are pinned and
checksum-verified; damaged download caches are replaced. Extracted dependency
files carry a full-file manifest; altered, missing or extra files trigger fresh
extraction. Failed extraction preserves the previous dependency directory.
Build failures receive one fresh generated-build retry; failing behavior tests
are never skipped. Converter probe failures receive one pinned verification
retry unless an explicit external converter was supplied. Startup failures use
SID identity verification and a normal-user Startup shortcut fallback. Persistent
errors stop with the failing step and write both source and installed reports.

The retained FFmpeg executable replaces the expiring upstream ZIP. Its download
is publication blocked until an operator sets the permanent binary and matching
source/notices URLs in the reviewed lock. See
[FFmpeg distribution](ffmpeg-distribution.md) for exact steps and the Windows
acceptance checklist. Setup checks its size and SHA-256 before local publication;
real conversion probes are still mandatory. Healthy approved existing converters
are retained. Forced repair verifies the same pinned executable. Downloads retain the maximum of three
checksum-verified attempts; persistent failures require rerunning setup.

For unattended operation, after reviewing the relevant terms and hosts:

```powershell
.\setup.cmd -Server https://your-coordinator.example -NonInteractive -InstallMissing -AcceptToolLicenses -AcceptConversionLicense -AcceptStorageHosts
```

`-AcceptStorageHosts` accepts the exact list advertised over the selected
coordinator's verified HTTPS connection. To pin it explicitly instead, use
`-StorageHosts storage.example`. `-NoConversion` deliberately leaves conversion
disabled. Existing externally approved FFmpeg remains usable with
`-FfmpegPath C:\approved\ffmpeg.exe -FfmpegSha256 <approved-executable-hash>`.
Setup records CPU, RAM, disk and DXGI adapter inventory before converter checks,
including with `-NoConversion`; that mode saves inventory and a disabled-capability
reason without hashing or probing FFmpeg. The per-machine `worker.json` limit
`limits.vram_mb` is the optional global VRAM ceiling for adaptive GPU profiles.
When absent, the worker chooses a conservative aggregate ceiling from detected
adapters. A positive value sets an explicit ceiling; `0` disables GPU profiles.
CPU, RAM, scratch and job limits continue to bound converter probes and runtime work.

Expired pending identity prompts for PAIR; unreadable or cross-endpoint state
prompts for ARCHIVE. Both require explicit action and retained backups. Automatic
repairs affect generated dependencies/builds/startup; they do not silently delete
keys, journals or result outboxes, override operator limits, or change coordinators.
The x64/build-19041 minimum and real Windows OS component requirements remain;
unsupported OS/architecture needs a compatible release, not an unsafe DLL download.

## Memory availability

Fresh installations default to `reserve_ram_mb: 0`: job placement uses the
physical memory Windows currently reports as available, without subtracting an
additional fixed OS reserve. Each offer still needs its declared memory budget,
and the worker's configured budget and active reservations remain enforced.
Existing configurations are retained. To change an installed reserve, exit the
tray worker, back up worker.json, set limits.reserve_ram_mb to the desired value,
and restart the installed worker. Its paired identity is retained.

## Progress feedback

Each setup operation announces its stage and completion or failure. Downloads
and archive checksums report actual bytes; manifests report checked file counts.
Commands stream stdout/stderr while running, with status at roughly two-second
intervals. Extraction/compilation cannot reliably report a total, so they show
elapsed time and output-line activity without a percentage. Thirty seconds
without measured activity displays a quiet warning; this is not proof of a hang.
Interactive prompts explicitly say WAITING FOR INPUT. GUI Microsoft installers
show an elapsed wait and direct the user to their installer/UAC window.

Setup's private `logs/setup-progress.log` contains the same sanitized feedback.
It retains a current and previous file, each normally bounded to about 5 MiB.
Signed URLs, authorization fields and common secret fields are redacted. Doctor
JSON is captured for validation, not dumped into the terminal.

A dependency transfer stops after 90 seconds without bytes, or 30 minutes
overall; the existing three-attempt checksum-verified retry remains. Response
headers have a 90-second deadline. Native stages have explicit deadlines:
Boost/FFmpeg extraction 30 minutes, configure 10 minutes, compile 60 minutes,
CTest 15 minutes, installation 2 minutes, runtime/sample probe 3 minutes and
process readiness 20 seconds. A timed-out setup-owned native process tree is
stopped and reported failed. The GUI/elevated installer wait is bounded to two
hours but does not forcibly terminate a system installation; inspect its window
before rerunning. No quiet warning skips tests or changes a pinned hash.

The worker console also shows DOWNLOAD / checksum / CONVERT / UPLOAD / result
persistence / cleanup stages, with job IDs and roughly two-second active-job
updates. Transfer and checksum percentages use exact file totals. Conversion
uses FFmpeg's reported output timeline position against its parsed source
duration; encoded bytes are never used as a percentage. The live viewer redraws
one stable ASCII row per active job, with elapsed and derived remaining time,
while `worker.log` stays timestamped plain text. Redirected output, missing
console handles and `NO_COLOR` disable both color and redraw. Converter probes
and other stages without a measurable extent state that once and then stay
quiet. Connection activity is shown only while jobs or acknowledgments remain.
An open connection or a 100% upload is not proof that a result was accepted:
confirm RESULT ACKNOWLEDGED and coordinator `state:done`.

For acceptance, run all three PowerShell suites first from the source folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\setup_policy_test.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\setup_progress_test.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\setup_converter_test.ps1
```

Then run setup's mandatory native CTest/doctor checks. Observe a real download,
cache reuse, quiet extraction, native failure/timeout and normal-user launch,
then a small real conversion through the coordinator. Do not replace source
files while an older setup is running. An existing paired identity is retained
when rerunning against the same endpoint without `-RePair`.

## Existing-tree permission propagation

The per-user root ACL remains restricted to the signed-in user and SYSTEM.
Windows can propagate its inheritable ACL across an existing dependency/build
cache even when icacls is invoked only for the root. That stage now has a bounded
15-minute deadline with the existing elapsed/activity feedback. A reported
30-second timeout on the operator's populated installation stopped setup before
compilation; no successful Windows run is claimed for the revised deadline yet.
Re-running against the same coordinator retains credentials, journals, settings
and verified caches. Permission failures and deadline expiry still stop setup.
