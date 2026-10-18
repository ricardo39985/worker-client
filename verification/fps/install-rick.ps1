$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018'
$env:TEMP=Join-Path $stage 'tmp';New-Item -ItemType Directory $env:TEMP -Force | Out-Null;$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $stage 'module-cache'
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $stage 'source\scripts\update-worker.ps1') -Package (Join-Path $stage 'package') -PackageManifestSha256 '820ce6b4d55ab2bc15485aa81fc39bf9eb5738a6192078da6057c5c45263cf45'
$code=$LASTEXITCODE;Write-Output "INSTALL_EXIT=$code";exit $code
