$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\Users\danny\AppData\Local\Temp\ow-gpu-measured-20261018-a72c'
$env:TEMP=$root; $env:TMP=$root; $env:PSModuleAnalysisCachePath="$root\module-cache"; $env:DOTNET_CLI_HOME=$root
$env:VSCMD_SKIP_SENDTELEMETRY='1'; $env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
Write-Output ('SOURCE_ARCHIVE_SHA256='+(Get-FileHash -LiteralPath "$root\final-source.zip" -Algorithm SHA256).Hash)
Expand-Archive -LiteralPath "$root\final-source.zip" -DestinationPath "$root\source-final" -Force
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
$boost='C:\Users\danny\AppData\Local\OrganizerWorker\dependencies\boost_1_89_0'
Select-String -LiteralPath "$boost\boost\version.hpp" -Pattern '^#define BOOST_LIB_VERSION'
& $cmake -S "$root\source-final" -B "$root\build-final" -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON -DOW_ENABLE_IPO=OFF
Write-Output "CONFIGURE_EXIT=$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
& $cmake --build "$root\build-final" --config Release --parallel 2
Write-Output "BUILD_EXIT=$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
foreach($mode in @('quality','poster-reuse','negotiation','baseline-full','fused-full')) {
 & "$root\build-final\Release\ow_gpu_measurements.exe" 'C:\Users\danny\ffmpeg.exe' "$root\measurements" $mode 3
 Write-Output "MODE_EXIT=$mode/$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
}
& $ctest --test-dir "$root\build-final" -C Release -V --output-junit "$root\native-final.xml"
Write-Output "CTEST_EXIT=$LASTEXITCODE"; exit $LASTEXITCODE
