# RTX 3070 NVENC A/B: 512 MiB child commit limit is the trigger

Executed on `danny@192.168.1.189`, 2026-10-17 23:11–23:18 America/New_York (2026-10-18 03:11–03:18 UTC). This was one isolated synthetic investigation with the brief's numbered comparisons executed in order, followed by confirmation, diagnostics and output validation. No repository changes or branches were made.

## Answer and ranked explanation

1. **Confirmed trigger: the worker's 512 MiB Job Object aggregate commit limit.** The same 3070, driver, display topology, input and encoder settings succeed with the memory limit removed and fail when 512 MiB is applied. Both host H.264 and HEVC succeed under a bounded 768 MiB limit. Successful 3070 host runs peaked around 547–562 MiB job commit; the 3050 fits under 512 MiB (about 496 MiB H.264 and 474 MiB HEVC with the exact worker fixture). The failing 3070 jobs approach the 512 MiB ceiling. This is process/job commit pressure, not proof that its several GiB of free VRAM are exhausted. Small transient peak-accounting overshoots appear in Windows job statistics; no hard GPU-memory quota is claimed.
2. **Secondary crash: allocation failure reaches a corrupt-heap cleanup path.** `CreateInputBuffer` failure sometimes returns -12 cleanly. `CreateBitstreamBuffer` failures reproduced `0xc0000374`. A live first/second-chance exception stack reaches `ntdll!RtlFreeHeap` through NVIDIA `nvcuvid.dll` / `nvEncodeAPI64.dll`. This localizes where corruption is detected during failure handling; it does not establish which function first corrupts memory, or prove an NVIDIA defect independently of FFmpeg.
3. **Host-input path alone is not the cause.** CUDA device surfaces bypass the software input branch, and encoder initialization can succeed, but under 512 MiB the decoder later fails CUDA allocations and only 7/72 frames complete. Device input succeeds completely uncapped and at 768 MiB. It does not recover reliable 1080p operation while retaining this 512 MiB cap.
4. **Driver/context/display topology as the independent trigger is unsupported.** Nothing about those was changed between failure and success. The 3070's higher allocation footprint may depend on per-card driver/display state, but this experiment does not explain that difference. No driver reset, update, display/device experiment, hardware diagnosis or settings change was performed.

**Single change with the best expected payoff:** make the GPU task/profile RAM reservation and enforced Job Object commit budget reflect measured encoder needs, rather than forcing the 3070 into 512 MiB. **768 MiB is a tested bounded budget for these 1080p H.264/HEVC samples**, including host and device input. Admission accounting and the enforced child budget must agree. This is a proposed source change, not an applied change, and 768 MiB is not established as sufficient for arbitrary 4K, source formats, presets or concurrent work. Removing containment or leaving jobs unlimited is not the proposed fix.

## Setup and controls

- Read the complete operator brief before execution. Installed worker config names `C:\Users\danny\ffmpeg.exe`.
- Authoritative local `config/dependencies.lock.json`, installed config hash, installed executable hash and copied executable hash all agree: `f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4` (165167104-byte pin). The installed executable remained unchanged after testing. Reference lock is included in the local evidence folder.
- The only Windows directory created was `C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591`. FFmpeg, synthetic sources/outputs, helper source/assemblies, compiler temporary files and logs were confined there; `TEMP` and `TMP` were set only in the experiment processes. Existing .NET compilation facilities were used; no software was installed.
- Explicit NVIDIA selection: GPU 0 = RTX 3070, UUID `GPU-d040cefc-685c-8da3-12ea-c97ae41261fd`, PCI `01:00.0`; GPU 1 = RTX 3050, UUID `GPU-373452ee-1a66-c4f4-b0f0-e755c5a3db17`, PCI `05:00.0`. Host cases use `-gpu 0` or `-gpu 1`; verbose logs name the cards. Device cases additionally use `-init_hw_device cuda=ow:0 -hwaccel cuda -hwaccel_device ow -hwaccel_output_format cuda`, with no download filter; logs confirm `pixfmt:cuda` and encoder input frames context CUDA. Driver remained 617.42.
- All encodes were serial, synthetic, 3 seconds/72 frames at 24 fps, with AAC audio. Common worker encoder settings were `-preset p4 -rc vbr -cq 24 -b:v 0 -rc-lookahead 0`, one FFmpeg/filter thread, and host `-pix_fmt yuv420p`.
- Initial shared fixture was libx264 veryfast/CRF23 (B-frame source). Confirmation used the exact source-generation arguments in this checkout's worker: testsrc2 1920x1080/24, sine 880 Hz, libx264 ultrafast, 72 frames, AAC. The original worker was never asked to probe or execute a job.
- The launcher creates each test child suspended, assigns it to a new Job Object, then resumes it. `JOB_OBJECT_LIMIT_JOB_MEMORY` was toggled to isolate memory, with the worker's kill-on-close, die-on-unhandled-exception and normal-priority containment flags retained. No CPU hard cap was applied in any comparison, so this is a memory-factor experiment rather than a complete reproduction of worker CPU throttling. `JobMemoryLimit`, not `ProcessMemoryLimit`, matches the worker source. "none" means no explicit job memory cap, while process-tree containment remains.
- The initial launcher used process-local error mode 3 to avoid error dialogs; later diagnostics used mode 1 and captured the crash event. No machine error-reporting policy was changed.

