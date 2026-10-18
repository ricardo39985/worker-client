# Exact FFmpeg command ledger

The cap is enforced by the experiment-only native Job Object launcher before resuming each child. All command lines below are exact recorded Windows command lines. See runner.cs/debug-runner.cs/event-runner.cs and the local PowerShell scripts for the launch mechanism.

## 1. 00-fixture

UTC: 2026-10-18T03:11:29.7804127Z to 2026-10-18T03:11:31.5663628Z; PID 25080; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 238260224 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-f" "lavfi" "-i" "testsrc2=size=1920x1080:rate=24" "-f" "lavfi" "-i" "sine=frequency=1000:sample_rate=48000" "-t" "3" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "aac" "-b:a" "160k" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4"
```

## 2. 01-3070-host-h264-unlimited

UTC: 2026-10-18T03:11:31.6247727Z to 2026-10-18T03:11:32.2161256Z; PID 21248; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 589119488 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\01-3070-host-h264-unlimited.mp4"
```

## 3. 02-3050-host-h264-unlimited

UTC: 2026-10-18T03:11:32.2196541Z to 2026-10-18T03:11:32.8810087Z; PID 35836; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 524238848 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "1" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\02-3050-host-h264-unlimited.mp4"
```

## 4. 03-3070-device-h264-unlimited

UTC: 2026-10-18T03:11:39.2373967Z to 2026-10-18T03:11:39.7853104Z; PID 6236; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 586657792 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\03-3070-device-h264-unlimited.mp4"
```

## 5. 04a-3070-host-hevc-unlimited

UTC: 2026-10-18T03:11:39.8230137Z to 2026-10-18T03:11:40.4094852Z; PID 9932; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 574152704 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\04a-3070-host-hevc-unlimited.mp4"
```

## 6. 04b-3070-device-hevc-unlimited

UTC: 2026-10-18T03:11:40.4124835Z to 2026-10-18T03:11:40.9335529Z; PID 36316; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 572755968 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\04b-3070-device-hevc-unlimited.mp4"
```

## 7. 05a-3070-host-h264-cap512

UTC: 2026-10-18T03:11:50.5040059Z to 2026-10-18T03:11:50.8317302Z; PID 18912; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 538042368 bytes; exit 4294967284 / 0xfffffff4.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05a-3070-host-h264-cap512.mp4"
```

## 8. 05b-3070-host-h264-unlimited-repeat

UTC: 2026-10-18T03:11:50.8724194Z to 2026-10-18T03:11:51.4735029Z; PID 4208; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 589639680 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05b-3070-host-h264-unlimited-repeat.mp4"
```

## 9. 05c-3050-host-h264-cap512

UTC: 2026-10-18T03:11:51.4776786Z to 2026-10-18T03:11:52.0984664Z; PID 12496; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 519917568 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "1" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05c-3050-host-h264-cap512.mp4"
```

## 10. 05d-3070-device-h264-cap512

UTC: 2026-10-18T03:11:52.1024650Z to 2026-10-18T03:11:52.5092077Z; PID 33692; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 537776128 bytes; exit 69 / 0x00000045.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05d-3070-device-h264-cap512.mp4"
```

## 11. 05e-3070-host-hevc-cap512

UTC: 2026-10-18T03:11:52.5181631Z to 2026-10-18T03:11:52.8042242Z; PID 23432; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 537387008 bytes; exit 4294967284 / 0xfffffff4.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05e-3070-host-hevc-cap512.mp4"
```

## 12. 05f-3070-device-hevc-cap512

UTC: 2026-10-18T03:11:52.8086316Z to 2026-10-18T03:11:53.1923908Z; PID 19464; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 538173440 bytes; exit 69 / 0x00000045.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05f-3070-device-hevc-cap512.mp4"
```

## 13. 05g-3070-host-h264-cap768

