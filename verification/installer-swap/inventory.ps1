$ErrorActionPreference='Stop'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$c=Get-Content (Join-Path $r 'current.json') -Raw | ConvertFrom-Json
Write-Output ('USER='+$env:USERNAME+' ROOT='+$r)
Write-Output ('VERSION='+[IO.Path]::GetFileName($c.directory)+' SHA256='+(Get-FileHash (Join-Path $c.directory 'OrganizerWorker.exe')).Hash)
Get-Process OrganizerWorker,ffmpeg -ErrorAction SilentlyContinue | Select-Object Id,Path,SessionId | ConvertTo-Json -Compress
Get-Content (Join-Path $r 'worker-status.txt')
Get-Content (Join-Path $r 'worker.log') -Tail 8
Write-Output ('STAGING_EXISTS='+[IO.Directory]::Exists((Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018')))
