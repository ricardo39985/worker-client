# Immutable package staging and shared transactional publication for production
# updater and isolated legacy upgrade contracts. Host stop/start/readiness ports
# are required; no alternate pointer/file-copy bypass exists.
function Get-WorkerPackageNames($Info) {
    $schema=if($Info.PSObject.Properties['schema']){[int]$Info.schema}else{1}
    if($schema -notin @(1,2)){throw 'Unknown worker package schema'}
    $names=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe','cli.md')
    if($schema -eq 2){$names+=@('OrganizerWorkerTray.exe','start-headless.ps1')}
    return $names
}
function Read-WorkerValidatedPackage {
    param([string]$Package,[string]$PackageManifestSha256)
    if($PackageManifestSha256 -notmatch '\A[a-fA-F0-9]{64}\z' -or (Get-FileHash (Join-Path $Package 'package.json')).Hash -ine $PackageManifestSha256){throw 'Verified package manifest SHA256 required'}
    $info=Get-Content (Join-Path $Package 'package.json') -Raw|ConvertFrom-Json
    if($info.revision -notmatch '\A[a-f0-9]{40}\z'){throw 'Full source revision required'}
    $names=@(Get-WorkerPackageNames $info)
    foreach($name in $names){
        $expected=[string]$info.files.$name
        if($expected -notmatch '\A[a-fA-F0-9]{64}\z' -or (Get-FileHash (Join-Path $Package $name)).Hash -ine $expected){throw ('Package SHA256 mismatch: '+$name)}
    }
    return $info
}
function Invoke-WorkerPackageUpdate {
    param([string]$Root,[string]$Package,[string]$PackageManifestSha256,
          [scriptblock]$StopWorker,[scriptblock]$StartWorker,[scriptblock]$VerifyWorker)
    if(-not $StopWorker -or -not $StartWorker -or -not $VerifyWorker){throw 'Cooperative stop/start/readiness ports are required'}
    if([IO.File]::Exists((Join-Path $Root 'current.rollback.json'))){throw 'Interrupted update pending; recover before another install'}
    $info=Read-WorkerValidatedPackage $Package $PackageManifestSha256
    $names=@(Get-WorkerPackageNames $info)
    $sha=([string]$info.files.'OrganizerWorker.exe').ToLowerInvariant()
    $version=Join-Path $Root ('versions\0.1.2-'+$sha.Substring(0,16))
    if(Test-Path $version){
        foreach($name in $names){if((Get-FileHash (Join-Path $version $name)).Hash -ine [string]$info.files.$name){throw 'Existing immutable version differs; refusing overwrite'}}
        if((Get-Content (Join-Path $version 'source-revision.txt') -Raw) -cne $info.revision){throw 'Existing immutable source marker differs'}
    } else {
        $stage=$version+'.staging-'+[guid]::NewGuid().ToString('N');New-Item -ItemType Directory -Path $stage | Out-Null
        try {
            foreach($name in $names){Copy-Item (Join-Path $Package $name) (Join-Path $stage $name);if((Get-FileHash (Join-Path $stage $name)).Hash -ine [string]$info.files.$name){throw 'Staged package mismatch'}}
            [IO.File]::WriteAllText((Join-Path $stage 'source-revision.txt'),$info.revision)
            [IO.Directory]::Move($stage,$version)
        } finally {if(Test-Path $stage){Remove-Item -LiteralPath $stage -Recurse -Force}}
    }
    $current=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
    if($current.directory -ceq $version -and $current.sha256 -ceq $sha){& $VerifyWorker;Write-Output 'ALREADY_SELECTED_VERIFIED=True';return}
    Invoke-WorkerVersionSwap -Root $Root -Manifest ([ordered]@{schema=1;directory=$version;sha256=$sha}) -StopWorker $StopWorker -StartWorker $StartWorker -VerifyWorker $VerifyWorker
    $readback=Get-Content (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
    if($readback.directory -cne $version -or $readback.sha256 -cne $sha){throw 'Published manifest differs from verified package'}
    Write-Output ('INSTALLED_VERSION='+[IO.Path]::GetFileName($version))
    Write-Output ('INSTALLED_SHA256='+$sha)
    Write-Output ('INSTALLED_REVISION='+$info.revision)
}
