param([Parameter(Mandatory=$true)][string]$ExpectedSha,[Parameter(Mandatory=$true)][string]$Revision)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$c=Get-Content (Join-Path $r 'current.json') -Raw|ConvertFrom-Json
$sha=(Get-FileHash (Join-Path $c.directory 'OrganizerWorker.exe')).Hash.ToLowerInvariant()
$actualRevision=(Get-Content (Join-Path $c.directory 'source-revision.txt') -Raw).Trim()
if($sha -cne $ExpectedSha -or $c.sha256 -cne $ExpectedSha -or $actualRevision -cne $Revision){throw 'Installed manifest/image/source revision mismatch'}
$p=@(Get-Process OrganizerWorker -ErrorAction Stop)
if($p.Count -ne 1 -or $p[0].Path -ine (Join-Path $c.directory 'OrganizerWorker.exe') -or $p[0].SessionId -eq 0){throw 'Selected worker is not running in the signed-in desktop'}
$runtime=Get-Content (Join-Path $r 'runtime.json') -Raw|ConvertFrom-Json
if($runtime.process_id -ne $p[0].Id -or $runtime.process_started_filetime -ne $p[0].StartTime.ToUniversalTime().ToFileTimeUtc()){throw 'Runtime identity mismatch'}
Write-Output ('INSTALLED_VERSION='+[IO.Path]::GetFileName($c.directory))
Write-Output ('INSTALLED_SHA256='+$sha)
Write-Output ('INSTALLED_REVISION='+$actualRevision)
Write-Output ('PROCESS_PID='+$p[0].Id+' SESSION='+$p[0].SessionId+' RUNTIME_STATE='+$runtime.state)
$all=@(Get-Content (Join-Path $r 'worker.log'));$begin=-1
for($i=$all.Count-1;$i -ge 0;$i--){if($all[$i] -match 'WORKER STARTED \| counts since this process'){$begin=$i;break}}
if($begin -lt 0){throw 'Current worker startup epoch missing'}
$epoch=@($all[$begin..($all.Count-1)])
$epoch|Where-Object {$_ -match 'STATE CONNECTED|PROBE RESULT|schedulable GPUs=|GPU DEVICE|ENCODER|stats:|WORKER STARTED'}|Select-Object -Last 24
$connections=@($epoch|Where-Object {$_ -match 'STATE (CONNECTED|DISCONNECTED)'})
if($connections.Count -eq 0 -or $connections[-1] -notmatch 'STATE CONNECTED'){throw 'Current epoch is not connected'}
$status=Get-Content (Join-Path $r 'worker-status.txt') -Raw
Write-Output 'IDLE_TABLE_BEGIN';Write-Output $status;Write-Output 'IDLE_TABLE_END'
if($status -notmatch 'state\s+accepting' -or $status -notmatch 'jobs\s+running 0' -or $status -notmatch 'cards\s+.+idle'){throw 'Fresh accepting idle table missing'}
if((Get-Item (Join-Path $r 'worker-status.txt')).LastWriteTimeUtc -lt $p[0].StartTime.ToUniversalTime()){throw 'Stale status table'}
if($env:USERNAME -ieq 'danny') {
 if(-not ($epoch -match 'schedulable GPUs=2') -or $status -notmatch 'encoders\s+cpu,nvenc' -or $status -notmatch 'RTX 3070 idle\s+RTX 3050 idle'){throw 'Danny encoder/two-card qualification missing'}
} elseif($env:USERNAME -ieq 'reald') {
 if($status -notmatch 'encoders\s+cpu,qsv,amf'){throw 'Rick encoders missing'}
} else {throw 'Unexpected rollout account'}
Write-Output ('ROLLBACK_PENDING='+[IO.File]::Exists((Join-Path $r 'current.rollback.json')))
if([IO.File]::Exists((Join-Path $r 'current.rollback.json'))){throw 'Update did not commit'}
Write-Output 'CONNECTED_ACCEPTING_IDLE_VERIFIED=True'
