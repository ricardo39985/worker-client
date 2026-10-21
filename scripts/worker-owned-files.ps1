function Install-WorkerOwnedScript([string]$Source,[string]$Destination) {
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}
