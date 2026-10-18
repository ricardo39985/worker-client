$ErrorActionPreference='Stop'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-perf-finish-20261018'
if(Test-Path $root){throw 'Refusing to reuse performance directory'}
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$status=Get-Content (Join-Path $r 'worker-status.txt') -Raw
if($status -notmatch 'jobs\s+running 0'){throw 'Live worker busy; no measurements permitted'}
if(Get-Process ffmpeg -ErrorAction SilentlyContinue){throw 'FFmpeg already active'}
New-Item -ItemType Directory -Path $root | Out-Null
Write-Output ('MEASUREMENT_ROOT='+$root)
Get-Content (Join-Path $r 'worker-status.txt')
