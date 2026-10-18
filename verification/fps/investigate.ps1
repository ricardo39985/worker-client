$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
Get-ChildItem (Join-Path $r 'jobs') -Directory | Select-Object Name
Get-ChildItem (Join-Path $r 'dependencies') -Directory | Select-Object Name
Get-ChildItem (Join-Path $r 'dependencies') -Filter ffmpeg.exe | Select-Object FullName
Get-Command ffprobe.exe -ErrorAction SilentlyContinue | Select-Object Source
Select-String -Path (Join-Path $r 'worker.log') -Pattern '610735d5|f4b17fa4' | Where-Object {$_.Line -match 'START|ENCODER|succeeded|selected|decode='} | ForEach-Object {$_.Line}
