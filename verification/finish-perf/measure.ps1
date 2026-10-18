Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-perf-finish-20261018'
$env:TEMP=$root;$env:TMP=$root;$env:PSModuleAnalysisCachePath=Join-Path $root 'module-cache'
$exe=Join-Path $root 'ow_gpu_measurements.exe';$ffmpeg=Join-Path $env:USERPROFILE 'ffmpeg.exe'
$work=Join-Path $root 'measurements';New-Item -ItemType Directory -Path $work | Out-Null
foreach($mode in @('generate','baseline','baseline-normal','fused-normal')){
 $repeats=if($mode -like '*normal'){3}else{1}
 & $exe $ffmpeg $work $mode $repeats
 $code=$LASTEXITCODE;Write-Output "MEASUREMENT_EXIT=$mode/$code";if($code){exit $code}
}
foreach($size in @('1080p','4k')){
 foreach($file in @('feed.mp4','optimized.mp4')){
  $a=Join-Path $work "$size\baseline-normal-0\$file";$b=Join-Path $work "$size\fused-normal-0\$file"
  if((Get-FileHash $a).Hash -cne (Get-FileHash $b).Hash){throw 'Output bytes differ'}
  Write-Output "QUALITY size=$size file=$file"
  $metricLog=Join-Path $root ("quality-$size-$file.log")
  $args='-nostdin -hide_banner -v info -nostats -threads 4 -i "'+$a+'" -threads 4 -i "'+$b+'" -filter_complex_threads 4 -lavfi "[0:v]split=2[a][b];[1:v]split=2[c][d];[a][c]ssim[s];[b][d]psnr[p]" -map "[s]" -map "[p]" -an -f null -'
  $p=Start-Process -FilePath $ffmpeg -ArgumentList $args -Wait -PassThru -NoNewWindow -RedirectStandardError $metricLog
  Get-Content $metricLog | Select-String 'SSIM|PSNR'
  $LASTEXITCODE=$p.ExitCode
  $code=$LASTEXITCODE;Write-Output "QUALITY_EXIT=$size/$file/$code";if($code){exit $code}
 }
}
Write-Output 'PERFORMANCE_GATE_EXIT=0'
