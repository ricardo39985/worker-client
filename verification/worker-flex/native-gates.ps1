# Proposed isolated native gate. DO NOT RUN on either PC without operator approval.
param([Parameter(Mandatory=$true)][string]$ArchiveSha256)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-worker-flex-20261018'
$installed=Join-Path $env:LOCALAPPDATA 'OrganizerWorker'
$archive=Join-Path $root 'source.tar.gz'
$source=Join-Path $root 'source'
$build=Join-Path $root 'build'
if(-not (Test-Path -LiteralPath $root -PathType Container)){throw 'Explicitly staged directory required'}
if(Test-Path -LiteralPath $source){throw 'Refusing to reuse an existing candidate source directory'}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -cne $ArchiveSha256.ToUpperInvariant()){throw 'Archive hash mismatch'}
if(@(Get-CimInstance Win32_Process -Filter "Name='ffmpeg.exe'").Count){throw 'FFmpeg already running; defer this gate'}
$protected=@('current.json','worker.json','credential.dpapi','pairing-key.dpapi')
$before=@{}
foreach($name in $protected){$before[$name]=(Get-FileHash -LiteralPath (Join-Path $installed $name) -Algorithm SHA256).Hash}
$env:TEMP=Join-Path $root 'tmp'
$env:TMP=$env:TEMP
$env:PSModuleAnalysisCachePath=Join-Path $root 'module-cache'
$env:PYTHONDONTWRITEBYTECODE='1'
$env:VSCMD_SKIP_SENDTELEMETRY='1'
New-Item -ItemType Directory -Path $source,$env:TEMP | Out-Null
Start-Transcript -LiteralPath (Join-Path $root 'native-gate.log') | Out-Null
try {
 & tar.exe -xf $archive -C $source
 if($LASTEXITCODE){throw 'Source extraction failed'}
 $cmake='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
 $ctest=Join-Path (Split-Path $cmake) 'ctest.exe'
 $boost=Join-Path $installed 'dependencies\boost_1_89_0'
 if(-not (Test-Path -LiteralPath (Join-Path $boost 'boost\json\src.hpp'))){throw 'Previously installed reviewed Boost root missing; do not install'}
 & $cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DOW_BOOST_ROOT=$boost" -DBUILD_TESTING=ON
 if($LASTEXITCODE){throw 'MSVC configure failed'}
 & $cmake --build $build --config Release --parallel 1
 if($LASTEXITCODE){throw 'MSVC build failed'}
 $inventory=(& $ctest --test-dir $build -C Release --show-only=json-v1) -join "`n"
 if($LASTEXITCODE){throw 'CTest discovery failed'}
 $suites=($inventory | ConvertFrom-Json).tests
 foreach($required in @('worker_contracts','pipeline_contracts','rendition_contracts','adaptive_hardware_contracts','progress_contracts','windows_contracts','setup_contracts','setup_progress_contracts','setup_converter_contracts','setup_swap_contracts')){
  if($required -cnotin @($suites.name)){throw ('Required native suite missing: '+$required)}
 }
 foreach($suite in $suites){
  & $ctest --test-dir $build -C Release -R ('^'+[regex]::Escape($suite.name)+'$') -V --output-junit (Join-Path $root ($suite.name+'.xml'))
  $code=$LASTEXITCODE
  Write-Output ('CTEST_EXIT '+$suite.name+'='+$code)
  if($code){throw ('Native suite failed: '+$suite.name)}
 }
 Write-Output 'NATIVE_GATE_EXIT=0'
} finally {
 foreach($name in $protected){
  if((Get-FileHash -LiteralPath (Join-Path $installed $name) -Algorithm SHA256).Hash -cne $before[$name]){Write-Error ('Protected installed file changed: '+$name)}
 }
 Stop-Transcript | Out-Null
}
# This gate neither installs nor launches the tray/Doctor; actual media and
# sustainable-capacity measurements need their own exact commands and approval.