UTC: 2026-10-18T03:11:53.2129488Z to 2026-10-18T03:11:53.8101508Z; PID 9900; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 587878400 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05g-3070-host-h264-cap768.mp4"
```

## 14. 05h-3070-host-hevc-cap768

UTC: 2026-10-18T03:11:53.8142822Z to 2026-10-18T03:11:54.3851984Z; PID 30416; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 573620224 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05h-3070-host-hevc-cap768.mp4"
```

## 15. 06-fixture-1280x720

UTC: 2026-10-18T03:12:18.8761497Z to 2026-10-18T03:12:19.4674733Z; PID 36144; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 239263744 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1280:720" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1280x720.mp4"
```

## 16. 06-3070-host-h264-1280x720-cap512

UTC: 2026-10-18T03:12:19.5253060Z to 2026-10-18T03:12:19.9139341Z; PID 21896; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 431075328 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1280x720.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1280x720-cap512.mp4"
```

## 17. 06-fixture-1600x900

UTC: 2026-10-18T03:12:19.9172234Z to 2026-10-18T03:12:21.0520311Z; PID 21900; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 282910720 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1600:900" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1600x900.mp4"
```

## 18. 06-3070-host-h264-1600x900-cap512

UTC: 2026-10-18T03:12:21.0563342Z to 2026-10-18T03:12:21.3425691Z; PID 13608; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 537600000 bytes; exit 4294967284 / 0xfffffff4.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1600x900.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1600x900-cap512.mp4"
```

## 19. 06-fixture-460x390

UTC: 2026-10-18T03:12:21.3457082Z to 2026-10-18T03:12:21.5892878Z; PID 31168; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 179503104 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=460:390" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-460x390.mp4"
```

## 20. 06-3070-host-h264-460x390-cap512

UTC: 2026-10-18T03:12:21.5925301Z to 2026-10-18T03:12:21.8961923Z; PID 34940; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 311873536 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-460x390.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-460x390-cap512.mp4"
```

## 21. 06-fixture-1440x810

UTC: 2026-10-18T03:12:39.4661770Z to 2026-10-18T03:12:40.2705884Z; PID 31188; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 260186112 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1440:810" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1440x810.mp4"
```

## 22. 06-3070-host-h264-1440x810-cap512

UTC: 2026-10-18T03:12:40.3291706Z to 2026-10-18T03:12:40.8133590Z; PID 33208; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 445337600 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1440x810.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1440x810-cap512.mp4"
```

## 23. 06-fixture-1504x846

UTC: 2026-10-18T03:12:40.8568247Z to 2026-10-18T03:12:41.6227834Z; PID 18212; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 268652544 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1504:846" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1504x846.mp4"
```

## 24. 06-3070-host-h264-1504x846-cap512

UTC: 2026-10-18T03:12:41.6271340Z to 2026-10-18T03:12:42.0486494Z; PID 35048; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 453320704 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1504x846.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1504x846-cap512.mp4"
```

## 25. 06-fixture-1536x864

UTC: 2026-10-18T03:12:42.0590856Z to 2026-10-18T03:12:42.8505591Z; PID 19972; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 272199680 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1536:864" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1536x864.mp4"
```

## 26. 06-3070-host-h264-1536x864-cap512

UTC: 2026-10-18T03:12:42.8538818Z to 2026-10-18T03:12:43.2696600Z; PID 35464; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 462581760 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1536x864.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1536x864-cap512.mp4"
```

## 27. 06-fixture-1568x882

UTC: 2026-10-18T03:12:43.2906295Z to 2026-10-18T03:12:44.1203003Z; PID 15804; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 278798336 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source.mp4" "-vf" "scale=1568:882" "-c:v" "libx264" "-preset" "veryfast" "-crf" "23" "-pix_fmt" "yuv420p" "-threads" "1" "-filter_threads" "1" "-c:a" "copy" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1568x882.mp4"
```

## 28. 06-3070-host-h264-1568x882-cap512

UTC: 2026-10-18T03:12:44.1248965Z to 2026-10-18T03:12:44.4066689Z; PID 33924; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 538710016 bytes; exit 3221226356 / 0xc0000374.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\source-1568x882.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1568x882-cap512.mp4"
```

