$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../scripts/worker-launch-policy.ps1')
. (Join-Path $PSScriptRoot '../scripts/worker-package.ps1')
function Check([bool]$Value,[string]$Reason){if(-not $Value){throw $Reason}}
# Public SID adapter, not a task/credential or shell executor.
function Test-WorkerAccountSid([string]$Account,[string]$ExpectedSid){return $Account -ceq $ExpectedSid}
$sid='S-1-5-21-123';$t=[pscustomobject]@{TaskName=('OrganizerWorker-Unattended-'+$sid);Principal=[pscustomobject]@{UserId=$sid;RunLevel='Limited';LogonType='Password'}}
$e=[pscustomobject]@{ok=$true;enrolled=$true;owner_sid=$sid;task_name=$t.TaskName}
Assert-WorkerLaunchTask $t $sid $true $e
foreach($bad in 'S4U','Interactive','ServiceAccount'){$t.Principal.LogonType=$bad;$threw=$false;try{Assert-WorkerLaunchTask $t $sid $true $e}catch{$threw=$true};Check $threw ('Unsafe unattended logon accepted '+$bad)}
$t.Principal.LogonType='Password';$t.Principal.RunLevel='Highest';$threw=$false;try{Assert-WorkerLaunchTask $t $sid $true $e}catch{$threw=$true};Check $threw 'Admin engine task accepted'
$t.Principal.RunLevel='Limited';$e.owner_sid='S-1-5-19';$threw=$false;try{Assert-WorkerLaunchTask $t $sid $true $e}catch{$threw=$true};Check $threw 'Different enrolled owner accepted'
$s=[pscustomobject]@{live=$true;connected=$true;exiting=$false;headless=$true;identity_ready=$true;processing_ready=$false;token_elevated=$false}
Check (-not (Test-WorkerLaunchReady $s $true)) 'False processing incorrectly called ready'
$s.processing_ready=$true;Check (Test-WorkerLaunchReady $s $true) 'Genuine headless readiness rejected'
foreach($field in 'headless','identity_ready'){$s.$field=$false;Check (-not (Test-WorkerLaunchReady $s $true)) ('Missing readiness accepted '+$field);$s.$field=$true}
$s.token_elevated=$true;Check (-not (Test-WorkerLaunchReady $s $true)) 'Elevated headless accepted'
$one=[pscustomobject]@{revision=('a'*40);files=[pscustomobject]@{}};$two=[pscustomobject]@{schema=2;revision=('a'*40);files=[pscustomobject]@{}}
Check (@(Get-WorkerPackageNames $one).Count -eq 5) 'Legacy package compatibility lost'
$names=@(Get-WorkerPackageNames $two);Check ($names.Count -eq 7 -and 'OrganizerWorkerTray.exe' -in $names -and 'start-headless.ps1' -in $names) 'Headless package is missing attach/launcher'
Write-Output 'UNATTENDED_POLICY_PASS strict readiness, same LIMITED password owner, preserved legacy package and fixed headless artifacts'
