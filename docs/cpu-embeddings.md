# CPU embeddings and fast Windows updates

This is a review candidate. MSVC compilation, Windows PowerShell 5.1 execution,
normal-user upgrade/rollback and an actual laptop job remain required before a
Windows release. Linux CPU probes do not certify Windows or laptop performance.

The tray controller keeps one paired identity, journal and machine budget.
Conversion runs FFmpeg; inference runs a separate short-lived llama.cpp child
inside Windows Job Objects. No Python runtime, GPU service or public inference
listener is installed. Local HTTP binds only 127.0.0.1 on an ephemeral port with
a random per-process API key, no proxy and no redirects. The coordinator issues
bounded signed storage transfers and does not execute model work inline.

## Setup and repair

Exit through the tray, extract the reviewed ZIP, then run `setup.cmd` as your
normal signed-in Windows user. Same-host setup retains identity, journal,
preferences, approved storage hosts and resource limits. `-NoInference` disables
the model configuration; `-SourceBuild` explicitly selects compilation.

The native llama.cpp ZIP imports MSVCP140, VCRUNTIME140 and VCRUNTIME140_1.
Setup checks these in System32 and offers the hash-pinned Microsoft 14.44.35211
redistributable only if missing, with valid Microsoft Authenticode signature,
explicit license consent (`-AcceptRuntimeLicense` for reviewed unattended setup),
UAC limited to the dependency installer and mandatory DLL readback. It does not
assume Visual Studio has supplied them. A required reboot is reported.

Model and runtime downloads use exact SHA-256 locks, per-user files, progress,
timeouts and bounded retry. Damaged runtime extraction gets one reconstruction
and sample-probe retry. Unsupported CPU/runtime failures leave inference
unavailable while verified conversion can run; no unverified capability is
advertised. With little free memory, model probes defer. Text can verify at
768 MB; media waits for 2048 MB. The worker reserves probes in the same budget
as jobs, so probing cannot secretly consume conversion's reserved capacity.

One H.264 conversion (512 MB) plus text needs 1280 MB in the shared budget;
conversion plus media needs 2560 MB. Actual free memory also gates admission.
Installed RAM is not a promise of current availability. Existing limits are
retained; adjust `worker.json` only after draining/exiting if a deliberate higher
budget is wanted. Idle connections do not print repeating activity lines.

## Prebuilt ZIP path

A binary ZIP contains scripts/config/setup.cmd and a reviewed `bundle` manifest
with the three worker executables. Setup verifies all binary hashes, skips
compiler/SDK discovery and Boost, installs a new version directory, runs doctor
on the target machine and publishes the stable pointer/config/launcher together.
A failed launch/startup check stops the owned candidate and restores those
files. Credentials, journal and results stay intact. If safe candidate shutdown
fails, the recovery backup remains on disk rather than being discarded.

Build from reviewed source once on native Windows, run all mandatory CTest
contracts, then create a binary ZIP using the installed CMake's CTest:

```powershell
.\scripts\package-windows.ps1 -Build "$env:LOCALAPPDATA\OrganizerWorker\build-windows-x64" -Destination "$env:USERPROFILE\Downloads\worker-client-0.1.3-binary-candidate" -Revision '<reviewed-full-source-commit>'
```

Packaging rejects a missing contract suite or any native failure. The revision
is operator-supplied provenance; hashes are integrity checks, not a code-signing
claim. Native runtime/job acceptance is still required before calling it a
release. An ordinary GitHub source archive has no binaries and therefore still
runs the explicit source build. Source cache verification uses a managed stream
hash instead of allocating a 1 MiB PowerShell array per header, and reports the
rejected relative file/category before bounded repair.

## Pinned model

Profile: `embeddinggemma2-q8-b11475-768-v1`, official ggml-org Q8_0 weights and
projector from revision `bfcd298762cc34d0357ece5ebdd31791a3a374d8`, llama.cpp
b11475 CPU x64 runtime. The reviewed URLs/checksums are in dependencies.lock.json.
Q8_0 was selected because the upstream artifacts provide exact published hashes.
No automatic latest-model upgrade occurs. A future profile must not be mixed
into this embedding space without deliberate reindexing.

Text, images, sampled video and bounded audio yield normalized 768-dimensional
vectors. Original assets and full-size conversion outputs are retained. This
extension stores embeddings; generated titles/descriptions and a semantic-search
UI are separate features.
