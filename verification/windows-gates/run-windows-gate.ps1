param([Parameter(Mandatory=$true)][string]$Expected)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $env:USERPROFILE 'worker-gates-20261017'
$source=Join-Path $work 'source'
$build=Join-Path $work 'build'
$git=Join-Path $env:LOCALAPPDATA 'HermesWorkerTools\MinGit-2.56.0.2\cmd\git.exe'
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
# Discard only the known diagnostic edits in this task's isolated build source.
& $git -C $source restore scripts/setup-converter.ps1 tests/windows_test.cpp tests/setup_converter_test.ps1
if($LASTEXITCODE){exit $LASTEXITCODE}
$diagnostic=Join-Path $source 'tests/converter-diagnostic.ps1'
if(Test-Path -LiteralPath $diagnostic){Remove-Item -LiteralPath $diagnostic}
& $git -C $source fetch (Join-Path $env:USERPROFILE 'worker-gates-final.bundle') HEAD
if($LASTEXITCODE){exit $LASTEXITCODE}
& $git -C $source checkout -B fix/worker-next FETCH_HEAD
if($LASTEXITCODE){exit $LASTEXITCODE}
$actual=(& $git -C $source rev-parse HEAD).Trim()
if($actual -cne $Expected){throw ('Unexpected source revision '+$actual)}
if(@(& $git -C $source status --porcelain).Count){throw 'Build source is dirty'}
Write-Output ('SOURCE_COMMIT='+$actual)
& $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$env:LOCALAPPDATA\OrganizerWorker\dependencies\boost_1_89_0" -DBUILD_TESTING=ON
$code=$LASTEXITCODE;Write-Output "CONFIGURE_EXIT=$code";if($code){exit $code}
& $cmake --build $build --config Release --parallel 2
$code=$LASTEXITCODE;Write-Output "BUILD_EXIT=$code";if($code){exit $code}
$inventory=(& $ctest --test-dir $build -C Release --show-only=json-v1) -join "`n"
$code=$LASTEXITCODE;Write-Output "CTEST_DISCOVERY_EXIT=$code";if($code){exit $code}
$suites=($inventory | ConvertFrom-Json).tests
foreach($required in @('windows_contracts','setup_contracts','setup_progress_contracts','setup_converter_contracts')){
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
