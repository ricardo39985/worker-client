# Setup and operations

## Inspect without changing the machine

Extract all files into a writable folder. Keep the config and scripts directories
beside setup.cmd; copying just the cmd file will not work.

```powershell
.\setup.cmd -CheckOnly
```

The script checks native x64 Windows/PowerShell, OS build, CPU, memory, disk,
Windows runtime DLL presence, compiler, CMake and SDK; GPU and SSH are inventory,
not requirements. It writes verification/windows-setup.json in the source tree.
It does not assume winget, Git, Python, CUDA, FFmpeg, WSL or a model exists.

The report is local and may include your username/path and hardware names.
Inspect it before sharing. It does not include worker credentials or Windows
product keys. This script does not configure SSH, open firewall ports, change
router settings, stop unrelated software or alter machine execution policy.

## Missing Microsoft build tools

After accepting the Microsoft Build Tools license, run from administrator
PowerShell:

```powershell
.\setup.cmd -InstallMissing -AcceptToolLicenses
```

The installer is fetched from Microsoft's VS2022 channel and its Authenticode
signature must be valid with Microsoft Corporation as signer. The script asks
for the C++ workload, recommended SDK components and CMake integration. Downloads
or installer failures stop setup. A required reboot is reported, never forced.
The administrator stage finishes without creating user credentials.

Return to a normal user's PowerShell and rerun `setup.cmd`. Source builds download
the locked Boost archive and check its SHA-256 before extraction. Existing tools
are probed; stale cached downloads with a different hash are rejected. This is
not a fully offline build unless the verified dependencies are already cached.

The source directory is trusted executable input: review it and don't share its
write access with untrusted users. The release archive contains no external
executables, private keys or model files.

## Build and installation gates

The script builds the controller, console and doctor with MSVC for x64, then
runs all CTest contracts. Installation is versioned under:

```
%LOCALAPPDATA%\OrganizerWorker\
  versions\0.1.0-<build-hash>\
  current.json
  worker.json
  credential.dpapi             (only after actual approval)
  pairing-key.dpapi            (only when pairing starts)
  journal.sqlite3
  worker.log
  jobs\
  logs\
```

The root ACL is restricted to the current user and SYSTEM by setup. It refuses
to install/update while a controller holds the instance lock. Use tray Exit
first; no force-kill updater exists. Failed builds cannot replace current.json.
Configuration is preserved unless explicitly overridden. A new configuration is
validated by the runtime doctor before the current installed version is switched;
review or restore worker.json if configuration validation itself fails.

The initial worker starts unconfigured and does no production work. No external
service address or user's hardware limits are assumed. Default budgets use
conservative detected RAM/CPU ceilings, with separate OS RAM headroom. GPU is
not enabled based on its name or advertised memory.

## Adding an approved converter

Obtain an x64 FFmpeg build from an approved distribution. Check the origin and
publisher checksum through an independent trusted channel; merely hashing a
random download is not approval. The worker executes no FFmpeg supplied by a
network job. The file stays at the path you explicitly provide; don't move it.

```powershell
# Replace these values with the exact approved local executable and checksum.
.\setup.cmd -FfmpegPath 'C:\ApprovedTools\ffmpeg.exe' -FfmpegSha256 '<64 hexadecimal characters>'
```

The setup and startup probes check the digest and run small real CPU conversions.
Missing libx264/MJPEG support, DLL failures, corrupt binaries and failed probes
keep the affected capabilities disabled. No hardware encoder, Vulkan, CUDA,
ROCm or inference support is assumed. This package does not download FFmpeg or
models automatically because no tested, approved artifacts for those adapters
have been locked yet.

## Coordinator configuration — only after the server is implemented

The client contract is specified in contracts/worker-v1.md. Those paths are not
created in production by this package. Do not point it at a working Organizer
server and interpret a 404 as a Windows networking failure.

When a reviewed staging gateway is deployed, run setup with its real HTTPS origin
and the exact staging object-storage hosts, e.g. from PowerShell:

```powershell
# Illustrative values only. Do not run these example hosts as real endpoints.
& .\scripts\setup.ps1 -Server 'https://worker-api.example.test' -StorageHosts @('objects.example.test')
```

The client checks protocol v1, generates a key, prints an expiring pairing code,
and waits for an authorized Lightsail operator to approve that exact request.
The permanent credential is protected under the same Windows user. A credential
cannot silently switch to a different coordinator. An explicit 401/403 stops
reconnection and requires operator intervention; there is no auto-repair bypass.

## Launch and optional startup

Launch from the signed-in desktop:

```powershell
& "$env:LOCALAPPDATA\OrganizerWorker\start-worker.ps1"
```

To opt in to launch after this user signs in:

```powershell
.\setup.cmd -StartAtLogon
```

The logon task runs at limited privileges in the interactive session. It is not
an always-on service before sign-in. Closing the log viewer leaves jobs running.
Tray Exit is required for normal manual shutdown. Sleep suspends the machine;
leases can expire and be retried elsewhere. Do not assume logon startup disables
sleep, changes battery behavior or keeps a laptop awake.

SSH can update/build under the same account. Starting via SSH requests an existing
interactive logon task when possible; it cannot display a tray in the SSH service
session or automatically sign a user into Windows. Finish the first local launch
on the Windows laptop itself.

## Failure diagnosis

Inventory: verification/windows-setup.json. Runtime doctor: installed
OrganizerWorkerDoctor.exe --json --probe-ffmpeg. Coordinator readiness:
OrganizerWorkerDoctor.exe --json --check-coordinator. Logs: the per-user root and
logs subdirectory. A green doctor confirms only the checks it actually ran; it is
not an end-to-end production completion or a benchmark.

Retain the entire state directory for upgrades. Never copy credential.dpapi to a
second machine/account as its identity. Pair the second machine independently.
Revoking a PC is a coordinator operation; deleting files locally is not revocation.
