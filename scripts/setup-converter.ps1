# Retained executable policy. Dot-sourcing has no installation or network effects.
function Assert-WorkerConverterUrl([string]$Value) {
    $uri=$null
    if([string]::IsNullOrWhiteSpace($Value) -or $Value -match '[\x00-\x20\\]' -or
       -not [uri]::TryCreate($Value,[UriKind]::Absolute,[ref]$uri) -or
       $uri.Scheme -ne 'https' -or -not $uri.Host -or $uri.Port -ne 443 -or
       $uri.UserInfo -or $uri.Query -or $uri.Fragment){
        throw 'Invalid FFmpeg lock URL: permanent HTTPS/443 without credentials, query or fragment required.'
    }
    return $uri
}
function Assert-WorkerConverterLock($Dependency) {
    foreach($name in @('version','format','distribution','sha256','bytes','url','license','license_url','redistribution_url')){
        if($null -eq $Dependency.PSObject.Properties[$name]){throw 'Incomplete FFmpeg lock; see docs/ffmpeg-distribution.md.'}
    }
    if($Dependency.format -cne 'exe' -or $Dependency.distribution -cne 'operator-retained' -or
       $Dependency.sha256 -cnotmatch '\A[a-f0-9]{64}\z' -or
       -not ($Dependency.bytes -is [int] -or $Dependency.bytes -is [long]) -or
       $Dependency.bytes -le 0 -or $Dependency.bytes -gt 2147483648 -or
       [string]::IsNullOrWhiteSpace($Dependency.version) -or [string]::IsNullOrWhiteSpace($Dependency.license)){
        throw 'Invalid FFmpeg lock integrity, format or license metadata.'
    }
    Assert-WorkerConverterUrl $Dependency.license_url | Out-Null
    if($null -eq $Dependency.url -and $null -eq $Dependency.redistribution_url){
        throw 'FFmpeg publication pending: the operator must publish the retained binary and matching source/notices, then update the reviewed lock. See docs/ffmpeg-distribution.md. Use -NoConversion only for explicit pairing-only setup.'
    }
    $uri=Assert-WorkerConverterUrl $Dependency.url
    Assert-WorkerConverterUrl $Dependency.redistribution_url | Out-Null
    $path=[uri]::UnescapeDataString($uri.AbsolutePath).ToLowerInvariant()
    $segments=$path.Split('/')
    if($Dependency.sha256 -cnotin $segments -or -not $path.EndsWith('/ffmpeg.exe') -or
       $path.Contains('autobuild-') -or 'latest' -in $segments -or
       ($uri.Host -eq 'github.com' -and $path.StartsWith('/btbn/'))){
        throw 'Invalid FFmpeg lock: require a retained URL containing the full checksum directory, ending in /ffmpeg.exe; upstream autobuild/latest is forbidden.'
    }
}
function Install-WorkerLockedConverter {
    param($Dependency,[string]$Root,[string]$StoredPath,[string]$StoredSha256,
          [switch]$NoConversion,[switch]$Force,[switch]$AcceptLicense,[switch]$NonInteractive,
          [scriptblock]$Consent,[scriptblock]$Download)
    if($NoConversion){return [pscustomobject]@{path='';sha256=''}}
    # Preserve an operator-approved existing converter, including explicit overrides.
    # Publication of a new distribution must not strand a healthy installed worker.
    if(-not $Force -and $StoredSha256 -match '\A[a-fA-F0-9]{64}\z' -and
       [IO.Path]::GetExtension($StoredPath) -ieq '.exe' -and (Test-Path -LiteralPath $StoredPath -PathType Leaf)){
        if((Get-WorkerFileSha256 $StoredPath) -ieq $StoredSha256){
            return [pscustomobject]@{path=$StoredPath;sha256=$StoredSha256.ToLowerInvariant()}
        }
    }
    Assert-WorkerConverterLock $Dependency
    $receipt=Join-Path $Root ('ffmpeg-license-'+$Dependency.sha256+'.json')
    $accepted=$false
    if(Test-Path -LiteralPath $receipt -PathType Leaf){
        try{$record=Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json;$accepted=$record.artifact_sha256 -ceq $Dependency.sha256}catch{$accepted=$false}
    }
    if(-not $AcceptLicense -and -not $accepted){
        if($NonInteractive){throw 'Conversion requires explicit -AcceptConversionLicense after license review, or -NoConversion.'}
        if(-not $Consent -or -not (& $Consent $Dependency)){throw 'Conversion installation cancelled. Use -NoConversion for explicit pairing-only setup.'}
    }
    $directory=Join-Path (Join-Path $Root 'dependencies') ('ffmpeg-'+$Dependency.sha256)
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $exe=Join-Path $directory 'ffmpeg.exe'
    # Get-WorkerVerifiedFile also has a $Download parameter. PowerShell resolves
    # callback variables dynamically, so retain the caller's transport under a
    # distinct name instead of recursively invoking that helper's wrapper.
    $converterTransport=$Download
    for($attempt=1;$attempt -le 3;$attempt++){
        try {
            Get-WorkerVerifiedFile -Destination $exe -Sha256 $Dependency.sha256 -Download {
                param($partial)
                if(-not $converterTransport){throw 'FFmpeg download transport is unavailable.'}
                & $converterTransport $Dependency.url $partial | Out-Null
                if(-not (Test-Path -LiteralPath $partial -PathType Leaf) -or (Get-Item -LiteralPath $partial).Length -ne $Dependency.bytes){
                    throw 'FFmpeg download byte count does not match the lock.'
                }
            }
            break
        } catch {
            if($attempt -eq 3){throw}
            Write-WorkerSetupLine '[REPAIR] Retrying the locked FFmpeg after a failed transfer or checksum check.'
            Start-Sleep -Seconds ($attempt*2)
        }
    }
    # Keep the configured pin from the reviewed lock, never derive approval from
    # whatever file happened to download. Get-WorkerVerifiedFile hashes before move.
    Write-WorkerJson $receipt @{artifact_sha256=$Dependency.sha256;accepted_utc=[DateTime]::UtcNow.ToString('o')}
    return [pscustomobject]@{path=$exe;sha256=$Dependency.sha256}
}