## 29. 07-fixture-worker-exact

UTC: 2026-10-18T03:12:44.4261253Z to 2026-10-18T03:12:44.7722683Z; PID 36492; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 112128000 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "error" "-y" "-f" "lavfi" "-i" "testsrc2=size=1920x1080:rate=24" "-f" "lavfi" "-i" "sine=frequency=880:sample_rate=48000" "-t" "3" "-frames:v" "72" "-c:v" "libx264" "-preset" "ultrafast" "-threads" "1" "-pix_fmt" "yuv420p" "-c:a" "aac" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4"
```

## 30. 07-exact-3070-host-h264-cap512

UTC: 2026-10-18T03:12:44.7761109Z to 2026-10-18T03:12:45.0483436Z; PID 35836; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 536879104 bytes; exit 3221226356 / 0xc0000374.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-h264-cap512.mp4"
```

## 31. 07-exact-3070-host-h264-cap0

UTC: 2026-10-18T03:12:45.0508441Z to 2026-10-18T03:12:45.5324085Z; PID 36632; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 589074432 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-h264-cap0.mp4"
```

## 32. 07-exact-3070-host-hevc-cap512

UTC: 2026-10-18T03:12:45.5354258Z to 2026-10-18T03:12:45.8099830Z; PID 17668; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 537104384 bytes; exit 3221226356 / 0xc0000374.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-hevc-cap512.mp4"
```

## 33. 07-exact-3070-host-hevc-cap0

UTC: 2026-10-18T03:12:45.8131127Z to 2026-10-18T03:12:46.3039641Z; PID 20496; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 574660608 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-hevc-cap0.mp4"
```

## 34. 08-debug-3070-host-hevc-cap512

UTC: 2026-10-18T03:15:15.7248826Z to 2026-10-18T03:15:16.4327839Z; PID 34544; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 538566656 bytes; exit 4294967284 / 0xfffffff4.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\08-debug-3070-host-hevc-cap512.mp4"
```

Diagnostic: DEBUG_ONLY_THIS_PROCESS; StackWalk64; no symbols downloaded

## 35. 08-debug-3070-host-h264-cap512

UTC: 2026-10-18T03:15:34.5560652Z to 2026-10-18T03:15:35.2363646Z; PID 33456; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 539901952 bytes; exit 4294967284 / 0xfffffff4.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\08-debug-3070-host-h264-cap512.mp4"
```

Diagnostic: DEBUG_ONLY_THIS_PROCESS; StackWalk64; no symbols downloaded

## 36. 08-debug-3070-host-hevc-cap528

UTC: 2026-10-18T03:15:35.2801908Z to 2026-10-18T03:15:36.0073084Z; PID 35680; JobMemoryLimit 528 MiB (0 = no explicit memory cap); peak commit 555683840 bytes; exit 69 / 0x00000045.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\08-debug-3070-host-hevc-cap528.mp4"
```

Diagnostic: DEBUG_ONLY_THIS_PROCESS; StackWalk64; no symbols downloaded

## 37. 08-debug-normalheap-3070-host-hevc-cap512

UTC: 2026-10-18T03:16:07.7690888Z to 2026-10-18T03:16:09.0232274Z; PID 27924; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 538206208 bytes; exit 3221226356 / 0xc0000374.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\08-debug-normalheap-3070-host-hevc-cap512.mp4"
```

Diagnostic: DEBUG_ONLY_THIS_PROCESS; _NO_DEBUG_HEAP=1 child-only; StackWalk64; no symbol downloads

## 38. 09-exact-3070-host-h264-cap768

UTC: 2026-10-18T03:16:31.9077418Z to 2026-10-18T03:16:32.4214457Z; PID 17816; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 576954368 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-host-h264-cap768.mp4"
```

## 39. 09-exact-3070-device-h264-cap768

UTC: 2026-10-18T03:16:32.4651166Z to 2026-10-18T03:16:32.9168753Z; PID 14696; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 559570944 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-device-h264-cap768.mp4"
```

## 40. 09-exact-3050-host-h264-cap512

UTC: 2026-10-18T03:16:32.9201077Z to 2026-10-18T03:16:33.4599422Z; PID 24792; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 520351744 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "h264_nvenc" "-gpu" "1" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3050-host-h264-cap512.mp4"
```

