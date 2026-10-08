# A bundle comes from an explicitly obtained release ZIP; never a network command.
function Test-WorkerBundle([string]$Bundle) {
    $manifestPath=Join-Path $Bundle 'manifest.json'
    if(-not (Test-Path -LiteralPath $manifestPath)){throw 'Binary bundle manifest is missing.'}
    $m=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if($m.schema -ne 1 -or $m.architecture -ne 'x64' -or $m.version -ne '0.1.3' -or $m.native_tests -ne 'passed'){throw 'Unsupported or unverified Windows bundle.'}
    $expected=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe')
    if(@($m.files).Count -ne $expected.Count){throw 'Unexpected Windows bundle contents.'}
    $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach($file in $m.files){
        if($file.path -notin $expected -or -not $seen.Add($file.path) -or $file.sha256 -notmatch '^[a-f0-9]{64}$'){throw 'Invalid binary bundle manifest.'}
        $path=Join-Path $Bundle $file.path
        if(-not (Test-Path -LiteralPath $path -PathType Leaf) -or ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw ('Bundle file missing or linked: '+$file.path)}
        if((Get-WorkerFileSha256 $path) -ine $file.sha256){throw ('Bundle checksum mismatch: '+$file.path)}
    }
    return $m
}
function Install-WorkerBundle([string]$Bundle,[string]$Root) {
    $m=Test-WorkerBundle $Bundle
    $id=(Get-WorkerFileSha256 (Join-Path $Bundle 'manifest.json') -Quiet).Substring(0,16)
    $version=Join-Path $Root ('versions\0.1.3-'+$id)
    New-Item -ItemType Directory -Force -Path $version | Out-Null
    foreach($file in $m.files){Copy-Item -LiteralPath (Join-Path $Bundle $file.path) -Destination (Join-Path $version $file.path) -Force}
    Copy-Item -LiteralPath (Join-Path $Bundle 'manifest.json') -Destination (Join-Path $version 'manifest.json') -Force
    Test-WorkerBundle $version | Out-Null
    return $version
}
function Invoke-WorkerInstallTransaction([string]$Root,[scriptblock]$Publish,[scriptblock]$Verify,[scriptblock]$StopCandidate) {
    # The pointer/config/launcher are the installation transaction. Identity,
    # journal, preferences and dependencies are never rolled back or removed.
    $paths=@('current.json','worker.json','start-worker.ps1','setup-policy.ps1','setup-progress.ps1')
    $backup=Join-Path $Root ('update-backup-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $backup | Out-Null
    foreach($name in $paths){$p=Join-Path $Root $name;if(Test-Path -LiteralPath $p){Copy-Item -LiteralPath $p -Destination (Join-Path $backup $name)}}
    $restoreCompleted=$false
    try { & $Publish;& $Verify;$restoreCompleted=$true }
    catch {
        & $StopCandidate
        foreach($name in $paths){$p=Join-Path $Root $name;$b=Join-Path $backup $name;if(Test-Path -LiteralPath $b){Copy-Item -LiteralPath $b -Destination $p -Force}else{Remove-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue}}
        Write-WorkerSetupLine '[ROLLBACK] Previous configuration and version pointer restored; identity and journal retained.'
        $restoreCompleted=$true
        throw
    } finally {if($restoreCompleted){Remove-Item -LiteralPath $backup -Recurse -Force}}
}
