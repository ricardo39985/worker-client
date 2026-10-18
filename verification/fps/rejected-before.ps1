$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-fps-20261018';$ff='C:\Users\danny\ffmpeg.exe'
$inputFile=Join-Path $root 'rejected.mp4'
foreach($mode in @('software','cuda','cuda-demux','software-demux')){
$a=@('-nostdin','-hide_banner','-v','verbose','-y','-threads','1','-filter_threads','1','-noautorotate')
if($mode.StartsWith('cuda')){$a+=@('-hwaccel','cuda','-hwaccel_device','1','-hwaccel_output_format','cuda')}
$a+=@('-i',$inputFile,'-map','0:v:0','-an','-c:v','h264_nvenc','-gpu','1','-preset','p4','-rc','vbr','-cq','24','-b:v','0','-rc-lookahead','0','-pix_fmt',$(if($mode.StartsWith('cuda')){'cuda'}else{'yuv420p'}),'-threads','1','-fps_mode','passthrough')
if($mode.EndsWith('demux')){$a+=@('-enc_time_base:v','demux')}
$a+=@('-movflags','+faststart',(Join-Path $root ('rejected-'+$mode+'.mp4')))
$watch=[Diagnostics.Stopwatch]::StartNew();$ErrorActionPreference='Continue'; & $ff @a 2>&1 | Out-File (Join-Path $root ('rejected-'+$mode+'.log'));$code=$LASTEXITCODE;$ErrorActionPreference='Stop';Write-Output ($mode+' EXIT='+$code+' MS='+$watch.ElapsedMilliseconds);if($code){throw $mode}
}