## Numbered tests and confirmation results

All success rows below produced 72 frames and passed separate complete software decode with `-xerror`, exit 0. Failure outputs are not treated as usable results.

| Test | Cap MiB | Peak job commit MiB | Frames | Exit |
|---|---:|---:|---:|---|
| 01-3070-host-h264-unlimited | none | 561.83 | 72 | 0 (0x00000000) |
| 02-3050-host-h264-unlimited | none | 499.95 | 72 | 0 (0x00000000) |
| 03-3070-device-h264-unlimited | none | 559.48 | 72 | 0 (0x00000000) |
| 04a-3070-host-hevc-unlimited | none | 547.55 | 72 | 0 (0x00000000) |
| 04b-3070-device-hevc-unlimited | none | 546.22 | 72 | 0 (0x00000000) |
| 05a-3070-host-h264-cap512 | 512 | 513.12 | 0 | 4294967284 (0xfffffff4) |
| 05b-3070-host-h264-unlimited-repeat | none | 562.32 | 72 | 0 (0x00000000) |
| 05c-3050-host-h264-cap512 | 512 | 495.83 | 72 | 0 (0x00000000) |
| 05d-3070-device-h264-cap512 | 512 | 512.86 | 4 | 69 (0x00000045) |
| 05e-3070-host-hevc-cap512 | 512 | 512.49 | 0 | 4294967284 (0xfffffff4) |
| 05f-3070-device-hevc-cap512 | 512 | 513.24 | 7 | 69 (0x00000045) |
| 05g-3070-host-h264-cap768 | 768 | 560.64 | 72 | 0 (0x00000000) |
| 05h-3070-host-hevc-cap768 | 768 | 547.05 | 72 | 0 (0x00000000) |
| 07-exact-3070-host-h264-cap512 | 512 | 512.01 | none | 3221226356 (0xc0000374) |
| 07-exact-3070-host-h264-cap0 | none | 561.79 | 72 | 0 (0x00000000) |
| 07-exact-3070-host-hevc-cap512 | 512 | 512.22 | none | 3221226356 (0xc0000374) |
| 07-exact-3070-host-hevc-cap0 | none | 548.04 | 72 | 0 (0x00000000) |
| 09-exact-3070-host-h264-cap768 | 768 | 550.23 | 72 | 0 (0x00000000) |
| 09-exact-3070-device-h264-cap768 | 768 | 533.65 | 72 | 0 (0x00000000) |
| 09-exact-3050-host-h264-cap512 | 512 | 496.25 | 72 | 0 (0x00000000) |
| 09-exact-3070-host-hevc-cap768 | 768 | 547.67 | 72 | 0 (0x00000000) |
| 09-exact-3070-device-hevc-cap768 | 768 | 520.02 | 72 | 0 (0x00000000) |
| 09-exact-3050-host-hevc-cap512 | 512 | 474.02 | 72 | 0 (0x00000000) |

The first uncapped 3070 case did not fail: it already used more than 512 MiB commit. Applying the cap then reproduced failure, immediately removing it restored success, and a later bounded 768 MiB run also restored success. The 3050 control remained successful at 512 MiB. The exact-worker-source confirmation reproduced the heap-corruption exit under 512 MiB in both codecs; the allocation failure site is not fixed to a particular codec.

Key stderr excerpts:

`05a-3070-host-h264-cap512` (exit 0xfffffff4):

