param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/start-headless.ps1')
$counter=[pscustomobject]@{starts=0}
$code=Invoke-WorkerHeadlessLifetime -Launch {$counter.starts++;if($counter.starts -le 5){return 75};return 0}
if($code -ne 0 -or $counter.starts -ne 6){throw 'Intentional caller-independent restart must not consume the three crash-retry budget, and drain exit0 must end the task'}
$counter.starts=0
$code=Invoke-WorkerHeadlessLifetime -Launch {$counter.starts++;return 4}
if($code -ne 4 -or $counter.starts -ne 1){throw 'Actual failure must reach bounded Windows retry policy, not hot-loop forever'}
'LAUNCHER_LIFETIME_PASS five intentional restarts survive CLI departure; drain ends; failures bounded'
