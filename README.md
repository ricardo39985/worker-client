# Organizer Worker — first implementation

Native C++20 Windows compute worker, with a system-tray controller and a separate
live console. This package is source, not a verified Windows binary release.

**Verified here:** 50 portable behavior contracts on Linux, including an
AddressSanitizer/UndefinedBehaviorSanitizer run. **Not verified here:** Windows
compilation, PowerShell execution, tray interaction, native process containment,
FFmpeg operation, pairing, and live Lightsail integration. Four additional native
Windows tests are included but have not run. Read [status](docs/status.md).

## First action on the Windows laptop

Extract this archive into a normal writable folder, open PowerShell in that
folder, and run:

```powershell
.\setup.cmd -CheckOnly
```

This reports Windows build, CPU, memory, GPU inventory, disk, native runtime
components, optional SSH, MSVC, CMake and Windows SDK availability. It does not
install anything, launch the worker, or change SSH/firewall settings. It only
writes its diagnostic report to `verification\windows-setup.json`.
Exit 0 means the inventory passed; exit 2 means build prerequisites are missing;
exit 1 means a failed check. A passing inventory is not a successful runtime test.

## Build and install

No Git, Python, Node, Docker, WSL, CUDA or GPU is required by the source build.
Windows x64 build 19041 or newer, PowerShell 5.1, an appropriate Windows SDK,
Visual Studio 2022 C++ Build Tools >=17.10 and CMake >=3.25 are required and
checked. See [setup details](docs/setup.md).

When build tools are missing, the explicit administrator bootstrap is:

```powershell
.\setup.cmd -InstallMissing -AcceptToolLicenses
```

Run that only after accepting Microsoft's Build Tools license. It verifies the
Microsoft installer signature, requests no automatic restart, and stops if a
reboot is needed. It never creates the worker identity under Administrator.
Return to a **normal, non-elevated PowerShell** to build and install:

```powershell
.\setup.cmd
```

The script obtains the SHA-256-pinned Boost archive, builds native release
executables, runs portable and Windows contract tests, installs into a versioned
per-user directory, and runs the executable doctor. Every command exit status is
checked. A missing or failed converter is marked disabled, not silently enabled.

Once those checks actually pass on Windows, launch from the signed-in desktop:

```powershell
& "$env:LOCALAPPDATA\OrganizerWorker\start-worker.ps1"
```

The terminal is a log viewer. Close it to leave the controller running in the
tray. The tray has console reopening, pause/resume, machine priority, video/image
preferences, logs, and explicit drain-or-stop exit. OS shutdown, sign-out and
crashes can still end a process; this is not an invisible always-on service.

## Pairing and capabilities

The coordinator defaults to **unconfigured**. Production Organizer has not been
modified by this package. Its pairing endpoints, approval CLI and job gateway
must first be implemented against [worker-v1](contracts/worker-v1.md). Earlier
example `organizer-admin` commands are proposed interfaces, not installed tools.

The client contains HTTPS pairing, a persisted protected identity, an outbound
WebSocket connection, offer/BEGIN/renewal/cancellation handling, and a durable
result outbox. It does not listen on public ports or use SSH for job transport.

This first version includes typed CPU H.264 MP4 and JPEG conversion adapters.
FFmpeg must be supplied as an explicitly approved executable plus its SHA-256,
and real sample conversions must pass before capabilities are advertised.
Inference, audio conversion, GPU acceleration and model downloads are **not yet
implemented**. Their eventual adapters must use the same job/lease contract;
no model family is assumed to be available or compatible.

## Repository map

`src/` and `include/ow/`: portable admission, protocol, resource and journal code.
`windows/`: native tray, console, networking, crypto, process trees and worker.
`tests/`: behavior tests, with additional native Windows contracts.
`scripts/`: self-checking setup, launcher and local test commands.
`contracts/`: coordinator integration contract, not a second server.
`docs/`: decisions, setup, release gates and remaining implementation work.
`verification/`: actual recorded local test output.

No production deployment, remote GitHub repository, Windows installation or
pairing was performed during preparation of this package. The intended separate
remote repository is `ricardo39985/organizer-worker`; its creation is still needed.