## 41. 09-exact-3070-host-hevc-cap768

UTC: 2026-10-18T03:16:33.4619567Z to 2026-10-18T03:16:33.9392995Z; PID 36064; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 574275584 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-host-hevc-cap768.mp4"
```

## 42. 09-exact-3070-device-hevc-cap768

UTC: 2026-10-18T03:16:33.9428123Z to 2026-10-18T03:16:34.4196434Z; PID 36600; JobMemoryLimit 768 MiB (0 = no explicit memory cap); peak commit 545284096 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-init_hw_device" "cuda=ow:0" "-hwaccel" "cuda" "-hwaccel_device" "ow" "-hwaccel_output_format" "cuda" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-device-hevc-cap768.mp4"
```

## 43. 09-exact-3050-host-hevc-cap512

UTC: 2026-10-18T03:16:34.4226989Z to 2026-10-18T03:16:34.9504932Z; PID 36000; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 497041408 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "1" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3050-host-hevc-cap512.mp4"
```

## 44. validation-01-3070-host-h264-unlimited

UTC: 2026-10-18T03:16:35.0114739Z to 2026-10-18T03:16:35.4146814Z; PID 23648; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53116928 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\01-3070-host-h264-unlimited.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 45. validation-02-3050-host-h264-unlimited

UTC: 2026-10-18T03:16:35.4176713Z to 2026-10-18T03:16:35.8203020Z; PID 33868; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53100544 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\02-3050-host-h264-unlimited.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 46. validation-03-3070-device-h264-unlimited

UTC: 2026-10-18T03:16:35.8234434Z to 2026-10-18T03:16:36.2510627Z; PID 36816; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53010432 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\03-3070-device-h264-unlimited.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 47. validation-04a-3070-host-hevc-unlimited

UTC: 2026-10-18T03:16:36.2535635Z to 2026-10-18T03:16:36.7616061Z; PID 36156; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59084800 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\04a-3070-host-hevc-unlimited.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 48. validation-04b-3070-device-hevc-unlimited

UTC: 2026-10-18T03:16:36.7650901Z to 2026-10-18T03:16:37.3129926Z; PID 36480; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59052032 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\04b-3070-device-hevc-unlimited.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 49. validation-05b-3070-host-h264-unlimited-repeat

UTC: 2026-10-18T03:16:37.3150408Z to 2026-10-18T03:16:37.7495314Z; PID 11172; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53198848 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05b-3070-host-h264-unlimited-repeat.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 50. validation-05c-3050-host-h264-cap512

UTC: 2026-10-18T03:16:37.7527905Z to 2026-10-18T03:16:38.1532761Z; PID 32956; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53018624 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05c-3050-host-h264-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 51. validation-05g-3070-host-h264-cap768

UTC: 2026-10-18T03:16:38.1555367Z to 2026-10-18T03:16:38.5826973Z; PID 22996; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53100544 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05g-3070-host-h264-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 52. validation-05h-3070-host-hevc-cap768

UTC: 2026-10-18T03:16:38.5846339Z to 2026-10-18T03:16:39.0861760Z; PID 18196; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59219968 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\05h-3070-host-hevc-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 53. validation-06-3070-host-h264-1280x720-cap512

UTC: 2026-10-18T03:16:39.0895665Z to 2026-10-18T03:16:39.2729796Z; PID 36324; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 35962880 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1280x720-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 54. validation-06-3070-host-h264-460x390-cap512

UTC: 2026-10-18T03:16:39.2749852Z to 2026-10-18T03:16:39.3447748Z; PID 956; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 25309184 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-460x390-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 55. validation-06-3070-host-h264-1440x810-cap512

UTC: 2026-10-18T03:16:39.3467759Z to 2026-10-18T03:16:39.5804997Z; PID 13604; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 40943616 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1440x810-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 56. validation-06-3070-host-h264-1504x846-cap512

UTC: 2026-10-18T03:16:39.5836245Z to 2026-10-18T03:16:39.8214710Z; PID 22328; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 41619456 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1504x846-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 57. validation-06-3070-host-h264-1536x864-cap512

UTC: 2026-10-18T03:16:39.8244743Z to 2026-10-18T03:16:40.1018733Z; PID 36560; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 42016768 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\06-3070-host-h264-1536x864-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 58. validation-07-exact-3070-host-h264-cap0

UTC: 2026-10-18T03:16:40.1050486Z to 2026-10-18T03:16:40.5372215Z; PID 19464; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 51916800 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-h264-cap0.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 59. validation-07-exact-3070-host-hevc-cap0

UTC: 2026-10-18T03:16:40.5414057Z to 2026-10-18T03:16:41.0997168Z; PID 34600; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 58957824 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\07-exact-3070-host-hevc-cap0.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 60. validation-09-exact-3070-host-h264-cap768

UTC: 2026-10-18T03:16:41.1023151Z to 2026-10-18T03:16:41.5530595Z; PID 26792; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53075968 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-host-h264-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 61. validation-09-exact-3070-device-h264-cap768

UTC: 2026-10-18T03:16:41.5551032Z to 2026-10-18T03:16:42.0004560Z; PID 32976; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53350400 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-device-h264-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 62. validation-09-exact-3050-host-h264-cap512

UTC: 2026-10-18T03:16:42.0038866Z to 2026-10-18T03:16:42.4262619Z; PID 6892; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 53186560 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3050-host-h264-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 63. validation-09-exact-3070-host-hevc-cap768

UTC: 2026-10-18T03:16:42.4286128Z to 2026-10-18T03:16:42.9381971Z; PID 36820; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59097088 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-host-hevc-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 64. validation-09-exact-3070-device-hevc-cap768

UTC: 2026-10-18T03:16:42.9415153Z to 2026-10-18T03:16:43.4776991Z; PID 4940; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59146240 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3070-device-hevc-cap768.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 65. validation-09-exact-3050-host-hevc-cap512

UTC: 2026-10-18T03:16:43.4796996Z to 2026-10-18T03:16:43.9946734Z; PID 11860; JobMemoryLimit 0 MiB (0 = no explicit memory cap); peak commit 59154432 bytes; exit 0 / 0x00000000.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "info" "-xerror" "-threads" "1" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\09-exact-3050-host-hevc-cap512.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-threads" "1" "-f" "null" "-"
```

