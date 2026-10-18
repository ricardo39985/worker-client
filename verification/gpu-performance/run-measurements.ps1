$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'
$root='C:\Users\danny\AppData\Local\Temp\ow-gpu-measured-20261018-a72c'
$env:TEMP=$root; $env:TMP=$root; $env:PSModuleAnalysisCachePath="$root\module-cache"; $env:DOTNET_CLI_HOME=$root
$env:VSCMD_SKIP_SENDTELEMETRY='1'; $env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
# Source already staged; no machine files outside this workspace.
$cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$boost='C:\Users\danny\AppData\Local\OrganizerWorker\dependencies\boost_1_89_0'
& $cmake -S "$root\source" -B "$root\build" -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON -DOW_ENABLE_IPO=OFF
Write-Output "CONFIGURE_EXIT=$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
& $cmake --build "$root\build" --config Release --parallel 2
Write-Output "BUILD_EXIT=$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
[IO.Directory]::CreateDirectory("$root\measurements") | Out-Null
foreach($mode in @('generate','baseline','device','shared','quality')) {
 & "$root\build\Release\ow_gpu_measurements.exe" 'C:\Users\danny\ffmpeg.exe' "$root\measurements" $mode 3
 Write-Output "MODE_EXIT=$mode/$LASTEXITCODE"; if($LASTEXITCODE){exit $LASTEXITCODE}
}
