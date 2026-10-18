$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$root=Join-Path $env:LOCALAPPDATA 'Temp\ow-perf-finish-20261018'
$owned=@(Get-Process ow_gpu_measurements -ErrorAction SilentlyContinue | Where-Object {$_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)})
foreach($p in $owned){Stop-Process -Id $p.Id -ErrorAction Stop;$p.WaitForExit(10000)|Out-Null;if(-not $p.HasExited){throw 'Owned measurement did not stop'}}
Write-Output ('STOPPED_MEASUREMENT_PROCESSES='+$owned.Count)
$ownedChildren=@(Get-CimInstance Win32_Process -Filter "name='ffmpeg.exe'" | Where-Object {$_.CommandLine -like ('*'+$root+'*')})
if($ownedChildren.Count){throw 'Owned converter is still draining; refusing deletion'}
if(Test-Path $root){[IO.Directory]::Delete($root,$true)}
Write-Output ('REMOVED_ROOT='+$root)
Write-Output ('ROOT_EXISTS='+[IO.Directory]::Exists($root))
Get-Content (Join-Path $env:LOCALAPPDATA 'OrganizerWorker\worker-status.txt')
if([IO.Directory]::Exists($root)){throw 'Measurement directory remains'}
exit 0
