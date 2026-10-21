$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/worker-update-policy.ps1')
function Check([bool]$Ok,[string]$Reason){if(-not $Ok){throw $Reason}}
$before='{"schema":1,"coordinator_url":"https://fixture.invalid","limits":{"cpu_threads":2,"ram_mb":1024},"storage_hosts":["fixture.invalid"],"opaque":[null,"x"]}'|ConvertFrom-Json
$expected='{"priority":"backup","video_preference":"disabled","image_preference":"preferred","paused":true}'|ConvertFrom-Json
$after='{"opaque":[null,"x"],"storage_hosts":["fixture.invalid"],"limits":{"ram_mb":1024,"cpu_threads":2},"coordinator_url":"https://fixture.invalid","schema":1,"operator":{"priority":"backup","video_preference":"disabled","image_preference":"preferred","paused":true}}'|ConvertFrom-Json
Test-WorkerRetainedConfiguration $before $after $expected
$after.limits.cpu_threads=4;$threw=$false
try { Test-WorkerRetainedConfiguration $before $after $expected } catch { $threw=$true };Check $threw 'Resource mutation accepted'
$after.limits.cpu_threads=2;$after.operator.paused=$false;$threw=$false
try { Test-WorkerRetainedConfiguration $before $after $expected } catch { $threw=$true };Check $threw 'Preference mutation accepted'
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-control-policy-'+[guid]::NewGuid().ToString('N'))
$fake=[pscustomobject]@{Path=(Join-Path $root 'versions/old/OrganizerWorker.exe');Id=$PID}
$requests=New-Object Collections.Generic.List[string]
$threw=$false
try { Invoke-WorkerCooperativeStop -Root $root -FindWorkers {$fake} -RequestDrain {param($ctl)$requests.Add($ctl)} -WaitSeconds 0 } catch { $threw=$_.Exception.Data['worker_error'] -eq 'owner_exit_required' }
Check $threw 'Old tray did not require actual owner exit';Check ($requests.Count -eq 0) 'Unsupported tray received a destructive control';Check (-not (Test-Path $root)) 'Preflight mutated installation'
Invoke-WorkerCooperativeStop -Root $root -FindWorkers {@()} -RequestDrain {throw 'Must not run without controller'} -WaitSeconds 0
Write-Output 'UPDATER_CONTROL_PASS semantic legacy retention, exact admission intent, no unsupported tray termination'
