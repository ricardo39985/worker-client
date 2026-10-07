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
are never skipped. Converter probe failures receive one pinned re-extraction
retry unless an explicit external converter was supplied. Startup failures use
SID identity verification and a normal-user Startup shortcut fallback. Persistent
errors stop with the failing step and write both source and installed reports.

For unattended operation, after reviewing the relevant terms and hosts:

```powershell
.\setup.cmd -Server https://your-coordinator.example -NonInteractive -InstallMissing -AcceptToolLicenses -AcceptConversionLicense -AcceptStorageHosts
```

`-AcceptStorageHosts` accepts the exact list advertised over the selected
coordinator's verified HTTPS connection. To pin it explicitly instead, use
`-StorageHosts storage.example`. `-NoConversion` deliberately leaves conversion
disabled. Existing externally approved FFmpeg remains usable with
`-FfmpegPath C:\approved\ffmpeg.exe -FfmpegSha256 <approved-executable-hash>`.

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
shows measured output bytes and elapsed time; a media duration is not assumed.
Converter probes and pairing waits also print elapsed status while awaiting
local sample processes or coordinator approval. Connection status reports active jobs and durable results awaiting ACK every
ten seconds. An open connection or a 100% upload is not proof that a result
was accepted: confirm RESULT ACKNOWLEDGED and coordinator `state:done`.

For acceptance, run both PowerShell suites first from the source folder:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\setup_policy_test.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\setup_progress_test.ps1
```

Then run setup's mandatory native CTest/doctor checks. Observe a real download,
cache reuse, quiet extraction, native failure/timeout and normal-user launch,
then a small real conversion through the coordinator. Do not replace source
files while an older setup is running. An existing paired identity is retained
when rerunning against the same endpoint without `-RePair`.