```text
[h264_nvenc @ 000001b4fade5b00] CreateInputBuffer failed: out of memory (10):
```

`05d-3070-device-h264-cap512` (exit 0x00000045):

```text
[CUDA @ 000001f63b5e68c0] cu->cuMemAlloc(&data, size) failed -> CUDA_ERROR_OUT_OF_MEMORY: out of memory
```

`05e-3070-host-hevc-cap512` (exit 0xfffffff4):

```text
[hevc_nvenc @ 000001dc658e3ec0] CreateInputBuffer failed: out of memory (10):
```

`05f-3070-device-hevc-cap512` (exit 0x00000045):

```text
[CUDA @ 0000014a697f2440] cu->cuMemAlloc(&data, size) failed -> CUDA_ERROR_OUT_OF_MEMORY: out of memory
```

`07-exact-3070-host-h264-cap512` (exit 0xc0000374):

```text
[h264_nvenc @ 000001c8c8cdbe80] CreateBitstreamBuffer failed: out of memory (10):
```

`07-exact-3070-host-hevc-cap512` (exit 0xc0000374):

```text
[hevc_nvenc @ 000001dbb4597c00] CreateBitstreamBuffer failed: out of memory (10):
```

## Resolution bisect (3070 host H.264, 512 MiB)

| Test | Cap MiB | Peak job commit MiB | Frames | Exit |
|---|---:|---:|---:|---|
| 06-3070-host-h264-1280x720-cap512 | 512 | 411.11 | 72 | 0 (0x00000000) |
| 06-3070-host-h264-1600x900-cap512 | 512 | 512.70 | 0 | 4294967284 (0xfffffff4) |
| 06-3070-host-h264-460x390-cap512 | 512 | 297.43 | 72 | 0 (0x00000000) |
| 06-3070-host-h264-1440x810-cap512 | 512 | 424.71 | 72 | 0 (0x00000000) |
| 06-3070-host-h264-1504x846-cap512 | 512 | 432.32 | 72 | 0 (0x00000000) |
| 06-3070-host-h264-1536x864-cap512 | 512 | 441.15 | 72 | 0 (0x00000000) |
| 06-3070-host-h264-1568x882-cap512 | 512 | 513.75 | none | 3221226356 (0xc0000374) |

For this veryfast-source family, the tested bracket is **1536×864 succeeds; 1568×882 fails**. 1280×720 and 460×390 succeed; 1600×900 and 1920×1080 fail. This is a bracket among tested even 16:9 sizes, not proof of a universal smallest failing pixel dimension. Different source reference/GOP structure, codec, surface pools and allocation timing can move the threshold and the specific failed API.

## Crash event and native stack

Fresh Windows Application Error event 1000 identifies the experiment's copied FFmpeg, PID `0x6D14` / 27924, exception `0xc0000374`, faulting `ntdll.dll`, offset `0x118095`, report ID `c95c8531-6250-4c23-84d1-85686c07dac9`. Its PID matches the live-debugger capture. The full event message/XML and historical FFmpeg events are retained separately.

The first three debugger trials did not reproduce heap corruption: Windows debug heap changes allocations and moved the failure earlier (or into later decode). One final test used process-local `_NO_DEBUG_HEAP=1` to retain the normal heap while debugging only the experiment child. That reproduced the bitstream-buffer failure and both first- and second-chance `0xc0000374` exceptions. StackWalk64 used Windows' installed DbgHelp with no downloaded symbols or debugger installation.

