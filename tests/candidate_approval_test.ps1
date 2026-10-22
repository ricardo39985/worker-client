param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/worker-package.ps1')
. (Join-Path $SourceRoot 'scripts/worker-launch-policy.ps1')
function Check([bool]$ok,[string]$why){if(-not $ok){throw $why}}
$names=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe','OrganizerWorkerTray.exe','cli.md','start-headless.ps1')
$files=[ordered]@{};foreach($name in $names){$files[$name]='a'*64}
$info=[pscustomobject]@{schema=2;revision=('b'*40);files=[pscustomobject]$files}
$approved=[pscustomobject]@{};$approved|Add-Member -NotePropertyName ('a'*64) -NotePropertyValue ([pscustomobject]@{files=[pscustomobject]$files})
$enrollment=[pscustomobject]@{ok=$true;enrolled=$true;launcher_sha256=('a'*64);approved_packages=$approved}
Assert-WorkerApprovedCandidate $info $enrollment
foreach($name in @('unapproved-worker','changed-policy','unapproved-companion')){
 $candidate=$info|ConvertTo-Json -Depth 10|ConvertFrom-Json
 switch($name){'unapproved-worker'{$candidate.files.'OrganizerWorker.exe'='c'*64};'changed-policy'{$candidate.files.'start-headless.ps1'='d'*64};'unapproved-companion'{$candidate.files.'OrganizerWorkerDoctor.exe'='e'*64}}
 $stopped=$false;try{Assert-WorkerApprovedCandidate $candidate $enrollment;$stopped=$true}catch{}
 Check (-not $stopped) ($name+': unapproved candidate reached current-engine stop/publication boundary')
}
'CANDIDATE_APPROVAL_PASS protected exact package/policy before any candidate execution or current-engine stop'
