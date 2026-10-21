param([Parameter(Mandatory=$true)][string]$SourceDirectory,[Parameter(Mandatory=$true)][string]$BinaryDirectory,
      [Parameter(Mandatory=$true)][string]$LegacyBinary,[Parameter(Mandatory=$true)][string]$Root,
      [Parameter(Mandatory=$true)][string]$StartupTaskName,[Parameter(Mandatory=$true)][string]$Revision)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$identity=[Security.Principal.WindowsIdentity]::GetCurrent();$principal=New-Object Security.Principal.WindowsPrincipal($identity)
if($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) -or (Get-Process -Id $PID).SessionId -eq 0){throw 'Legacy upgrade acceptance requires actual LIMITED interactive owner'}
if(Test-Path (Join-Path $Root 'current.json')){throw 'Legacy fixture root must be new'}
function Wait-File([string]$Path){$until=[DateTime]::UtcNow.AddSeconds(60);while(-not (Test-Path $Path)){if([DateTime]::UtcNow -ge $until){throw ('Timeout: '+$Path)};Start-Sleep -Milliseconds 100}}
$old=Join-Path $Root 'versions\legacy';$package=Join-Path $Root 'candidate-package';New-Item -ItemType Directory -Path $old,$package -Force | Out-Null
Copy-Item $LegacyBinary (Join-Path $old 'OrganizerWorker.exe')
[IO.File]::WriteAllText((Join-Path $old 'source-revision.txt'),'e4366c326c124acc46f63bd7251bd0cdf24f4331')
$oldSha=(Get-FileHash (Join-Path $old 'OrganizerWorker.exe')).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $Root 'current.json'),([ordered]@{schema=1;directory=$old;sha256=$oldSha}|ConvertTo-Json))
# Startup fixture task is pre-registered by the authorized outer owner broker,
# with this exact root, LIMITED Interactive principal and bounded lifetime.
$startupBefore=(Export-ScheduledTask -TaskName $StartupTaskName)
Start-ScheduledTask -TaskName $StartupTaskName
Wait-File (Join-Path $Root 'actor-ready.json')
$beforeActor=Get-Content (Join-Path $Root 'actor-ready.json') -Raw|ConvertFrom-Json
if($beforeActor.elevated -or $beforeActor.session -eq 0 -or $beforeActor.operator.priority -ne 'backup' -or -not $beforeActor.operator.paused){throw 'Legacy actual actor context/preferences incorrect'}
$beforeConfig=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json
$identityHashes=@{};foreach($file in 'credential.dpapi','pairing-key.dpapi'){$identityHashes[$file]=(Get-FileHash (Join-Path $Root $file)).Hash}
$actor=Get-Process -Id $beforeActor.pid;$held=$actor.Handle
# This marker is the isolated adapter's cooperative OLD public tray Exit/finish
# action, not an invented remote control for installed pre-CLI workers.
[IO.File]::WriteAllText((Join-Path $Root 'stop'),'finish')
if(-not $actor.WaitForExit(15000) -or $actor.ExitCode -ne 0){throw 'Legacy actor did not cooperatively exit'};$actor.Dispose()
Remove-Item (Join-Path $Root 'stop'),(Join-Path $Root 'actor-ready.json')
$until=[DateTime]::UtcNow.AddSeconds(15);while((Get-ScheduledTask -TaskName $StartupTaskName).State -eq 'Running'){if([DateTime]::UtcNow -ge $until){throw 'Fixture startup task did not settle'};Start-Sleep -Milliseconds 100}
Copy-Item (Join-Path $BinaryDirectory 'ow_upgrade_actor.exe') (Join-Path $package 'OrganizerWorker.exe')
foreach($name in 'OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe'){Copy-Item (Join-Path $BinaryDirectory $name) (Join-Path $package $name)}
Copy-Item (Join-Path $SourceDirectory 'docs/cli.md') (Join-Path $package 'cli.md')
$files=[ordered]@{};foreach($file in (Get-ChildItem $package -File)){$files[$file.Name]=(Get-FileHash $file.FullName).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $package 'package.json'),([ordered]@{revision=$Revision;fixture_artifact=$true;files=$files}|ConvertTo-Json -Depth 6))
$manifestSha=(Get-FileHash (Join-Path $package 'package.json')).Hash
& (Join-Path $SourceDirectory 'scripts/update-worker.ps1') -Root $Root -Package $package -PackageManifestSha256 $manifestSha -StartupTaskName $StartupTaskName
if($LASTEXITCODE -ne 0){throw 'Actual shared installer failed'}
$c=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
$ctl=Join-Path $c.directory 'OrganizerWorkerCtl.exe'
$statusText=& $ctl --json --data-root $Root status;if($LASTEXITCODE -ne 0){throw 'New actor CLI unavailable'};$status=$statusText|ConvertFrom-Json
if($c.directory -ceq $old -or $status.pid -eq $beforeActor.pid -or -not $status.connected -or -not $status.paused){throw 'Version switch/runtime/pause preservation failed'}
foreach($name in $identityHashes.Keys){if($identityHashes[$name] -cne (Get-FileHash (Join-Path $Root $name)).Hash){throw 'Protected identity changed'}}
$afterConfig=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json
. (Join-Path $SourceDirectory 'scripts/worker-update-policy.ps1')
Test-WorkerRetainedConfiguration $beforeConfig $afterConfig $beforeActor.operator
if((Export-ScheduledTask -TaskName $StartupTaskName) -cne $startupBefore){throw 'Startup registration changed'}
if(Test-Path (Join-Path $Root 'current.rollback.json')){throw 'Rollback marker not committed'}
if((Get-FileHash (Join-Path $old 'OrganizerWorker.exe')).Hash.ToLowerInvariant() -cne $oldSha){throw 'Old version was overwritten'}
$result=[ordered]@{fixture=$true;real_coordinator=$false;old_source='e4366c326c124acc46f63bd7251bd0cdf24f4331';new_source=$Revision;old_pid=$beforeActor.pid;new_pid=$status.pid;session=$beforeActor.session;elevated=$false;protected_identity_unchanged=$true;existing_settings_retained=$true;legacy_preferences_migrated=$true;startup_unchanged=$true;old_binary_retained=$true;actual_shared_installer=$true}
[IO.File]::WriteAllText((Join-Path $Root 'legacy-upgrade-result.json'),($result|ConvertTo-Json -Depth 6))
& $ctl --json --data-root $Root exit drain
if($LASTEXITCODE -ne 0){throw 'New actor cooperative exit failed'}
Write-Output 'LEGACY_UPGRADE_PASS actual old/new controllers, verified package transaction, retained identity/preferences/startup'
