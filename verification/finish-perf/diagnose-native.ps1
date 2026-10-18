$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\perf-land-0771b79'
$ctest='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
& $ctest --test-dir (Join-Path $root 'build') -C Release -N
Write-Output "DIAGNOSTIC_DISCOVERY_EXIT=$LASTEXITCODE"
Select-String -Path (Join-Path $root 'build\CMakeCache.txt') -Pattern 'Python.*EXECUTABLE|Python.*FOUND'
Get-Command python.exe,python3.exe -ErrorAction SilentlyContinue | Select-Object Name,Source | ConvertTo-Json -Compress
