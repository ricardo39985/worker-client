$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
Get-Process ffmpeg -ErrorAction SilentlyContinue | Select-Object Id,CPU,WorkingSet64,StartTime | ConvertTo-Json -Compress
$r=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018\after-real3-nvenc'
Get-ChildItem $r -Filter '*.mp4' | Select-Object Name,Length,LastWriteTimeUtc | ConvertTo-Json -Compress
Get-Content (Join-Path $env:LOCALAPPDATA 'OrganizerWorker\worker-status.txt')
