$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
$env:TEMP=Join-Path $stage 'tmp';$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $stage 'module-cache'
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $stage 'source\scripts\update-worker.ps1') -Package (Join-Path $stage 'package') -PackageManifestSha256 '9a075d7c6fa550202fcce23c096dcfc4d8481b6b296508e86b8d7e2b9263c811'
$code=$LASTEXITCODE;Write-Output "INSTALL_EXIT=$code";exit $code