## 66. 10-event-3070-host-hevc-cap512

UTC: 2026-10-18T03:17:01.0928259Z to 2026-10-18T03:17:01.3966716Z; PID 18008; JobMemoryLimit 512 MiB (0 = no explicit memory cap); peak commit 537341952 bytes; exit 3221226356 / 0xc0000374.

```text
"C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\ffmpeg.exe" "-nostdin" "-hide_banner" "-loglevel" "verbose" "-y" "-protocol_whitelist" "file" "-format_whitelist" "mov,matroska,webm,jpeg_pipe,png_pipe,webp_pipe" "-threads" "1" "-filter_threads" "1" "-noautorotate" "-i" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\worker-source.mp4" "-map" "0:v:0" "-map" "0:a:0?" "-c:v" "hevc_nvenc" "-gpu" "0" "-preset" "p4" "-rc" "vbr" "-cq" "24" "-b:v" "0" "-rc-lookahead" "0" "-pix_fmt" "yuv420p" "-threads" "1" "-tag:v" "hvc1" "-c:a" "aac" "-b:a" "160k" "-fps_mode" "passthrough" "-movflags" "+faststart" "C:\Users\Danny\AppData\Local\Temp\rtx3070-ab-70eaba6b4591\10-event-3070-host-hevc-cap512.mp4"
```

Diagnostic: SEM_FAILCRITICALERRORS only; normal child; worker-equivalent DIE_ON_UNHANDLED_EXCEPTION Job flag
