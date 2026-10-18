$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
$package=Join-Path $stage 'package'
$files=[ordered]@{}
foreach($name in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe'){$files[$name]=(Get-FileHash (Join-Path $package $name)).Hash.ToLowerInvariant()}
$info=[ordered]@{schema=1;revision='369eb1f65d3ef019c7ac5698e718a31640c81014';files=$files}
[IO.File]::WriteAllText((Join-Path $package 'package.json'),($info|ConvertTo-Json -Depth 8))
$manifestHash=(Get-FileHash (Join-Path $package 'package.json')).Hash.ToLowerInvariant()
& tar.exe -cf (Join-Path $stage 'package.tar') -C $package OrganizerWorker.exe OrganizerWorkerConsole.exe OrganizerWorkerDoctor.exe package.json -C (Join-Path $stage 'source') scripts/update-worker.ps1 scripts/worker-swap.ps1 scripts/setup-policy.ps1 scripts/setup-progress.ps1
$code=$LASTEXITCODE;Write-Output "PACKAGE_TAR_EXIT=$code";if($code){exit $code}
Write-Output ('PACKAGE_MANIFEST_SHA256='+$manifestHash)
Write-Output ('PACKAGE_TAR_SHA256='+(Get-FileHash (Join-Path $stage 'package.tar')).Hash.ToLowerInvariant())
Write-Output ('PACKAGE_INFO='+($info|ConvertTo-Json -Depth 8 -Compress))
exit 0
