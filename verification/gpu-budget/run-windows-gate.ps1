param([Parameter(Mandatory=$true)][string]$Expected)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $env:LOCALAPPDATA 'Temp\ow-gpu-budget-2a209bc'
$source=Join-Path $work 'source'
$build=Join-Path $work 'build'
$git=Join-Path $env:LOCALAPPDATA 'HermesWorkerTools\MinGit-2.56.0.2\cmd\git.exe'
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$boost=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\dependencies\boost_1_89_0'
foreach($required in @($git,$cmake,$ctest,(Join-Path $boost 'boost\json.hpp'))){
 if(-not (Test-Path -LiteralPath $required)){throw ('Missing existing prerequisite '+$required)}
}

if(Test-Path -LiteralPath $source){throw 'Measurement source must be new'}
New-Item -ItemType Directory -Path $source | Out-Null
& tar.exe -xf (Join-Path $work 'source.tar') -C $source
$code=$LASTEXITCODE;Write-Output "EXTRACT_EXIT=$code";if($code){exit $code}
$actual=$Expected
Write-Output ('SOURCE_COMMIT='+$actual)
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON
$code=$LASTEXITCODE;Write-Output "CONFIGURE_EXIT=$code";if($code){exit $code}
& $cmake --build $build --config Release --parallel 2
$code=$LASTEXITCODE;Write-Output "BUILD_EXIT=$code";if($code){exit $code}
$inventory=(& $ctest --test-dir $build -C Release --show-only=json-v1) -join "`n"
$code=$LASTEXITCODE;Write-Output "CTEST_DISCOVERY_EXIT=$code";if($code){exit $code}
$inventory | Set-Content -LiteralPath (Join-Path $work 'ctest-discovery.json') -Encoding UTF8
$suites=($inventory | ConvertFrom-Json).tests
foreach($required in @('worker_contracts','pipeline_contracts','rendition_contracts','adaptive_hardware_contracts','progress_contracts','windows_contracts','setup_contracts','setup_progress_contracts','setup_converter_contracts')){
 if($required -cnotin @($suites.name)){throw ('Native gate missing '+$required)}
}
$gateCode=0;$passed=0;$failed=0
foreach($suite in $suites){
 $name=$suite.name
 & $ctest --test-dir $build -C Release -R ('^'+[regex]::Escape($name)+'$') -V --output-junit (Join-Path $work ($name+'.xml'))
 $code=$LASTEXITCODE
 Write-Output ('CTEST_SUITE_EXIT '+$name+'='+$code)
 if($code){$failed++;if(-not $gateCode){$gateCode=$code}}else{$passed++}
}
Write-Output ('WINDOWS_GATE suites_passed='+$passed+' suites_failed='+$failed+' exit='+$gateCode+' commit='+$actual)
exit $gateCode
