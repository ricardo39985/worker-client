#requires -Version 5.1
[CmdletBinding()]
param([string]$Root=(Join-Path $env:LOCALAPPDATA 'OrganizerWorker'),[string]$Package,
      [string]$PackageManifestSha256,[switch]$RecoverOnly,
      [string]$CapacityProfile,[string]$CapacityProfileSha256)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. (Join-Path $PSScriptRoot 'worker-swap.ps1')
. (Join-Path $PSScriptRoot 'setup-policy.ps1')
$rootPath=[IO.Path]::GetFullPath($Root)
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$taskName='OrganizerWorker-'+$identity.User.Value
$task=Get-ScheduledTask -TaskName $taskName -ErrorAction Stop
if([string]$task.Principal.RunLevel -notin @('Limited','0') -or [string]$task.Principal.LogonType -notin @('Interactive','3') -or
   -not (Test-WorkerAccountSid -Account $task.Principal.UserId -ExpectedSid $identity.User.Value)){throw 'Existing startup task must belong to this user and be limited/interactive'}
# Operational extension: the version-swap implementation is unchanged. Config
# publication follows the selected version, including rollback. No preferences,
# origins, approved hosts, dependency pins or protected identities are changed.
$version=$null
$configPath=Join-Path $Root 'worker.json'
$configRollback=Join-Path $Root 'capacity.rollback.json'
if((Test-Path $configRollback) -and -not $RecoverOnly){throw 'Pending capacity rollback; recover before another install'}
$oldConfigBytes=if(Test-Path $configRollback){[IO.File]::ReadAllBytes($configRollback)}else{[IO.File]::ReadAllBytes($configPath)}
$newConfigBytes=$oldConfigBytes
if(-not $RecoverOnly){
    if($CapacityProfileSha256 -notmatch '\A[a-f0-9]{64}\z' -or (Get-FileHash -LiteralPath $CapacityProfile -Algorithm SHA256).Hash -ine $CapacityProfileSha256){throw 'Explicitly verified measured-capacity profile required'}
    $capacity=Get-Content -LiteralPath $CapacityProfile -Raw|ConvertFrom-Json
    $cfg=[Text.Encoding]::UTF8.GetString($oldConfigBytes)|ConvertFrom-Json
    foreach($name in 'cpu_threads','ram_mb','scratch_mb','max_jobs'){
        $v=$capacity.$name
        if($v -isnot [int] -and $v -isnot [long]){throw ('Non-integer capacity: '+$name)}
        if($v -le 0){throw ('Non-positive capacity: '+$name)}
        $cfg.limits.$name=$v
    }
    if($capacity.cpu_threads -gt 64 -or $capacity.max_jobs -gt 64 -or $capacity.ram_mb -lt 512 -or $capacity.ram_mb -gt 1048576){throw 'Capacity outside worker contract'}
    $available=[long]((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1024)
    if($capacity.ram_mb -gt [long]($available*0.95)){throw 'Measured RAM ceiling no longer has live headroom; defer installation'}
    $newConfigBytes=[Text.Encoding]::UTF8.GetBytes(($cfg|ConvertTo-Json -Depth 16))
}
$publishConfig={
    $selected=(Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json).directory
    $bytes=if($version -and $selected -ceq $version){$newConfigBytes}else{$oldConfigBytes}
    $temp=Join-Path $Root ('capacity-'+[guid]::NewGuid().ToString('N')+'.tmp')
    $backup=$temp+'.bak'
    try{Write-WorkerSwapFile $temp $bytes;Invoke-WorkerManifestReplace $temp $configPath $backup}
    finally{if(Test-Path $temp){Remove-Item -LiteralPath $temp -Force};if(Test-Path $backup){Remove-Item -LiteralPath $backup -Force}}
}
$hashes=@{}
foreach($name in 'credential.dpapi','pairing-key.dpapi'){$hashes[$name]=(Get-FileHash (Join-Path $Root $name)).Hash}
$checkIdentity={foreach($name in $hashes.Keys){if($hashes[$name] -cne (Get-FileHash (Join-Path $Root $name)).Hash){throw 'Identity/configuration changed'}}}
$stop={
    $status=Get-Content (Join-Path $Root 'worker-status.txt') -Raw
    if($status -notmatch 'jobs\s+running 0' -or (Get-Process ffmpeg -ErrorAction SilentlyContinue)){throw 'Worker/encoder is busy; defer update'}
    $allowed=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
    foreach($p in @(Get-Process OrganizerWorker -ErrorAction SilentlyContinue)) {
        if(-not $p.Path.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected worker process path'}
        $null=$p.Handle
        Stop-Process -Id $p.Id -ErrorAction Stop
        if(-not $p.WaitForExit(10000)){throw 'Worker did not terminate within 10 seconds'}
        $p.Dispose()
    }
    Write-Output 'WORKER_STOPPED_IDLE=True'
}
$start={
    & $publishConfig
    & $checkIdentity
    & schtasks.exe /Run /TN $taskName | Out-Null
    $code=$LASTEXITCODE;Write-Output "START_TASK_EXIT=$code"
    if($code){throw ('Scheduled worker start failed, exit='+$code)}
}
$verify={
    $c=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
    Test-WorkerSwapManifest $Root $c
    $exe=Join-Path $c.directory 'OrganizerWorker.exe'
    $until=[datetime]::UtcNow.AddSeconds(1800)
    do {
        if(Test-WorkerRuntime -Root $Root -ExpectedExecutable $exe) {
            $runtime=Get-Content (Join-Path $Root 'runtime.json') -Raw|ConvertFrom-Json
            $p=Get-Process -Id $runtime.process_id -ErrorAction Stop
            $statusFile=Get-Item (Join-Path $Root 'worker-status.txt') -ErrorAction SilentlyContinue
            if($p.SessionId -ne 0 -and $statusFile -and $statusFile.LastWriteTimeUtc -ge $p.StartTime.ToUniversalTime()) {
                $status=Get-Content $statusFile.FullName -Raw
                $lines=@(Get-Content (Join-Path $Root 'worker.log'))
                $begin=-1
                for($i=$lines.Count-1;$i -ge 0;$i--){if($lines[$i] -match 'WORKER STARTED \| counts since this process'){$begin=$i;break}}
                $epoch=@()
                if($begin -ge 0){$epoch=@($lines[$begin..($lines.Count-1)] | Where-Object {$_ -match 'STATE (CONNECTED|DISCONNECTED)'})}
                if($epoch.Count -gt 0 -and $epoch[-1] -match 'STATE CONNECTED' -and $status -match 'state\s+accepting') {
                    & $checkIdentity
                    Write-Output ('VERIFIED_SHA256='+$c.sha256+' PROCESS_PID='+$p.Id+' SESSION='+$p.SessionId)
                    return
                }
            }
        }
        Start-Sleep -Milliseconds 500
    } while([datetime]::UtcNow -lt $until)
    throw ('Installed worker did not become connected/accepting: '+$exe)
}
if($RecoverOnly) {
    Restore-WorkerVersion -Root $Root -StopWorker $stop -StartWorker $start -VerifyWorker $verify
    if(Test-Path $configRollback){Remove-Item -LiteralPath $configRollback -Force}
    exit 0
}
if([IO.File]::Exists((Join-Path $Root 'current.rollback.json'))){throw 'Interrupted update pending; run -RecoverOnly before installing'}
if($PackageManifestSha256 -notmatch '\A[a-fA-F0-9]{64}\z' -or
   (Get-FileHash (Join-Path $Package 'package.json')).Hash -ine $PackageManifestSha256){throw 'Verified package manifest SHA256 is required'}
$packageInfo=Get-Content (Join-Path $Package 'package.json') -Raw|ConvertFrom-Json
if($packageInfo.revision -notmatch '\A[a-f0-9]{40}\z'){throw 'Full source revision required'}
$names=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe')
foreach($name in $names){
    $expected=[string]$packageInfo.files.$name
    if($expected -notmatch '\A[a-fA-F0-9]{64}\z' -or (Get-FileHash (Join-Path $Package $name)).Hash -ine $expected){throw ('Package SHA256 mismatch: '+$name)}
}
$sha=([string]$packageInfo.files.'OrganizerWorker.exe').ToLowerInvariant()
$version=Join-Path $Root ('versions\0.1.2-'+$sha.Substring(0,16))
if(Test-Path $version){throw ('Candidate version already exists; refusing to overwrite: '+$version)}
$staging=$version+'.staging-'+[guid]::NewGuid().ToString('N')
New-Item -ItemType Directory -Path $staging | Out-Null
try {
    foreach($name in $names){
        Copy-Item (Join-Path $Package $name) (Join-Path $staging $name)
        if((Get-FileHash (Join-Path $staging $name)).Hash -ine [string]$packageInfo.files.$name){throw ('Staged SHA256 mismatch: '+$name)}
    }
    [IO.File]::WriteAllText((Join-Path $staging 'source-revision.txt'),$packageInfo.revision)
    [IO.Directory]::Move($staging,$version)
} finally {if(Test-Path $staging){Remove-Item $staging -Recurse -Force}}
Write-WorkerSwapFile $configRollback $oldConfigBytes
try {
    Invoke-WorkerVersionSwap -Root $Root -Manifest ([ordered]@{schema=1;directory=$version;sha256=$sha}) -StopWorker $stop -StartWorker $start -VerifyWorker $verify
} catch {
    if(-not (Test-Path (Join-Path $Root 'current.rollback.json')) -and
       [Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath)) -ceq [Convert]::ToBase64String($oldConfigBytes)) {
        if(Test-Path $configRollback){Remove-Item -LiteralPath $configRollback -Force}
    }
    throw
}
$readback=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
if($readback.directory -cne $version -or $readback.sha256 -cne $sha){throw 'Published manifest differs from the verified package'}
Write-Output ('INSTALLED_VERSION='+[IO.Path]::GetFileName($version))
Write-Output ('INSTALLED_SHA256='+(Get-FileHash (Join-Path $readback.directory 'OrganizerWorker.exe')).Hash.ToLowerInvariant())
Write-Output ('INSTALLED_REVISION='+(Get-Content (Join-Path $readback.directory 'source-revision.txt') -Raw))
$expectedHash=[Convert]::ToBase64String($newConfigBytes)
if([Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath)) -cne $expectedHash){throw 'Qualified config publication differs'}
if(Test-Path $configRollback){Remove-Item -LiteralPath $configRollback -Force}
Write-Output 'PROTECTED_IDENTITY_AND_NONLIMIT_SETTINGS_UNCHANGED=True'
Write-Output ('QUALIFIED_LIMITS='+($capacity|ConvertTo-Json -Compress))
