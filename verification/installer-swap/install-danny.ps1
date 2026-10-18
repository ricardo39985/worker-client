$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
$env:TEMP=Join-Path $stage 'tmp';$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $stage 'module-cache'
if((Get-FileHash (Join-Path $stage 'package.tar')).Hash -ine '9ed9fc2dd44f7b005dfba07bfb03e11e83cae7ba7ddcd3b879dd568c28606aab'){throw 'Package archive SHA256 mismatch'}
& tar.exe -xf (Join-Path $stage 'package.tar') -C $stage
$code=$LASTEXITCODE;Write-Output "EXTRACT_PACKAGE_EXIT=$code";if($code){exit $code}
& powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $stage 'scripts\update-worker.ps1') -Package $stage -PackageManifestSha256 '9a075d7c6fa550202fcce23c096dcfc4d8481b6b296508e86b8d7e2b9263c811'
$code=$LASTEXITCODE;Write-Output "INSTALL_EXIT=$code";exit $code
