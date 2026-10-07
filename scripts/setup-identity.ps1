function Read-WorkerProtectedRecord([string]$Root,[string]$Slot) {
    $name=if ($Slot -eq 'credential') {'credential.dpapi'} elseif ($Slot -eq 'pending') {'pending-pairing.dpapi'} else {throw 'Unknown identity slot.'}
    $path=Join-Path $Root $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { return $null }
    if ((Get-Item -LiteralPath $path).Length -gt 262144) { throw 'Protected identity is oversized.' }
    Add-Type -AssemblyName System.Security
    $plain=$null
    try {
        $plain=[Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes($path),$null,[Security.Cryptography.DataProtectionScope]::CurrentUser)
        return ([Text.Encoding]::UTF8.GetString($plain) | ConvertFrom-Json)
    } finally { if ($plain) { [Array]::Clear($plain,0,$plain.Length) } }
}