```text
exception code=0xc0000374 firstChance=1 tid=27220 address=0x00007ff807ed8095
#00 ntdll.dll+0x118095 (0x00007ff807ed8095) RtlpMuiRegCreateRegistryInfo+0x1b5
#05 ntdll.dll+0x2796a (0x00007ff807de796a) RtlFreeHeap+0x6da
#06 nvcuvid.dll+0x293d70 (0x00007ffe5df63d70) __std_4U4S4U_X04+0xcee90
#07 nvcuvid.dll+0x1e99fe (0x00007ffe5deb99fe) __std_4U4S4U_X04+0x24b1e
#08 nvcuvid.dll+0xf9038 (0x00007ffe5ddc9038) __std_2U4S4U_X08+0xf5c88
#09 nvEncodeAPI64.dll+0x2592c (0x00007fffa67e592c)
#10 ffmpeg.exe+0x7e6f197 (0x00007ff6d265f197) cairo_dwrite_font_face_set_measuring_mode+0x3c3ca47
exception code=0xc0000374 firstChance=0 tid=27220 address=0x00007ff807ed8095
#00 ntdll.dll+0x118095 (0x00007ff807ed8095) RtlpMuiRegCreateRegistryInfo+0x1b5
#05 ntdll.dll+0x2796a (0x00007ff807de796a) RtlFreeHeap+0x6da
#06 nvcuvid.dll+0x293d70 (0x00007ffe5df63d70) __std_4U4S4U_X04+0xcee90
#07 nvcuvid.dll+0x1e99fe (0x00007ffe5deb99fe) __std_4U4S4U_X04+0x24b1e
#08 nvcuvid.dll+0xf9038 (0x00007ffe5ddc9038) __std_2U4S4U_X08+0xf5c88
#09 nvEncodeAPI64.dll+0x2592c (0x00007fffa67e592c)
#10 ffmpeg.exe+0x7e6f197 (0x00007ff6d265f197) cairo_dwrite_font_face_set_measuring_mode+0x3c3ca47
```

Module-relative addresses are the reliable evidence. Without matching private/PDB symbols, the helper's nearest-export names for large offsets (for example FFmpeg's unrelated Cairo export) do **not** identify the actual functions. The full raw stack is retained; there is no claim to have located the corrupting write. One oversized diagnostic script transfer stalled before executing; only its positively identified experiment PowerShell PID 29380 was terminated. No worker process was stopped.

## Validation, evidence and exact commands

- 66 recorded FFmpeg invocations: source preparation, comparisons, resolution tests, diagnostics and 22 complete output validations. All 22 validation runs decoded exactly 72 frames and exited 0. All test children completed within their 90-second bound; no media child required timeout termination.
- `commands-results.jsonl` records each exact executable command/argv, UTC interval, child PID, exit in unsigned decimal and hex, peak job commit, job cap and elapsed time. `COMMANDS.md` renders the same ledger with every exact command. `4294967284` / `0xfffffff4` is signed -12; `3221226356` is `0xc0000374`; CUDA decode failure cases returned 69.
- Every recorded invocation has its complete `.stderr.log` and `.stdout.log`, including failed jobs. Successful output decode logs are included. Synthetic media and copied executable were deleted from Windows after evidence retrieval.
- `evidence.zip` was retrieved before cleanup and SHA-256 verified locally against the Windows hash: `a373e4c9543071d4b3f4b6182ae1dfe56de5bd3fae4d624e30f5c51a89c05dc1`. Extracted local evidence also includes the reference lock added after retrieval; the original ZIP is unchanged.
- The local `.ps1` scripts and helper sources show the exact launch mechanism. `cleanup.ps1` shows removal; `cleanup-verify.ps1` shows the final read-only verification. Its first post-removal worker creation-time comparison was too strict for JSON millisecond precision; corrected verification compared the same PID, executable path and epoch milliseconds, and passed. Removal itself had already succeeded.

## Cleanup and unchanged machine boundary

Final read-only verification returned:

```json
{
    "utc":  "2026-10-18T03:18:07.0389149Z",
    "removedDirectory":  "C:\\Users\\Danny\\AppData\\Local\\Temp\\rtx3070-ab-70eaba6b4591",
    "testPathAfterRemoval":  false,
    "originalWorkerPid":  33992,
    "originalWorkerCreationEpochMs":  1792289909277,
    "originalWorkerCreationUnchanged":  true,
    "workerTrackedFilesUnchanged":  true,
    "ownedFFmpegRemaining":  0,
    "note":  "Creation time compared at the millisecond precision preserved by the JSON snapshot"
}
```

Worker PID 33992 and creation time are unchanged. `worker.json` and `current.json` SHA-256 hashes match before/after; installed FFmpeg pin is unchanged. No experiment FFmpeg process remained. Local git status stayed clean. The Windows temporary directory was removed and a separate read-only check confirmed `Test-Path = False`.

No worker/configuration/installed-file changes, worker stop/restart, device/driver/display/Windows setting changes, startup changes, branch, push, production action or Cloudflare action occurred. Experiment artifacts are retained only in this local `/tmp/rtx3070-ab-70eaba6b4591` evidence directory. No representative workload/quality/performance benchmark, concurrency qualification, arbitrary-input memory guarantee or production deployment is claimed.
