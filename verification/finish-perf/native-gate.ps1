Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\perf-land-0771b79'
$source=Join-Path $root 'source';$build=Join-Path $root 'build'
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $root 'module-cache';$env:PYTHONDONTWRITEBYTECODE='1';$env:VSCMD_SKIP_SENDTELEMETRY='1'
New-Item -ItemType Directory -Path $source,$env:TEMP | Out-Null
& tar.exe -xf (Join-Path $root 'source.tar') -C $source
$code=$LASTEXITCODE;Write-Output "EXTRACT_EXIT=$code";if($code){exit $code}
Write-Output ('SOURCE_ARCHIVE_SHA256='+(Get-FileHash (Join-Path $root 'source.tar') -Algorithm SHA256).Hash)
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$boost=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\dependencies\boost_1_89_0'
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON
$code=$LASTEXITCODE;Write-Output "CONFIGURE_EXIT=$code";if($code){exit $code}
& $cmake --build $build --config Release --parallel 2
$code=$LASTEXITCODE;Write-Output "BUILD_EXIT=$code";if($code){exit $code}
$inventory=(& $ctest --test-dir $build -C Release --show-only=json-v1) -join "`n"
$code=$LASTEXITCODE;Write-Output "DISCOVERY_EXIT=$code";if($code){exit $code}
$suites=($inventory | ConvertFrom-Json).tests
foreach($required in @('worker_contracts','pipeline_contracts','rendition_contracts','adaptive_hardware_contracts','progress_contracts','windows_contracts','setup_contracts','setup_progress_contracts','setup_converter_contracts')){
 if($required -cnotin @($suites.name)){throw ('Required native suite missing: '+$required)}
}
foreach($suite in $suites){
 $name=$suite.name
 & $ctest --test-dir $build -C Release -R ('^'+[regex]::Escape($name)+'$') -V --output-junit (Join-Path $root ($name+'.xml'))
 $code=$LASTEXITCODE;Write-Output ('CTEST_SUITE_EXIT '+$name+'='+$code);if($code){exit $code}
}
Write-Output 'WINDOWS_GATE_EXIT=0'
$package=Join-Path $root 'package';New-Item -ItemType Directory -Path $package | Out-Null
foreach($name in @('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','ow_gpu_measurements.exe')){Copy-Item (Join-Path $build ('Release\'+$name)) $package}
Get-ChildItem $package -File | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json -Compress
exit 0
