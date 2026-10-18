$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018'
$env:TEMP=Join-Path $stage 'tmp';New-Item -ItemType Directory $env:TEMP -Force | Out-Null;$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $stage 'module-cache'
if((Get-FileHash (Join-Path $stage 'package.tar')).Hash -ine '31ff97c98796feb6c71d76b6c5733ca0af016b60612b7c2c6ad9b09eda2b0bd9'){throw 'Package archive SHA256 mismatch'}
& tar.exe -xf (Join-Path $stage 'package.tar') -C $stage
$code=$LASTEXITCODE;Write-Output "EXTRACT_PACKAGE_EXIT=$code";if($code){exit $code}
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $stage 'scripts\update-worker.ps1') -Package $stage -PackageManifestSha256 '820ce6b4d55ab2bc15485aa81fc39bf9eb5738a6192078da6057c5c45263cf45'
$code=$LASTEXITCODE;Write-Output "INSTALL_EXIT=$code";exit $code
