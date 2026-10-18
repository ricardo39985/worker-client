$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018';$ff='C:\Users\danny\ffmpeg.exe';$probe=Join-Path $root 'ow_rendition_probe.exe'
if((Get-FileHash $ff).Hash -ne 'f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4'){throw 'FFmpeg pin differs'}
if((Get-Content (Join-Path $env:LOCALAPPDATA 'OrganizerWorker\worker-status.txt') -Raw) -notmatch 'jobs\s+running 0'){throw 'Live worker busy'}
function Run($name,$a,$log){$watch=[Diagnostics.Stopwatch]::StartNew();$ErrorActionPreference='Continue'; & $ff @a 2>&1 | ForEach-Object {$_.ToString()} | Set-Content -Encoding UTF8 $log;$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Write-Output ($name+' EXIT='+$code+' MS='+$watch.ElapsedMilliseconds);if($code){throw $name}}
foreach($name in @('integer','fractional','vfr','rotated','rejected','rejected2','real3')){foreach($mode in @('nvenc','nvenc-software')){
if($mode -eq 'nvenc-software' -and $name -in @('vfr','rejected','real3')){continue}
$work=Join-Path $root ('after-'+$name+'-'+$mode);if(Test-Path $work){throw 'Refusing measurement overwrite'};New-Item -ItemType Directory $work | Out-Null
Copy-Item (Join-Path $root ($name+'.mp4')) (Join-Path $work 'source.bin')
Run ($name+' input-timing') @('-nostdin','-hide_banner','-v','info','-i',(Join-Path $work 'source.bin'),'-map','0:v:0','-an','-c:v','copy','-f','framehash','-hash','adler32','-') (Join-Path $work 'timing.log')
$plan=& $probe $mode encode $work;$code=$LASTEXITCODE;if($code){throw ('Command factory failed '+$code)}
foreach($line in $plan){$parts=$line -split "`t";if($parts[0] -eq 'thumbnail'){continue};$arguments=$parts[3..($parts.Count-1)];Run ($name+' '+$mode+' '+$parts[0]) $arguments (Join-Path $work ($parts[0]+'.log'))}
}}
