$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018'
if(!(Test-Path $root)){throw 'Missing owned measurement directory'}
$ff='C:\Users\danny\ffmpeg.exe'
if((Get-FileHash $ff).Hash -ne 'f78373b58fa0a239ca062cbd9e03130285ec4e835e57abc22a5d4025e293d2b4'){throw 'pin mismatch'}
function Run($name,$arguments){$watch=[Diagnostics.Stopwatch]::StartNew(); $ErrorActionPreference='Continue'; & $ff @arguments 2>&1 | Out-File (Join-Path $root ($name+'.log'));$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Write-Output ($name+' EXIT='+$code+' MS='+$watch.ElapsedMilliseconds);if($code){throw $name}}
foreach($entry in @(@('integer','30'),@('fractional','30000/1001'),@('vfr','30'))){
$name=$entry[0];$a=@('-nostdin','-v','error','-y','-f','lavfi','-i',('testsrc2=size=320x240:rate='+$entry[1]),'-t','3','-an')
if($name -eq 'vfr'){$a+=@('-vf',"select='not(mod(n,2))+not(mod(n,5))'",'-fps_mode','passthrough')}
$a+=@('-c:v','libx264','-threads','2',(Join-Path $root ($name+'.mp4')));Run ('generate-'+$name) $a
}
Run 'rotate' @('-nostdin','-v','error','-y','-i',(Join-Path $root 'fractional.mp4'),'-c','copy','-metadata:s:v:0','rotate=90',(Join-Path $root 'rotated.mp4'))
foreach($name in @('integer','fractional','vfr','rotated')){foreach($decode in @('software','cuda')){foreach($codec in @('h264','hevc')){
$a=@('-nostdin','-hide_banner','-loglevel','verbose','-y','-threads','1','-filter_threads','1','-noautorotate')
if($decode -eq 'cuda'){$a+=@('-hwaccel','cuda','-hwaccel_device','1','-hwaccel_output_format','cuda')}
$a+=@('-i',(Join-Path $root ($name+'.mp4')),'-map','0:v:0','-an','-c:v',($codec+'_nvenc'),'-gpu','1','-preset','p4','-rc','vbr','-cq','24','-b:v','0','-rc-lookahead','0','-pix_fmt',$(if($decode -eq 'cuda'){'cuda'}else{'yuv420p'}),'-threads','1','-fps_mode','passthrough','-movflags','+faststart',(Join-Path $root ($name+'-'+$decode+'-'+$codec+'.mp4')))
Run ($name+'-'+$decode+'-'+$codec) $a
}}}
