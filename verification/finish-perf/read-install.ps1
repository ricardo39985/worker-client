Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$r=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$current=Get-Content (Join-Path $r 'current.json') -Raw | ConvertFrom-Json
$exe=Join-Path $current.directory 'OrganizerWorker.exe'
$sha=(Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()
if($sha -cne $current.sha256){throw 'Installed SHA mismatch'}
$processes=@(Get-Process OrganizerWorker -ErrorAction Stop)
if($processes.Count -ne 1 -or $processes[0].Path -ine $exe -or $processes[0].SessionId -eq 0){throw 'Worker not running as installed in desktop session'}
$runtime=Get-Content (Join-Path $r 'runtime.json') -Raw | ConvertFrom-Json
Write-Output ('VERSION='+[IO.Path]::GetFileName($current.directory))
Write-Output ('SHA256='+$sha)
Write-Output ('REVISION='+(Get-Content (Join-Path $current.directory 'source-revision.txt') -Raw))
Write-Output ('PROCESS_PID='+$processes[0].Id+' SESSION='+$processes[0].SessionId)
Write-Output ('RUNTIME='+($runtime|ConvertTo-Json -Compress))
$all=Get-Content (Join-Path $r 'worker.log')
$begin=-1;for($i=$all.Count-1;$i -ge 0;$i--){if($all[$i] -match 'worker started; counts since this process'){$begin=$i;break}}
if($begin -lt 0){throw 'Startup epoch missing'}
$epoch=$all[$begin..($all.Count-1)]
$epoch | Where-Object {$_ -match 'STATE CONNECTED|PROBE RESULT|schedulable GPUs=|GPU DEVICE|stats:|ENCODER|worker started|  encoders'} | Select-Object -Last 16
$status=Get-Content (Join-Path $r 'worker-status.txt') -Raw
Write-Output 'IDLE_TABLE_BEGIN';Write-Output $status;Write-Output 'IDLE_TABLE_END'
if($epoch -notmatch 'STATE CONNECTED'){throw 'Current epoch is not CONNECTED'}
if($status -notmatch 'state\s+accepting' -or $status -notmatch 'jobs\s+running 0' -or $status -notmatch 'cards\s+.+idle' -or $status -notmatch 'encoders\s+cpu'){throw 'Expected accepting idle table missing'}
if($env:USERNAME -ieq 'danny'){
 if($epoch -notmatch 'schedulable GPUs=2' -or $status -notmatch 'encoders\s+cpu,nvenc'){throw 'Danny GPU qualification missing'}
}else{if($status -notmatch 'encoders\s+cpu,qsv,amf'){throw 'Rick encoders missing'}}
Write-Output 'POST_INSTALL_PROOF_EXIT=0'
