$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../scripts/worker-launch-policy.ps1')
. (Join-Path $PSScriptRoot '../scripts/worker-package.ps1')
. (Join-Path $PSScriptRoot '../scripts/worker-swap.ps1')
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
$temp=Join-Path ([IO.Path]::GetTempPath()) ('ow-package-headless-'+[guid]::NewGuid().ToString('N'));$package=Join-Path $temp 'package';$installed=Join-Path $temp 'installed'
New-Item $package -ItemType Directory -Force|Out-Null
try{
 $files=[ordered]@{};foreach($name in $names){$path=Join-Path $package $name;[IO.File]::WriteAllText($path,('fixture-'+$name),(New-Object Text.UTF8Encoding($false)));$files[$name]=(Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant()}
 $manifest=[ordered]@{schema=2;revision=('a'*40);files=$files};[IO.File]::WriteAllText((Join-Path $package 'package.json'),($manifest|ConvertTo-Json -Depth 5));[IO.File]::WriteAllText((Join-Path $package 'source-revision.txt'),('a'*40))
 $manifestSha=(Get-FileHash (Join-Path $package 'package.json') -Algorithm SHA256).Hash.ToLowerInvariant();$info=Read-WorkerValidatedPackage $package $manifestSha
 $old=Join-Path $installed 'versions/old';[IO.Directory]::CreateDirectory($old)|Out-Null;[IO.File]::WriteAllText((Join-Path $old 'OrganizerWorker.exe'),'old-fixture');[IO.File]::WriteAllText((Join-Path $installed 'current.json'),(@{schema=1;directory=$old;sha256=(Get-FileHash (Join-Path $old 'OrganizerWorker.exe')).Hash.ToLowerInvariant()}|ConvertTo-Json))
 Invoke-WorkerPackageUpdate -Root $installed -Package $package -PackageManifestSha256 $manifestSha -StopWorker {} -StartWorker {} -VerifyWorker {$m=Get-Content (Join-Path $installed 'current.json') -Raw|ConvertFrom-Json;foreach($name in @('OrganizerWorkerTray.exe','start-headless.ps1')){if(-not (Test-Path (Join-Path $m.directory $name))){throw 'Headless file not published'}}}|Out-Null
 $current=Get-Content (Join-Path $installed 'current.json') -Raw|ConvertFrom-Json
 Check ((Get-FileHash (Join-Path $current.directory 'OrganizerWorkerTray.exe')).Hash -ieq $files['OrganizerWorkerTray.exe']) 'Published tray bytes changed'
 [IO.File]::AppendAllText((Join-Path $package 'OrganizerWorkerTray.exe'),'tamper');$threw=$false;try{Read-WorkerValidatedPackage $package $manifestSha|Out-Null}catch{$threw=$true};Check $threw 'Tampered headless tray accepted'
 [IO.File]::WriteAllText((Join-Path $package 'OrganizerWorkerTray.exe'),'fixture-OrganizerWorkerTray.exe',(New-Object Text.UTF8Encoding($false)))
 Remove-Item (Join-Path $package 'start-headless.ps1');$threw=$false;try{Read-WorkerValidatedPackage $package $manifestSha|Out-Null}catch{$threw=$true};Check $threw 'Incomplete headless package accepted'
 Check ((Get-FileHash (Join-Path $current.directory 'OrganizerWorkerTray.exe')).Hash -ieq $files['OrganizerWorkerTray.exe']) 'Candidate tampering changed immutable live version'
}finally{Remove-Item -LiteralPath $temp -Recurse -Force}
Write-Output 'UNATTENDED_POLICY_PASS strict readiness, same LIMITED password owner, preserved legacy package and fixed headless artifacts'
