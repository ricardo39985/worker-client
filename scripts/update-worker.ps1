#requires -Version 5.1
[CmdletBinding()]
param([string]$Root=(Join-Path $env:LOCALAPPDATA 'OrganizerWorker'),[string]$Package,
      [string]$PackageManifestSha256,[switch]$RecoverOnly,
      [ValidateSet('preserve','resume','paused')][string]$AdmissionIntent='preserve',
      [string]$StartupTaskName='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. (Join-Path $PSScriptRoot 'worker-swap.ps1')
. (Join-Path $PSScriptRoot 'worker-package.ps1')
. (Join-Path $PSScriptRoot 'worker-update-policy.ps1')
. (Join-Path $PSScriptRoot 'setup-policy.ps1')
. (Join-Path $PSScriptRoot 'worker-launch-policy.ps1')
. (Join-Path $PSScriptRoot 'setup-startup.ps1')
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
if($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Updater requires existing LIMITED owner, not an Administrator token'}
$unattended=(Get-WorkerStartupMode $Root $identity.User.Value) -eq 'unattended';$enrollment=$null
if($unattended){
    $selected=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json;Test-WorkerSwapManifest $Root $selected
    $currentCtl=Join-Path $selected.directory 'OrganizerWorkerCtl.exe'
    $raw=(& $currentCtl --json enrollment)-join "`n";if($LASTEXITCODE){throw 'Existing unattended enrollment invalid; LOCAL secure recovery required, no fallback/re-pair'};$enrollment=$raw|ConvertFrom-Json
    $expectedTask='OrganizerWorker-Unattended-'+$identity.User.Value
    if($StartupTaskName -and $StartupTaskName -cne $expectedTask){throw 'Cannot replace enrolled owner task with another startup action'}
    $StartupTaskName=$expectedTask
} elseif(-not $StartupTaskName){$StartupTaskName='OrganizerWorker-'+$identity.User.Value}
$task=Get-ScheduledTask -TaskName $StartupTaskName -ErrorAction Stop
Assert-WorkerLaunchTask -Task $task -OwnerSid $identity.User.Value -Unattended $unattended -Enrollment $enrollment
$allowed=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
$findWorkers={@(Get-Process OrganizerWorker -ErrorAction SilentlyContinue|Where-Object {$_.Path -and $_.Path.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)})}
# Reject unsupported old trays before staging/publishing anything. The owner must
# choose the real tray Exit/finish action; window injection/force-kill is not used.
Assert-WorkerCooperativeExitAvailable -Root $Root -Workers @(& $findWorkers)
if(Get-Process ffmpeg -ErrorAction SilentlyContinue){throw 'Encoder active; defer update without stopping any process'}
$hashes=@{};foreach($name in 'credential.dpapi','pairing-key.dpapi'){$hashes[$name]=(Get-FileHash (Join-Path $Root $name)).Hash}
$beforeConfig=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json
$expectedOperator=$null
function Invoke-WorkerJson([string]$Executable,[string[]]$Arguments) {
    $raw=(& $Executable --json @Arguments) -join "`n";$code=$LASTEXITCODE
    if($code){throw ('Worker control/inspection failed, exit='+$code)}
    return $raw|ConvertFrom-Json
}
if(-not $RecoverOnly){
    $info=Read-WorkerValidatedPackage $Package $PackageManifestSha256
    if($unattended -and 'start-headless.ps1' -notin @(Get-WorkerPackageNames $info)){throw 'Cannot downgrade an enrolled headless owner to a legacy interactive-only package'}
    if($unattended){Assert-WorkerApprovedCandidate -Info $info -Enrollment $enrollment} # DATA only; never run candidateCtl to establish its own authority
    # Whitelisted public preferences, read-only SQLite, no credentials/key reads.
    $inspectionDoctor=if($unattended){Join-Path $selected.directory 'OrganizerWorkerDoctor.exe'}else{Join-Path $Package 'OrganizerWorkerDoctor.exe'}
    $inspection=Invoke-WorkerJson $inspectionDoctor -Arguments @('--operator-settings','--data-root',$Root)
    $expectedOperator=$inspection.operator
    if($AdmissionIntent -ne 'preserve'){$expectedOperator.paused=($AdmissionIntent -eq 'paused')}
}
$retention=[pscustomobject]@{operator=$expectedOperator;paused=$(if($expectedOperator){[bool]$expectedOperator.paused}else{$null})}
$checkIdentity={
    foreach($name in $hashes.Keys){if($hashes[$name] -cne (Get-FileHash (Join-Path $Root $name)).Hash){throw 'Protected identity bytes changed'}}
    $after=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json
    $op=if($after.PSObject.Properties['operator']){$retention.operator}else{$null}
    Test-WorkerRetainedConfiguration $beforeConfig $after $op
}
$stop={
    Invoke-WorkerCooperativeStop -Root $Root -FindWorkers $findWorkers -RequestDrain {
        param($ctl)
        $null=Invoke-WorkerJson $ctl -Arguments @('--data-root',$Root,'exit','drain')
    } -WaitSeconds 600
    if(Get-Process ffmpeg -ErrorAction SilentlyContinue){throw 'Encoder active; defer update without terminating it'}
    $guard=[IO.File]::Open((Join-Path $Root 'instance.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$guard.Dispose()
    if($retention.operator -and -not $RecoverOnly){
        $latest=Invoke-WorkerJson $inspectionDoctor -Arguments @('--operator-settings','--data-root',$Root)
        $latest.operator.paused=$retention.paused;$retention.operator=$latest.operator
    }
    Write-Output 'WORKER_STOPPED_COOPERATIVELY=True'
}
$start={
    foreach($name in $hashes.Keys){if($hashes[$name] -cne (Get-FileHash (Join-Path $Root $name)).Hash){throw 'Protected identity bytes changed'}}
    $currentConfig=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json
    Test-WorkerRetainedConfiguration $beforeConfig $currentConfig $null $true
    & schtasks.exe /Run /TN $StartupTaskName | Out-Null
    $code=$LASTEXITCODE;Write-Output "START_TASK_EXIT=$code";if($code){throw ('Existing startup task failed, exit='+$code)}
}
$verify={
    $c=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
    Test-WorkerSwapManifest $Root $c
    $exe=Join-Path $c.directory 'OrganizerWorker.exe';$ctl=Join-Path $c.directory 'OrganizerWorkerCtl.exe'
    $until=[DateTime]::UtcNow.AddSeconds(600);$intentApplied=$false
    do {
        if(Test-WorkerRuntime -Root $Root -ExpectedExecutable $exe){
            $runtime=Get-Content (Join-Path $Root 'runtime.json') -Raw|ConvertFrom-Json
            $p=Get-Process -Id $runtime.process_id -ErrorAction Stop
            if(-not $unattended -and $p.SessionId -eq 0){throw 'Legacy worker is not in the owner interactive session'}
            if(Test-Path $ctl){
                try {
                    if(-not $intentApplied -and $retention.operator){
                        $action=if($retention.operator.paused){'pause'}else{'resume'}
                        $null=Invoke-WorkerJson $ctl -Arguments @('--data-root',$Root,$action);$intentApplied=$true
                    }
                    $status=Invoke-WorkerJson $ctl -Arguments @('--data-root',$Root,'status')
                    $wantPaused=if($retention.operator){[bool]$retention.operator.paused}else{[bool]$status.paused}
                    if($status.pid -eq $p.Id -and $status.owner_sid -eq $identity.User.Value -and (Test-WorkerLaunchReady -Status $status -Unattended $unattended) -and $status.paused -eq $wantPaused -and ($wantPaused -or $status.accepting)){
                        & $checkIdentity
                        if($retention.operator){$after=Get-Content (Join-Path $Root 'worker.json') -Raw|ConvertFrom-Json;Test-WorkerRetainedConfiguration $beforeConfig $after $retention.operator}
                        Write-Output ('VERIFIED_SHA256='+$c.sha256+' PROCESS_PID='+$p.Id+' SESSION='+$p.SessionId+' CONNECTED=True PAUSED='+$status.paused)
                        return
                    }
                } catch {if([DateTime]::UtcNow -ge $until){throw}}
            } else {
                # Legacy rollback can be healthy but has no remote control. Never
                # pretend it is the candidate or fabricate a resume of that tray.
                $statusFile=Get-Item (Join-Path $Root 'worker-status.txt') -ErrorAction SilentlyContinue
                if($statusFile -and $statusFile.LastWriteTimeUtc -ge $p.StartTime.ToUniversalTime()){
                    $lines=@(Get-Content (Join-Path $Root 'worker.log') -Tail 3000);$begin=-1
                    for($n=$lines.Count-1;$n -ge 0;$n--){if($lines[$n] -match 'WORKER STARTED \| counts since this process'){$begin=$n;break}}
                    if($begin -ge 0){
                        $epoch=@($lines[$begin..($lines.Count-1)]|Where-Object {$_ -match 'STATE (CONNECTED|RECONNECTED|DISCONNECTED|OFFLINE|REVOKED|ACTION)'})
                        if($epoch.Count -and $epoch[-1] -match 'STATE (CONNECTED|RECONNECTED)') {& $checkIdentity;Write-Output 'LEGACY_ROLLBACK_RUNNING=True';return}
                    }
                }
            }
        }
        Start-Sleep -Milliseconds 500
    } while([DateTime]::UtcNow -lt $until)
    throw 'Installed worker did not become connected with preserved admission intent within ten minutes'
}
if($RecoverOnly){Restore-WorkerVersion -Root $Root -StopWorker $stop -StartWorker $start -VerifyWorker $verify;exit 0}
Invoke-WorkerPackageUpdate -Root $Root -Package $Package -PackageManifestSha256 $PackageManifestSha256 -StopWorker $stop -StartWorker $start -VerifyWorker $verify
Write-Output 'PROTECTED_IDENTITY_AND_EXISTING_SETTINGS_RETAINED=True'
