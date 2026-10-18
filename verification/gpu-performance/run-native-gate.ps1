param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$expected=Join-Path $env:USERPROFILE 'ow-gpu-stage1-20261017'
if($source -cne $expected){throw ('Gate must use new isolated source '+$expected)}
$build=Join-Path $source 'build-native'
if(Test-Path -LiteralPath $build){throw 'Refusing to reuse a native gate build'}
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$boost=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\dependencies\boost_1_89_0'
if(-not (Test-Path -LiteralPath (Join-Path $boost 'boost\json.hpp'))){throw 'Reviewed installed Boost missing; no installation permitted'}
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON
$code=$LASTEXITCODE;Write-Output "CONFIGURE_EXIT=$code";if($code){exit $code}
& $cmake --build $build --config Release --parallel 2
$code=$LASTEXITCODE;Write-Output "BUILD_EXIT=$code";if($code){exit $code}
& $ctest --test-dir $build -C Release -V --output-junit (Join-Path $source 'native-gate.xml')
$code=$LASTEXITCODE;Write-Output "CTEST_EXIT=$code"
# This command does not run ow_gpu_baseline, Doctor or any FFmpeg conversion.
exit $code
