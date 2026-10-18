$Package='C:\Users\reald\AppData\Local\OrganizerWorker\perf-land-0771b79\package';$ExpectedSha='8177f03c9172c023d137d1ea74d06ec141c1e1d123717de0312bc2c373d9ca05';$Revision='f56952e727478178555967100558943480efdc88'
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$old=Get-Content (Join-Path $r 'current.json') -Raw | ConvertFrom-Json
$hashes=@{};foreach($f in @('credential.dpapi','pairing-key.dpapi','worker.json')){$hashes[$f]=(Get-FileHash (Join-Path $r $f) -Algorithm SHA256).Hash}
foreach($file in @('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe')){if(-not (Test-Path (Join-Path $Package $file))){throw "Missing package binary $file"}}
$actual=(Get-FileHash (Join-Path $Package 'OrganizerWorker.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
if($actual -cne $ExpectedSha){throw 'Package SHA mismatch'}
$status=Get-Content (Join-Path $r 'worker-status.txt') -Raw
if($status -notmatch 'jobs\s+running 0'){throw 'Worker is busy; refusing installation'}
if(Get-Process ffmpeg -ErrorAction SilentlyContinue){throw 'FFmpeg active; refusing installation'}
$workers=@(Get-Process OrganizerWorker -ErrorAction SilentlyContinue)
foreach($p in $workers){if($p.Path -ine (Join-Path $old.directory 'OrganizerWorker.exe')){throw 'Unexpected running image'}}
foreach($p in $workers){Stop-Process -Id $p.Id -ErrorAction Stop;$p.WaitForExit(10000) | Out-Null;if(-not $p.HasExited){throw 'Old worker did not stop'}}
Write-Output 'OLD_WORKER_STOPPED_IDLE=True'
$version=Join-Path $r ('versions\0.1.2-'+$actual.Substring(0,16))
New-Item -ItemType Directory -Path $version -Force | Out-Null
foreach($file in @('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe')){Copy-Item (Join-Path $Package $file) (Join-Path $version $file) -Force;if((Get-FileHash (Join-Path $Package $file)).Hash -cne (Get-FileHash (Join-Path $version $file)).Hash){throw 'Installed file SHA mismatch'}}
[IO.File]::WriteAllText((Join-Path $version 'source-revision.txt'),$Revision)
$current=[ordered]@{schema=1;directory=$version;sha256=$actual}
$temp=Join-Path $r 'current.perf-publish.json'
[IO.File]::WriteAllText($temp,($current | ConvertTo-Json))
[IO.File]::Replace($temp,(Join-Path $r 'current.json'),$null)
foreach($f in $hashes.Keys){if($hashes[$f] -cne (Get-FileHash (Join-Path $r $f) -Algorithm SHA256).Hash){throw 'Identity/configuration changed'}}
Write-Output 'IDENTITY_AND_CONFIGURATION_UNCHANGED=True'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent();$taskName='OrganizerWorker-'+$identity.User.Value
$task=Get-ScheduledTask -TaskName $taskName
if([string]$task.Principal.RunLevel -notin @('Limited','0') -or [string]$task.Principal.LogonType -notin @('Interactive','3')){throw 'Startup task is not limited interactive'}
& schtasks.exe /Run /TN $taskName
$code=$LASTEXITCODE;Write-Output "START_TASK_EXIT=$code";if($code){exit $code}
Write-Output ('INSTALLED_VERSION='+[IO.Path]::GetFileName($version))
Write-Output ('INSTALLED_SHA256='+(Get-FileHash (Join-Path $version 'OrganizerWorker.exe') -Algorithm SHA256).Hash.ToLowerInvariant())
Write-Output ('INSTALLED_REVISION='+(Get-Content (Join-Path $version 'source-revision.txt') -Raw))
exit 0
