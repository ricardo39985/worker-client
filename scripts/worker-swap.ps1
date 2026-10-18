# Dot-source only: filesystem publication for already validated version directories.
# Binaries are immutable. The only active-version commit point is current.json.
function Write-WorkerSwapFile([string]$Path,[byte[]]$Bytes) {
    $stream=[IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try {$stream.Write($Bytes,0,$Bytes.Length);$stream.Flush($true)} finally {$stream.Dispose()}
}
function Test-WorkerSwapManifest([string]$Root,$Manifest) {
    $allowed=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
    $directory=[IO.Path]::GetFullPath([string]$Manifest.directory)
    if($Manifest.schema -ne 1 -or -not $directory.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {throw 'Invalid version manifest path/schema'}
    $exe=Join-Path $directory 'OrganizerWorker.exe'
    if(-not [IO.File]::Exists($exe) -or (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ine [string]$Manifest.sha256) {throw 'Candidate/rollback worker missing or SHA256 mismatch'}
}
function Invoke-WorkerManifestReplace {
    param([string]$Source,[string]$Target,[string]$Backup,[int]$RetryMilliseconds=10000,[scriptblock]$OnSharingViolation)
    # All three names share a directory/volume. Never delete or move the live
    # manifest to make a replace succeed. Never pass PowerShell $null as backup.
    $watch=[Diagnostics.Stopwatch]::StartNew();$notified=$false
    while($true) {
        try {[IO.File]::Replace($Source,$Target,$Backup);return}
        catch {
            $errorObject=$_.Exception
            while($errorObject.InnerException){$errorObject=$errorObject.InnerException}
            $code=$errorObject.HResult -band 65535
            if($code -in @(32,33) -and $watch.ElapsedMilliseconds -lt $RetryMilliseconds -and [IO.File]::Exists($Source)) {
                if(-not $notified -and $OnSharingViolation){& $OnSharingViolation | Out-Null;$notified=$true}
                Start-Sleep -Milliseconds ([Math]::Min(100,[Math]::Max(1,$RetryMilliseconds-$watch.ElapsedMilliseconds)))
                continue
            }
            throw ('File.Replace("'+$Source+'", "'+$Target+'", "'+$Backup+'") failed; HRESULT='+$errorObject.HResult+' Win32='+$code+' elapsed_ms='+$watch.ElapsedMilliseconds+': '+$errorObject.Message)
        }
    }
}
function Restore-WorkerVersionCore {
    param([string]$Root,[scriptblock]$StopWorker,[scriptblock]$StartWorker,[scriptblock]$VerifyWorker,[int]$RetryMilliseconds=10000)
    $backup=Join-Path $Root 'current.rollback.json'
    if(-not [IO.File]::Exists($backup)){throw 'No pending version rollback exists'}
    $bytes=[IO.File]::ReadAllBytes($backup)
    $manifest=[Text.Encoding]::UTF8.GetString($bytes)|ConvertFrom-Json
    Test-WorkerSwapManifest $Root $manifest
    & $StopWorker
    $current=Join-Path $Root 'current.json'
    # Failed replace may leave current untouched, absent, or already changed.
    # Keep the immutable rollback copy until old-version readiness is proved.
    $same=[IO.File]::Exists($current) -and [Convert]::ToBase64String([IO.File]::ReadAllBytes($current)) -ceq [Convert]::ToBase64String($bytes)
    if(-not $same) {
        $temp=Join-Path $Root ('restore-'+[guid]::NewGuid().ToString('N')+'.tmp')
        $discard=$temp+'.bak'
        try {
            Write-WorkerSwapFile $temp $bytes
            if([IO.File]::Exists($current)){Invoke-WorkerManifestReplace $temp $current $discard -RetryMilliseconds $RetryMilliseconds}
            else {[IO.File]::Move($temp,$current)}
        } finally {if([IO.File]::Exists($temp)){[IO.File]::Delete($temp)};if([IO.File]::Exists($discard)){[IO.File]::Delete($discard)}}
    }
    & $StartWorker
    & $VerifyWorker
    [IO.File]::Delete($backup)
    Write-Output 'ROLLBACK_OLD_VERSION_VERIFIED=True'
}
function Restore-WorkerVersion {
    param([string]$Root,[scriptblock]$StopWorker,[scriptblock]$StartWorker,[scriptblock]$VerifyWorker,[int]$RetryMilliseconds=10000)
    $guard=[IO.File]::Open((Join-Path $Root 'version-swap.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try {Restore-WorkerVersionCore -Root $Root -StopWorker $StopWorker -StartWorker $StartWorker -VerifyWorker $VerifyWorker -RetryMilliseconds $RetryMilliseconds}
    finally {$guard.Dispose()}
}
function Invoke-WorkerVersionSwap {
    param([string]$Root,$Manifest,[scriptblock]$StopWorker,[scriptblock]$StartWorker,[scriptblock]$VerifyWorker,
          [scriptblock]$OnSharingViolation,[int]$RetryMilliseconds=10000)
    if(-not $StopWorker -or -not $StartWorker -or -not $VerifyWorker){throw 'Stop/start/readiness boundaries are required'}
    if($RetryMilliseconds -lt 0 -or $RetryMilliseconds -gt 30000){throw 'Retry bound must be 0..30000 milliseconds'}
    Test-WorkerSwapManifest $Root $Manifest
    $current=Join-Path $Root 'current.json';$backup=Join-Path $Root 'current.rollback.json'
    # Exclusive transaction lock survives no process; recovery bytes do.
    $guard=[IO.File]::Open((Join-Path $Root 'version-swap.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $temp=Join-Path $Root ('publish-'+[guid]::NewGuid().ToString('N')+'.tmp');$replaceBackup=$temp+'.bak'
    try {
        if([IO.File]::Exists($backup)){throw 'Interrupted update pending. Recover the old version before another update.'}
        $old=[IO.File]::ReadAllBytes($current)
        Test-WorkerSwapManifest $Root ([Text.Encoding]::UTF8.GetString($old)|ConvertFrom-Json)
        Write-WorkerSwapFile $temp ([Text.Encoding]::UTF8.GetBytes(($Manifest|ConvertTo-Json -Depth 16)))
        # Publish recovery bytes only after the complete copy reaches disk.
        # A process death while writing must not expose a partial recovery file.
        Write-WorkerSwapFile ($temp+'.rollback') $old
        [IO.File]::Move(($temp+'.rollback'),$backup)
        try {
            & $StopWorker
            Invoke-WorkerManifestReplace $temp $current $replaceBackup -RetryMilliseconds $RetryMilliseconds -OnSharingViolation $OnSharingViolation
            & $StartWorker
            & $VerifyWorker
            # Removing the durable recovery marker commits the transaction.
            # A failure here must roll back as well, rather than report failure
            # while leaving the candidate selected.
            [IO.File]::Delete($backup)
        } catch {
            $failure=$_.Exception.Message
            try {Restore-WorkerVersionCore -Root $Root -StopWorker $StopWorker -StartWorker $StartWorker -VerifyWorker $VerifyWorker -RetryMilliseconds $RetryMilliseconds}
            catch {throw ($failure+'; rollback pending at '+$backup+': '+$_.Exception.Message)}
            throw ($failure+'; previous version restored and verified')
        }
        Write-Output 'VERSION_SWAP_VERIFIED=True'
    } finally {
        try {
            foreach($path in @($temp,($temp+'.rollback'),$replaceBackup)) {
                try {if([IO.File]::Exists($path)){[IO.File]::Delete($path)}}
                catch {Write-Warning ('Retained inactive swap temporary file: '+$path+'; '+$_.Exception.Message)}
            }
        } finally {$guard.Dispose()}
    }
}
