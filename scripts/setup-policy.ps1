# Pure setup decisions and explicitly injected operating-system boundaries.
# Dot-sourcing this file does not install, launch, prompt, or write files.
function Get-WorkerPairingAction([string]$Origin,[long]$NowMs,[scriptblock]$ReadSecret) {
    try {
        $credential=& $ReadSecret 'credential'
        if ($credential) {
            if ((ConvertTo-WorkerOrigin $credential.coordinator_url) -ne $Origin) { return @{kind='archive';reason='Saved credential belongs to another endpoint.'} }
            return @{kind='none';reason='Saved credential retained.'}
        }
        $pending=& $ReadSecret 'pending'
        if (-not $pending) { return @{kind='none';reason='New pairing is required.'} }
        if ((ConvertTo-WorkerOrigin $pending.coordinator_url) -ne $Origin) { return @{kind='archive';reason='Pending pairing belongs to another endpoint.'} }
        if (($pending.PSObject.Properties.Name -contains 'terminal') -or [long]$pending.expires_at_ms -le $NowMs) { return @{kind='re-pair';reason='Previous pairing expired or was denied.'} }
        return @{kind='none';reason='Pending pairing retained.'}
    } catch { return @{kind='archive';reason='Saved identity cannot be read safely; existing bytes were preserved.'} }
}
function Test-WorkerAccountSid([string]$Account,[string]$ExpectedSid) {
    if ([string]::IsNullOrWhiteSpace($Account) -or [string]::IsNullOrWhiteSpace($ExpectedSid)) { return $false }
    try {
        if ($Account -match '^S-1-') {
            $identity=New-Object Security.Principal.SecurityIdentifier -ArgumentList $Account
        } else {
            $name=New-Object Security.Principal.NTAccount -ArgumentList $Account
            $identity=$name.Translate([Security.Principal.SecurityIdentifier])
        }
        return $identity.Value -eq $ExpectedSid
    } catch { return $false }
}
function ConvertTo-WorkerOrigin([string]$Value) {
    $valueText=([string]$Value).Trim([char[]]" `t`r`n")
    if ($valueText -match '\A[a-zA-Z0-9.-]+(?::443)?/?\z') { $valueText='https://'+$valueText }
    $match=[regex]::Match($valueText,'\Ahttps://([a-zA-Z0-9.-]+)(?::443)?/?\z',[Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) { throw 'Enter an HTTPS worker origin on port 443, without a path, query, fragment or credentials.' }
    $hostName=$match.Groups[1].Value.ToLowerInvariant()
    if ($hostName.Length -gt 253 -or $hostName.EndsWith('.')) { throw 'Invalid worker endpoint hostname.' }
    foreach ($label in $hostName.Split('.')) {
        if ($label -notmatch '\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\z') { throw 'Invalid worker endpoint hostname.' }
    }
    return 'https://'+$hostName
}
function Resolve-WorkerOrigin([string]$Stored,[string]$Requested,[bool]$HasIdentity,[switch]$RePair,[switch]$NonInteractive,[scriptblock]$Prompt) {
    $value=$Requested
    if ([string]::IsNullOrWhiteSpace($value)) { $value=$Stored }
    if ([string]::IsNullOrWhiteSpace($value)) {
        if ($NonInteractive -or -not $Prompt) { throw 'Worker endpoint is required. Supply -Server or run setup interactively.' }
        for ($attempt=0;$attempt -lt 3;$attempt++) {
            $value=& $Prompt
            try { $value=ConvertTo-WorkerOrigin $value;break }
            catch { if ($attempt -eq 2) { throw };Write-Host $_.Exception.Message }
        }
    }
    $origin=ConvertTo-WorkerOrigin $value
    if ($HasIdentity -and -not $RePair) {
        if ([string]::IsNullOrWhiteSpace($Stored) -or (ConvertTo-WorkerOrigin $Stored) -ne $origin) {
            throw 'Saved identity belongs to another endpoint. Use explicit -RePair; no identity was changed.'
        }
    }
    return $origin
}
function Complete-WorkerSetup([switch]$NoStartup,[switch]$NoLaunch,[scriptblock]$RegisterStartup,[scriptblock]$VerifyStartup,[scriptblock]$LaunchWorker) {
    if (-not $NoStartup) {
        Invoke-WorkerRepair -Operation {
            & $RegisterStartup | Out-Null
            if (-not (& $VerifyStartup)) { throw 'Windows startup registration could not be verified. Setup is incomplete.' }
        } -Repair { Write-Host '[REPAIR] Re-registering this users normal-privilege startup task.' } | Out-Null
    }
    if (-not $NoLaunch) { & $LaunchWorker | Out-Null }
    return [pscustomobject]@{startup_registered=(-not $NoStartup);started=(-not $NoLaunch)}
}
function Invoke-WorkerRepair([scriptblock]$Operation,[scriptblock]$Repair) {
    try { return (& $Operation) }
    catch {
        $first=$_.Exception.Message
        & $Repair | Out-Null
        try { return (& $Operation) }
        catch { throw ('Repair did not resolve the problem. First failure: '+$first+'. Final failure: '+$_.Exception.Message) }
    }
}
function Get-WorkerVerifiedFile([string]$Destination,[string]$Sha256,[scriptblock]$Download) {
    if ($Sha256 -notmatch '\A[a-fA-F0-9]{64}\z') { throw 'Invalid dependency checksum lock.' }
    if (Test-Path -LiteralPath $Destination) {
        if ((Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash -ieq $Sha256) { return }
        Remove-Item -LiteralPath $Destination -Force
        Write-Host '[REPAIR] Replacing a damaged download from the reviewed dependency lock.'
    }
    $partial=$Destination+'.'+[guid]::NewGuid().ToString('N')+'.part'
    try {
        & $Download $partial | Out-Null
        if (-not (Test-Path -LiteralPath $partial) -or (Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ine $Sha256) { throw 'Downloaded dependency checksum mismatch; refusing to extract or execute.' }
        [IO.File]::Move($partial,$Destination)
    } finally { if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force } }
}
function Write-WorkerJson([string]$Path,$Value,[scriptblock]$ReplaceFile) {
    $temp=$Path+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
    $backup=$Path+'.'+[guid]::NewGuid().ToString('N')+'.bak'
    $published=$false
    try {
        $utf8=New-Object System.Text.UTF8Encoding($false)
        [IO.File]::WriteAllText($temp,($Value | ConvertTo-Json -Depth 16),$utf8)
        if (Test-Path -LiteralPath $Path) {
            if ($ReplaceFile) { & $ReplaceFile $temp $Path $backup }
            else { [IO.File]::Replace($temp,$Path,$backup) }
        } else { [IO.File]::Move($temp,$Path) }
        $published=$true
    } finally {
        if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Force }
        # Preserve a recovery copy if a filesystem operation failed ambiguously.
        if ($published -and (Test-Path -LiteralPath $backup)) { Remove-Item -LiteralPath $backup -Force }
    }
}
function Test-WorkerRuntime([string]$Root,[string]$ExpectedExecutable,[scriptblock]$GetProcess) {
    try {
        $path=Join-Path $Root 'runtime.json'
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $false }
        $runtime=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
        if ($runtime.schema -ne 1 -or $runtime.state -ne 'running' -or [long]$runtime.process_id -le 0) { return $false }
        if ($GetProcess) { $process=& $GetProcess ([int]$runtime.process_id) }
        else { $process=Get-Process -Id ([int]$runtime.process_id) -ErrorAction Stop }
        if (-not $process -or $process.HasExited) { return $false }
        return ([IO.Path]::GetFullPath([string]$process.Path).Equals([IO.Path]::GetFullPath($ExpectedExecutable),[StringComparison]::OrdinalIgnoreCase) -and
            $process.StartTime.ToUniversalTime().ToFileTimeUtc() -eq [long]$runtime.process_started_filetime)
    } catch { return $false }
}
function Archive-WorkerPairing([string]$Root,[switch]$WholeCoordinator) {
    $names=@('credential.dpapi','pending-pairing.dpapi')
    if ($WholeCoordinator) {
        $names+=@('pairing-key.dpapi','journal.sqlite3','journal.sqlite3-wal','journal.sqlite3-shm','jobs','probes','logs','worker.log','worker.log.1','runtime.json')
    }
    $files=$names | Where-Object {Test-Path -LiteralPath (Join-Path $Root $_)}
    if (-not $files) { return $null }
    $archive=Join-Path (Join-Path $Root 'pairing-backups') ([datetime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $archive -Force | Out-Null
    $moved=New-Object System.Collections.Generic.List[string]
    try {
        foreach ($name in $files) {
            Move-Item -LiteralPath (Join-Path $Root $name) -Destination (Join-Path $archive $name)
            $moved.Add($name)
        }
        if ($WholeCoordinator -and (Test-Path -LiteralPath (Join-Path $Root 'worker.json'))) {
            Copy-Item -LiteralPath (Join-Path $Root 'worker.json') -Destination (Join-Path $archive 'worker.json')
        }
    } catch {
        foreach ($name in $moved) {
            if (-not (Test-Path -LiteralPath (Join-Path $Root $name))) {
                Move-Item -LiteralPath (Join-Path $archive $name) -Destination (Join-Path $Root $name)
            }
        }
        throw 'Could not archive the old pairing. Existing files were retained; setup is incomplete.'
    }
    return $archive
}
