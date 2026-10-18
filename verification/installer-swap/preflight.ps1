$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$id=[Security.Principal.WindowsIdentity]::GetCurrent();$tn='OrganizerWorker-'+$id.User.Value
$t=Get-ScheduledTask -TaskName $tn
$t.Principal | Select-Object UserId,RunLevel,LogonType | ConvertTo-Json -Compress
$t.Actions | Select-Object Execute,Arguments,WorkingDirectory | ConvertTo-Json -Compress
Get-Content (Join-Path $r 'current.json') -Raw
Write-Output ('PENDING_ROLLBACK='+[IO.File]::Exists((Join-Path $r 'current.rollback.json')))
Get-Content (Join-Path $r 'worker-status.txt')
